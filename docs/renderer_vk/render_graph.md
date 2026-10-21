# Render Graphs

A render graph is a DAG of `IRenderTaskVK` nodes connected by resource dependency edges. The application does not build it
directly: it registers a rebuild callback that returns a complete `RendGraphDescVK`, usually produced by a Lua/Teal recipe.

## Registration and Rebuilds

```cpp
Ref<RenderGraphVK> graph = renderer->register_render_graph("scene",
    [this, renderer](const RendGraphContextVK& p_context) {
        RendGraphRecipeImportsVK imports;
        imports.add(REND_RES_SWAP_CHAIN, *renderer->acquire_swap_chain_image_view_container())
            .add("camera_info", *camera_info_ubo);
        const DataValue recipe = RendGraphScriptVK::run("render_graphs.scene",
            RendGraphScriptVK::make_context(*render_context, p_context));
        return RendGraphRecipeBuilderVK().build(recipe, imports, render_context);
    });
renderer->activate_render_graph("scene");
```

The callback runs for the first build (at registration), whenever the context changes (the swap-chain extent, checked when
the graph executes), and on `RenderGraphVK::rebuild()`. Keep one-time setup outside it: pipeline compiles,
application-owned buffers, and initial-data snapshots. Inside it, only assemble the descriptor. Import renderer-owned
containers such as the swap chain inside the callback, since the renderer replaces them when the number of frames in flight
changes. Unregistering defers destroying the graph until no frame in flight can use it.

A rebuild diffs the new descriptor against the current graph (`RendGraphResourceDiffVK`):

- A graph-owned resource is **kept**, with its content, when its id, kind, per-frame layout, and evaluated description match
  and, for a view, its image is kept. Imports are kept when they bind the same object.
- A changed description is **recreated** in place in its container, losing its content.
- A changed kind, per-frame layout, or import identity is **replaced** with a new container; re-acquire it with
  `acquire_resource()`.
- Every task is recreated from its factory, so task state that must survive a rebuild lives outside the task.

The diff relies on complete `operator==` on the description structs.

## Descriptors

`RendGraphDescVK` holds tasks, resources, and subgraphs.

- **Graph-owned resources** are declared with `add_buffer()`, `add_image()`, `add_image_view()`, and `add_sampler()`, each
  taking a factory of the context that returns `RendBufferDescVK`, `RendImageDescVK`, `RendImageViewDescVK`, or
  `RendSamplerDescVK`. The primary usage (`RendResPrimaryUsageVK`) selects Vulkan usage flags. Buffers take a memory domain,
  optional initial data, and `per_frame`. Per-frame resources live in an `ROCMultislotsVK` with one slot per frame in flight,
  and others in an `ROCSingleVK`.
- **Imports** are resources owned outside the graph: `add_resource(id, *bindable)` with any `IBindableRenderResourceVK`.
  Composites such as compiled meshes register their members as `<id>.<member>`. A `RenderResourceListVK` registers
  `<id>[<index>]` entries, and an access or edge on the list expands to every entry.
- **Tasks** are `RendTaskDescVK` entries holding a `task_id`, a factory
  (`Unique<IRenderTaskVK>(const RendGraphContextVK&)`), edges added with `connect()`, and aliases added with `alias()`.
  Descriptors are copyable; copies share the factory's captures.

Frame-local data is held in graph resources and updated in place, never rebound:

```cpp
Ref<ROCMultislotsVK> camera_buffers = graph->acquire_resource("camera_info").dynamic_cast_ref<ROCMultislotsVK>();
camera_buffers->publish_buffer_update(&camera_info, 0, sizeof(camera_info));
```

Each slot applies the newest bytes when it is next used. Publish on the rendering thread; other threads use
`IRenderer::add_job()`.

## Edges and Synchronization

Edges are the only ordering mechanism. `connect(next_task_id, resource_id)` on the earlier task adds one edge carrying one
resource. Both tasks must access the resource, and the edge's stages and accesses are derived from the `RendResUsageVK`
usages both report. Every read-write, write-read, and write-write conflict on a resource must be ordered by a chain of edges
on that same resource; read-read pairs do not conflict. Construction fails on cycles, self edges, unknown tasks, undeclared
resources, unordered conflicts, and graphics-only usages on compute tasks.

Tasks report every access themselves through `IRenderTaskVK::append_accesses()`: reflected shader resources, render targets,
and fixed-function buffers. Accesses never create edges. The only way to map task-local identifiers to graph resources is
`alias()`; the graph rejects aliases the task never uses.

Same-queue edges become pipeline barriers recorded before the later task: buffer barriers for buffers, whole-image barriers in
the unified `GENERAL` layout for image views, and global memory barriers otherwise. Cross-queue edges use one timeline
semaphore per producer task.

### Queues

`QueueFamilyVK::GRAPHICS` selects the graphics queue. `COMPUTE_1` and `COMPUTE_2` select two queues of the compute family,
which share one `VkQueue` when the family has too few queues. Compute tasks default to `GRAPHICS`; choose a compute queue to
overlap work. Every task on a non-graphics queue must reach a graphics task through edges, so the frame fence covers it.
Images and buffers default to concurrent sharing across queue families; the graph emits no queue-family ownership transfers.

Acquire waits attach to the first graphics submission. The last graphics submission transitions the swap-chain image for
presentation and signals the terminal semaphore and in-flight fence.

## Subgraphs

`add_subgraph(id, RendSubgraphDescVK, port bindings)` groups tasks, graph-owned resources, and internal edges under one id. It is
flattened when added: member tasks and owned resources become `<id>.<name>`, and names inside it resolve to a port's bound
resource, else the subgraph's own resource, else the enclosing graph's. Imports stay graph-wide, and importing the same object
again merges. Edges to and from the subgraph id are resolved at build time from the tasks' accesses. An incoming edge on R
reaches the members that access R with no edge on R from another member; an outgoing edge leaves the members that access R
with no edge on R to another member. Order a subgraph's accesses to its ports with internal edges.

## Recipes

A recipe is a Teal (or Lua) `script` resource whose module exports `build(context)` and returns a `RendGraphRecipeVK`
(`rend_graph_recipe_params_vk.hh`). `RendGraphScriptVK::run()` executes it in a fresh sandbox, and
`RendGraphRecipeBuilderVK::build()` turns the result into a descriptor. Recipes only build descriptors; they never touch render
objects.

```lua
local type wbe = require("wbe")
local Graph = require("render_graph_utils.render_graph_utils")
local exports = {}

function exports.build(_context: wbe.RendRecipeContextVK): wbe.RendGraphRecipeVK
    local graph = Graph.new()
    local target = graph:import_resource("swap_chain")
    local camera = graph:import_resource("camera_info")
    local static_mesh = graph:add_mesh("model_cube_static.mesh")
    local pipeline = graph:add_graphics_pipeline("draw_cube_unshaded_pipeline")
    local draw = graph:draw_mesh("run_static_mesh", {
        mesh = static_mesh,
        pipeline = pipeline,
        parts = { "opaque" },
    }, { uses = { camera } })
    draw:color_target(0, target)
    return graph:build()
end

return exports
```

### Task objects and aliases

Typed graph methods create and return task objects owned by that graph. For example:

```lua
local lighting_pipeline = graph:add_compute_pipeline("lighting_shader")
local lighting = graph:compute("lighting", {
    pipeline = lighting_pipeline,
    group_count_x = groups_x,
    group_count_y = groups_y,
}, { aliases = { albedo = albedo } })
local overlay = graph:import_task("draw_imgui")
```

The constructors are `:compute`, `:draw`, `:draw_mesh`, `:draw_indirect`, `:compute_indirect`, `:compute_sequential`,
`:generate_mipmaps`, `:cpu_acquire_image`, `:full_memory_barrier`, and `:ping_pong`, generated from the task types that
name them (see [Task types](#task-types)). Each takes `(id, params, options?)`;
`options` can supply task-local `aliases`, `uses`, `trigger`, and `toggle`. Their signatures type-check parameters against
the reflected C++ types or the generated handle-bearing parameter interfaces. Constructors copy parameter data and
option tables while preserving resource handle identity. `:import_task(id)` registers an externally supplied C++ task.

`graph:add(description)` remains available for custom task types and feature subgraphs. Save the returned objects and use them in
`graph:chain(resource, { producer, consumer })`, or connect directly with `producer:connect(consumer, resource)`.
Connections remain explicit and resource-specific. Both methods require actual task handles from the same graph;
matching string IDs or lookalike tables are insufficient. Subgraph tasks participate in connections the same way.

Connections and chains accept one resource handle, a non-empty list of handles, or a reusable resource bundle:

```lua
gbuffer:connect(lighting, { albedo, normal, depth })
graph:chain({ color, depth }, { first, second, third })
local targets = graph:resource_bundle({ albedo, normal, depth })
graph:chain(targets, { geometry, lighting })
geometry:connect(lighting, targets)
```

Each resource expands independently into the existing edges. A chain connects adjacent tasks in order; every adjacent
pair must access every listed resource. Resource lists preserve their input order.

`graph:resource_bundle(resources)` copies the list and retains the original resource handles in order. Later changes to
the input list do not affect the bundle. Bundles are flat collections for `:chain()`, `:connect()`, and `:connect_all()`;
they have no resource ID or serialized declaration. Their members are collected only when a task references them, so
creating an unused bundle adds no resources to the recipe.

Use `producer:connect_all(consumers, resources)` for fan-out. It connects the producer to each consumer, leaving the
consumers unordered relative to one another:

```lua
composition:connect_all({ extract_bloom, present, present_frame_sequence }, result_image)
```

`consumer:inputs_from(producer, bindings)` combines task-local aliases and producer-to-consumer edges:

```lua
lighting:inputs_from(gbuffer, {
    gbuffer_albedo = albedo,
    gbuffer_normal = normal,
    gbuffer_depth = depth,
})
```

Bindings are processed in sorted local-name order. Multiple names for the same resource handle create one edge per call;
distinct handles remain visible to resource validation. Existing alias names cannot be rebound. All endpoints must belong
to the same graph, and batches validate all arguments before changing aliases or edges. Resource lists and fan-out target
lists must be dense and non-empty; empty or single-task chains retain their existing no-op behavior. These methods return
the receiving task for fluent calls. The helpers retain explicit ordering and do not infer accesses from aliases or `uses`.

`task:alias(local_name, resource)` binds a task-local resource name and returns the task for fluent calls. Two tasks can
use the same local name for different resource handles. Repeating a local alias name within one task is an error.
Descriptions can still supply an `aliases` map; each task receives its own copy, so later alias calls cannot alter another
task's bindings. `task:connect()` also returns the task for chaining.

`task:color_target(index, resource)` and `task:depth_target(resource)` bind render attachments through those same
task-local aliases. Color indices start at zero and must be non-negative integers. They return the task and reject
duplicate bindings, including a target already bound through `:alias()`.

Task IDs remain stable names for C++ and diagnostics. `graph:get(id)` is available for explicit lookup, but dependency
chains use saved handles. `graph:build()` lowers connections and aliases to the unchanged C++ string-id recipe schema;
methods and task-to-graph references are never serialized.

### Resource handles

Graph resource methods return recipe handles. Reuse those objects in task aliases, edges, and subgraph port bindings:

```lua
local image = graph:add_image("gbuffer_albedo_image", "image_render_target", formats.gbuffer_albedo)
local albedo = image:view("gbuffer_albedo")
-- draw_gbuffer and lighting are task objects.
draw_gbuffer:color_target(0, albedo)
lighting:alias("gbuffer_albedo", albedo)
graph:chain(albedo, { draw_gbuffer, lighting })
```

`graph:build()` discovers graph-owned resources from task bindings and edges, including the image behind each used view.
There is no graph-level resource registration or authored `resources` list. Unused handles are omitted, and repeated uses of
the same handle emit one declaration. Different owned handles with the same id and cyclic handle dependencies fail.

`image:view()` returns a cached default view with the stable id `<image.id>.view`; repeated calls on the same image return
the same handle. Its type and format follow the image, and its range preserves the graph default of one mip and one layer.
Use `image:view(id, description?)` for explicit names or specialized mip/aspect/layer ranges. A customized view requires an
explicit id. `graph:add_image_view(id, image, description?)` also constructs a view. Views retain their
source image. Imports, references, and ports can also describe views of external images; C++ validates their actual kind.

Resource-bearing parameters of typed constructors accept handles directly:

```lua
graph:generate_mipmaps("shadow_mipmaps", {
    source_view = moments,
    mip_chain_view = sampled_moments,
    format = formats.shadow_moments,
})
```

These adapters retain handles and lower only the `WBE_RECIPE_RESOURCE` fields of the params to IDs. A task-local alias
cannot redirect an ID passed as a parameter handle to a different resource; constructors, alias calls, and builds validate
this contract.
Sequential dispatch handles use task-local
identity aliases under their resource IDs; an existing alias redirecting that ID to a different resource is rejected.
Existing string dispatch aliases remain local to the containing task.
Declare meshes and pipelines once with `graph:add_mesh(key)`, `graph:add_graphics_pipeline(key)`, and
`graph:add_compute_pipeline(key)`, then pass the returned handles to task parameters. These declarations identify
application-owned renderer database entries. Every constructor returns the same `Graph.Resource` type, including images,
buffers, meshes, and pipelines. Internal metadata distinguishes graph resources from database entries and identifies the
expected C++ mesh or pipeline class. The task adapter checks those kinds at runtime; mesh and pipeline handles cannot be
used as graph edges, shader bindings, or image attachments.

A handle is a copyable descriptor without ownership of the render object. The Teal task adapter snapshots it into the
string key the C++ recipe format already accepts. Unused declarations do not create graph resources, imports, aliases, or
edges; task factories continue to resolve and retain the actual render objects in C++.

Event names remain strings. Existing string graph-resource parameters are supported, but callers must retain those
resources through another binding, edge, or `uses` entry. Raw recipe tables also keep string mesh and pipeline keys.

A task's `uses = { handle, ... }` retains resources accessed by implicit same-name shader bindings, including read-only
samplers and single-task scratch buffers, or by a custom task's string parameters. This only controls resource discovery:
accesses are still reported by C++ tasks and ordering comes exclusively from explicit edges.

Use `graph:import_resource(id)` for an exact C++ supplied import, `graph:port(id)` for a subgraph-local port, and
`graph:reference(id)` for an
existing enclosing or composite-member resource. Imports and ports are collected automatically. A feature can pass its
complete C++ import list to `graph:build(imports)`; this preserves strict supplied-import matching even for resources an
imported C++ task binds internally. `reference` does not create a resource or declare another import.

`child_graph:as_subgraph(id, { color = parent_handle }, imports?)` builds the child recipe and returns a detached task
for `parent_graph:add(...)`. Its bound port names declare the child ports, including unused ones, and its bindings retain
the parent resources. Owned child resources stay scoped to the child. The final build emits the existing string-id recipe
schema; resource handles and graph methods never cross the Lua/C++ boundary.

The emitted recipe retains the existing C++ schema:

- `imports` lists every resource owned outside the graph. Each must be supplied through `RendGraphRecipeImportsVK`; a
  declared-but-missing or supplied-but-undeclared import is an error. Recipes cannot set initial data, so buffers that need it
  are application-owned imports registered in the render database under their import name.
- `resources` declare exactly one of `buffer`, `image`, `image_view`, or `sampler` each.
- `tasks` name a registered task type and pass it typed, strictly deserialized `params`. `aliases` map task-local ids, and
  `connect` entries (`{ to = <task id>, resource = <resource id> }`) add edges. Pipeline params are render-database keys of
  pipelines the application compiled.
- `trigger = { event, activate }` wraps the task with `RTTriggeredVK`, which records only on triggered or enabled frames.
  `toggle = { event, enabled }` wraps it with `RTToggleVK`, outside any trigger. Both are driven by commands emitted through
  `EventManager`. Rebuilds re-run the recipe, so pass a toggle's current state through the context. Work that runs once is a
  trigger activated while pending, which the application disables after the first recorded frame.
- Tasks that need application objects (for example the ImGui overlay) are built in C++, supplied with
  `RendGraphRecipeImportsVK::add_task()`, and listed as `{ id = <task id>, import = true }`.
- A task with a nested recipe in `subgraph` and bound `ports` (built with `child:as_subgraph()`) becomes a
  subgraph. A trigger or toggle on it wraps every member.

The callback may add to the descriptor the builder returns for parts not yet expressible in Lua. The deferred test adds its
acceleration structure and dynamic mesh entities this way.

### Context

`context` holds `RendRecipeContextVK` facts: `frames_in_flight`, `swap_chain_format`, and `swap_chain_extent`
(`RendGraphScriptVK::make_context()`). An application that adds entries declares a `WBE_REFLECT_SCRIPT` struct deriving from
`RendRecipeContextVK`, fills its base with `get_recipe_context()`, and passes `serialize_context()` to `run()`. Size
swap-chain-dependent resources and dispatches in Lua from `swap_chain_extent`.

### Feature modules

A reusable recipe part lives in `render_graph_utils/features/` and exports `subgraph(id, params, target)`, returning a subgraph
task. Its C++ side has a `configure_recipe()` that serializes the feature's `WBE_REFLECT_SCRIPT` parameter struct (including the
Lua `module` name) into the context and supplies the feature's resources and C++ tasks to the imports. The recipe calls
`require(params.module).subgraph(id, params, target)`. The deferred test's pixel-sort, SVO, and octree features follow this
pattern.

### Task types

Recipes refer to task types by name. A type is registered by annotating its class
`WBE_META(WBE_RENDER_TASK(<method>), WBE_SERIALIZATION_ALIASES(<name>))` and declaring a reflected static
`create(const RenderContextVK*, const <Params>&, const RendGraphContextVK&)`. The params struct is
`WBE_SERIALIZABLE_STATIC, WBE_STRICT_DESERIALIZATION, WBE_REFLECT_SCRIPT`. An optional static `prepare_graph()` lets the type add
imports and aliases when it is added (`RTGraphicsDrawMeshVK` uses it to import meshes). Reflection generates the
`RenderTaskRegistryVK` implementation, the shared `render_graph_utils.render_graph` declarations, and, into
`render_graph_utils.render_graph_tasks`, the installer of the `graph:<method>(id, params, options?)` constructor, which
creates the type `<name>`. A bare `WBE_RENDER_TASK` registers the type without a constructor.

Annotate a params field that holds resource ids `WBE_RECIPE_RESOURCE(graph)` for graph resource ids, or
`WBE_RECIPE_RESOURCE(task_local)` for ids the task's aliases map. The field holds a `std::string`, optional or in a vector,
or a `DataValue` map of ids. The constructor of params with such fields, directly or in nested params, takes an authoring
interface named after the C++ params that also accepts resource handles there. Use these interfaces directly from
the shared `render_graph_utils.render_graph` module; no task-specific aliases or resource type arguments are needed:

```lua
local type render_graph = require("render_graph_utils.render_graph")
local params: render_graph.RTParamsRunComputePipelineVK = {
    pipeline = lighting_pipeline,
    group_count_x = groups_x,
}
graph:compute("lighting", params)
```

Use `WBE_RECIPE_RESOURCE(database, RenderObjectType)` for render-database keys. `RenderObjectType` must be an existing
`WBE_RENDER_OBJECT` class. The generated authoring field accepts the common `Resource` handle and records the expected C++
class for runtime validation. The adapter lowers the handle's id to the existing string field, including in optional
values, vectors, maps, and nested params. No C++ object is exposed to Lua.

| Type | Constructor | Task | Purpose |
| --- | --- | --- | --- |
| `run_compute_pipeline` | `:compute` | `RTRunComputePipelineVK` | One compute dispatch |
| `run_compute_pipeline_indirect` | `:compute_indirect` | `RTRunComputePipelineIndirectVK` | Dispatch with counts read from a buffer |
| `run_compute_pipelines_sequential` | `:compute_sequential` | `RTRunComputePipelinesSequentialVK` | Ordered compute passes in one command buffer |
| `ping_pong` | `:ping_pong` | `RTPingPongVK` | Alternates two compute pipelines for N iterations |
| `max_downsample` | — | `RTMaxDownsampleVK` | Mip chain by 2x2 maximum |
| `run_graphics_pipeline` | `:draw` | `RTRunGraphicsPipelineVK` | One graphics pipeline draw |
| `run_graphics_pipeline_indirect` | `:draw_indirect` | `RTRunGraphicsPipelineIndirectVK` | Non-mesh indexed indirect draws |
| `graphics_draw_mesh` | `:draw_mesh` | `RTGraphicsDrawMeshVK` | Draws parts of a compiled mesh |
| `generate_mipmaps` | `:generate_mipmaps` | `RTGenerateMipmapsVK` | Color mip chain with filtered blits |
| `full_memory_barrier` | `:full_memory_barrier` | `RTFullMemoryBarrierVK` | Orders a frame's mutation of persistent state after the previous frame's consumers |
| `cpu_acquire_image` / `cpu_acquire_buffer` | `:cpu_acquire_image` / — | `RTCPUInFlightAcquireVK` / `RTCPUInFlightBufferAcquireVK` | Asynchronous CPU readback, delivered through `EventManager` |

Compute types take a `queue_family` param and a `push_constants` table.

### Scripts

Recipes and their modules are Teal sources under `res/assets/scripts/teal/` (see
[resource_standard.md](../resource/resource_standard.md#script-resources)). They import the generated declarations of the
`WBE_REFLECT_SCRIPT` types as types only (`local type wbe = require("wbe")`) and annotate `build(context: <context type>):
wbe.RendGraphRecipeVK`. Typed graph methods check their parameter tables directly. When using raw `graph:add()` for a
custom task, annotate its params with the reflected struct because `RendRecipeTaskVK.params` is untyped. Enums
serialize as their canonical names; compare `Graph.enum_name(value)` with the canonical upper-case name, which also
accepts the lowercase aliases.

`Graph.new()` creates a graph with resource constructors (`:add_buffer`, `:add_storage_buffer`, `:add_image`,
`:add_storage_image`, `:add_image_view`, `:add_clamp_to_edge_sampler`), typed mesh and pipeline declarations,
`:import_resource`, `:port`, `:reference`, `:indirect_dispatch`, `:as_subgraph`, and the task/build methods. `Graph.enum_name`
remains a static utility. `render_graph_utils.render_graph_utils` implements these operations and returns the shared module,
so its import also exposes `Graph.Resource`, `Graph.Task`, and the concrete `Graph.RTParams...` types. The generated
`render_graph_utils.render_graph` module contains declarations only and depends on the reflected `wbe` types. Generated task
installation depends on that common module; the common module does not import the implementation or installer. For
type-only use, import `render_graph_utils.render_graph` directly.
The older standalone constructors and `graph()` factory also remain compatibility APIs.
`render_graph_utils.sort.radix_sort_buffers(names, count, graph?)` creates the sort's resource handles;
`render_graph_utils.sort.add_radix_sort_task(graph, id, buffers, queue_family?)` registers its typed sequential compute task.
`render_graph_utils.sort.radix_sort_task` remains available for callers that need an unregistered task description.

## C++-only Graphs

Small tests may build the descriptor in C++ inside the callback; see
`tests/executable/function/render/vulkan_draw_triangle_test.cpp`:

```cpp
RendTaskDescVK task_desc(render_allocator);
task_desc.task_id = "run_pipeline";
task_desc.alias(RenderTargetsVK::color_id(0), REND_RES_SWAP_CHAIN);
task_desc.factory = [=](const RendGraphContextVK& p_context) {
    return RTRunGraphicsPipelineVK::Builder(render_allocator)
        .set_pipeline(graphics_pipeline)
        .set_render_extent(p_context.swap_chain_extent)
        .create_unique(render_context);
};
graph_desc.get_task("run_shadow").connect("run_pipeline", "shadow_moments");
graph_desc.add_task(std::move(task_desc));
```

## Debugging

- `RenderGraphVK::dump_graph_metadata()` describes tasks (queue, order, subgraph), edges (stages, accesses, cross-queue), and
  subgraphs.
- With timestamp queries enabled, `gpu_timestamp_report.json` includes that structure; `python export_render_graph.py` renders
  it as Mermaid or SVG.
- `write_descriptor_debug_output(name, graph->list_descriptors())` dumps the bound descriptors.

## Examples

- `tests/executable/function/render/vulkan_draw_cube_unshaded_test.cpp` with `render_graphs/draw_cube_unshaded.tl`: minimal
  recipe.
- `vulkan_pixel_sorting_test.cpp` with `pixel_sorting_recipe_context.hh` and `render_graphs/pixel_sorting.tl`: custom
  context and an imported ImGui task.
- `deferred_test/deferred_render_graph.cpp` with `render_graphs/deferred.tl` and `render_graph_utils/features/`: full deferred
  pipeline with feature subgraphs, triggers, toggles, and C++ additions.
