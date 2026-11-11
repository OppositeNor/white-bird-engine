# Render Graphs

A render graph is a DAG of `IRenderTaskVK` nodes connected by resource dependency edges. Applications compose complete
graphs only in Teal: a manifest **definition** names a recipe and its static parameters, and the application acquires an
**instance** of it with **bindings** that supply its runtime resources and native tasks.

## Definitions, Instances, and Rebuilds

A definition is a `render_graph` resource (`RURenderGraph`), declared in a manifest reachable from a root chunk; the
engine's are in `res/assets/render_graphs/render_graphs.toml`. It is immutable and holds no renderer state:

```toml
[[resources]]
id = "scene_graph"
type = "render_graph"
recipe = "render_graphs.scene"

[resources.parameters]
bloom_blur_pass_count = 8
```

The application acquires and activates an instance on the rendering thread:

```cpp
auto bindings = make_ref<RenderGraphBindingsFunctionVK>(global_allocator(), "scene_graph",
    [this](const RenderContextVK& /*p_render_context*/, const RendRecipeContextVK& p_facts) {
        RendGraphPreparedBindingsVK result;
        result.imports.add("camera_info", *camera_info_ubo);
        return result;
    });
Ref<IRenderGraph> graph = renderer->acquire_render_graph("scene_graph", bindings);
renderer->activate_render_graph(graph);
// Before shutdown: renderer->activate_render_graph(MEM_NULL); graph = MEM_NULL;
```

Bindings (`IRenderGraphBindingsVK`) carry an immutable instance key and, for every build, prepare the serialized recipe
context and the `RendGraphRecipeImportsVK`. They never see the descriptor or the graph, and must not retain the graph, which
retains them. The graph replaces the context's renderer facts with authoritative ones, and supplies the swap-chain container
when the recipe imports `swap_chain` and the bindings do not, acquiring it again for every build because the renderer
replaces it when the number of frames in flight changes.

Instances are cached weakly in `GlobalRenderCache` under `RenderGraphInstanceCache::make_cache_key(definition, instance)`.
Acquiring the same definition with the same instance key and bindings object returns the live graph; another bindings
object for a live identity throws; other instance keys compile independent graphs. A failed compilation caches nothing.
Applications, scenes, and the renderer's activation hold instances strongly; an instance is recompiled after its last
reference is released, and every reference must be released before the renderer shuts down. A `Scene` acquires one instance
per distinct world definition and instance key (`World::set_render_graph_instance_key()`, defaulting to the definition key)
with the bindings given to `Scene::add_render_graph_bindings()`, and queues acquisition, activation, and release through
`add_job()`.

The recipe runs for the first build, whenever the context changes (the swap-chain extent, checked when the graph executes),
and on `RenderGraphVK::rebuild()`; a frame with an unchanged context runs no script and prepares no bindings. Keep one-time
setup outside the bindings: pipeline compiles, application-owned buffers, and initial-data snapshots. Make nonstructural
changes with buffer updates and trigger, toggle, or select events.

A rebuild diffs the new descriptor against the current graph (`RendGraphResourceDiffVK`):

- A graph-owned resource is **kept**, with its content, when its id, kind, per-frame layout, and evaluated description match
  and, for a view, its image is kept. Imports are kept when they bind the same object.
- A changed description is **recreated** in place in its container, losing its content.
- A changed kind, per-frame layout, or import identity is **replaced** with a new container; re-acquire an exported one
  with `acquire_resource()`.
- Every task is recreated from its factory, so task state that must survive a rebuild lives outside the task.

The diff relies on complete `operator==` on the description structs.

## Descriptors

`RendGraphDescVK` holds tasks, resources, and subgraphs.

- **Graph-owned resources** are declared with `add_buffer()`, `add_image()`, `add_image_view()`, and `add_sampler()`, each
  taking a factory of the context that returns `RendBufferDescVK`, `RendImageDescVK`, `RendImageViewDescVK`, or
  `RendSamplerDescVK`. The primary usage (`RendResPrimaryUsageVK`) selects Vulkan usage flags. Buffers take a memory domain,
  optional initial data, and `per_frame`. Per-frame resources live in an `ROCMultislotsVK` with one slot per frame in flight,
  and others in an `ROCSingleVK`. `set_debug_name()` names the objects the graph creates for a resource
  (`<graph key>.<name>`, with `[<slot>]` per frame); unnamed ones are named by their id.
- **Exports** (`add_export(name, resource_id)`) are the names `RenderGraphVK::acquire_resource()` takes, so C++ can reach
  a resource whose id it does not know, such as one a recipe generated. Subgraphs cannot export.
- **Imports** are resources owned outside the graph: `add_resource(id, *bindable)` with any `IBindableRenderResourceVK`.
  Composites such as compiled meshes register their members as `<id>.<member>`. A `RenderResourceListVK` registers
  `<id>[<index>]` entries, and an access or edge on the list expands to every entry.
- **Tasks** are `RendTaskDescVK` entries holding a `task_id`, a `debug_name` that diagnostics and profiles show, a factory
  (`Unique<IRenderTaskVK>(const RendGraphContextVK&)`), edges added with `connect()`, and aliases added with `alias()`.
  Descriptors are copyable; copies share the factory's captures.

Frame-local data is held in resources and updated in place, never rebound. A graph-owned resource is reached through its
export:

```cpp
// The recipe returns graph.build({ tasks = { ... }, exports = { camera_info = camera_info } }).
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
`alias()`; the graph rejects aliases the task never uses, except identity aliases. Unmapped identifiers resolve to
themselves only in C++-built descriptors and imported tasks; recipe tasks' maps are strict
(`RenderTaskResourceMap::require_bindings()`), so every identifier they use must be mapped.

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
with no edge on R to another member. Order a subgraph's accesses to its ports with internal edges. The debug names of its
resources, tasks, and nested subgraphs (their ids when they have none) become `<subgraph name>.<name>`, where the subgraph
name is `RendSubgraphDescVK::debug_name`, or the id when it has none.

## Recipes

A recipe is a Teal (or Lua) `script` resource whose module exports `build(context, parameters)` and returns a
`RendGraphRecipeVK`
(`rend_graph_recipe_params_vk.hh`). `RendGraphScriptVK::run()` executes it in a fresh sandbox, and
`RendGraphRecipeBuilderVK::build()` turns the result into a descriptor. Recipes only build descriptors; they never touch render
objects.

Recipes are pure descriptions written against `render_graph_utils.render_graph_utils`. There is no graph object: resources
and tasks are free-standing handles made by module functions, and `graph.build()` emits every resource and registration
the listed tasks reach.

```lua
local type wbe = require("wbe")
local graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(context: wbe.RendRecipeContextVK, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local draw = graph.draw_mesh({
        mesh = graph.mesh("triangle", { mesh = "triangle" }),
        pipeline = { default = graph.graphics_pipeline("draw_triangle_pipeline", {
            pipeline = "draw_triangle_pipeline",
            color_formats = { context.swap_chain_format },
        }) },
        parts = { "opaque" },
        clear_values = { { color = { 0.0, 0.0, 0.0, 1.0 } } },
    }, { name = "draw_triangle" }):color_target(0, graph.import("swap_chain"))
    return graph.build({ tasks = { draw } })
end

return exports
```

### Resources

A resource is declared by the recipe, imported, or registered:

- Declarations take one descriptor table of the reflected parameters plus an optional debug `name`:
  `graph.buffer{}` (`RendBufferParamsVK`), `graph.storage_buffer{}` (usage `buffer_storage`), `graph.image{}`
  (`RendImageParamsVK`; an image with no extent follows the swap chain), `graph.storage_image{}` (usage
  `image_read_write`), `graph.image_view(image, descriptor?)`, and `graph.clamp_to_edge_sampler{}`. `image:view()` returns
  the image's cached default view, which takes the image's name; `image:view(descriptor)` makes a view of a mip, layer, or
  aspect range. A view keeps its image reachable.
- `graph.import(id)` names a resource C++ supplies under `id` through `RendGraphRecipeImportsVK`; import handles of one id
  denote one import. `import:member(name)` names a member of an imported composite (`<id>.<name>`), such as a mesh entity's
  transform or `mesh_acceleration_structure`'s `top_level`. `graph.port(name)` names a port inside a subgraph.
- Registered handles carry their registration: `graph.mesh(key, description?)`, `graph.compute_pipeline(key,
  description?)`, `graph.graphics_pipeline(key, description?)`, `graph.shader_compute_pipeline(shader)` (compiled from the
  shader of that key, registered under it), and `graph.registered_resource(key, type, description, resource_type)` for
  custom description types. Without a description a handle names an entry registered elsewhere. `graph.registered_image(key,
  description)` registers an image and returns an import handle, which the builder resolves to the registered view.
  Registered handles are only valid in task params fields that name them, never as bindings or edges, and the task checks
  their type.

The build generates declared resources' ids, `<kind>-<8 hex>`, from the kind, the name, and the declaration's ordinal among
those of that kind and name in the order the build reaches them. Rebuilds therefore keep every id and the resources'
contents, and a named resource keeps its id when others are added. The name becomes the Vulkan debug name of the objects
the graph creates (`RendGraphDescVK::set_debug_name()`, as `<graph key>.<name>`) and appears in diagnostics; the ids are
not meant to be read. Name every resource.

```lua
local shadow_depth = graph.image({
    name = "shadowmap_depth",
    usage = "image_depth_target",
    format = context.shadow_depth_format,
    extent = shadow_extent,
}):view()
local column_offsets = graph.storage_buffer({ name = "pixel_sort_column_offsets", size = offsets_size })
local shadow_sampler = graph.clamp_to_edge_sampler({ name = "shadow_sampler" })
```

A registration is emitted, as a `registrations` entry, when a built task's params reach its handle; equal registrations of
a key merge and different ones fail. Recipes register on every build, which compiles nothing for an equal description.

### Tasks

The typed constructors are `graph.compute`, `compute_indirect`, `compute_sequential`, `ping_pong`, `draw`,
`draw_indirect`, `draw_mesh`, `generate_mipmaps`, `full_memory_barrier`, and `cpu_acquire_image`, generated from the task
types that name them (see [Task types](#task-types)). Each is `graph.<method>(params, options?)`, where `options` is
`{ name?, bindings?, trigger?, toggle? }`, and returns a task handle. Params type-check against `graph.RTParams<Name>VK`,
whose resource fields take handles, and only handles. Params and options are copied, so one table can serve several tasks.

```lua
local count_mask = graph.compute({
    pipeline = graph.shader_compute_pipeline("count_pixel_sort_mask_" .. orientation .. "_shader"),
    group_count_x = column_group_count,
    group_count_y = 1,
}, { name = "count_pixel_sort_mask", bindings = {
    pixel_sort_source = source,
    pixel_sort_contrast = contrast,
    pixel_sort_column_offsets = column_offsets,
} })
```

- **Bindings are explicit.** A task's `bindings` map every name its shaders reflect and its render targets use to a
  resource handle, imports included. Recipe tasks' resource maps are strict (`RenderTaskResourceMap::require_bindings()`):
  an unbound name fails instead of resolving to a resource of the same id. A resource that a params field names is bound
  by the build.
- `task:bind(name, resource)` adds a binding; `task:color_target(index, resource)` and `task:depth_target(resource)` bind
  `render_target_colors[<index>]` and `render_target_depth`. A name binds once per task.
- A `compute_sequential` dispatch's `bindings` (and `graph.indirect_dispatch(pipeline, buffer, offset, bindings?)`) bind
  the names of that dispatch's pipeline, so dispatches of one task can bind one shader name differently.
- `graph.task(type, params, options?)` creates a registered type without a constructor; its params name no resource.
- `graph.import_task(id)` references a task built in C++ (see [Imports and C++ tasks](#imports-and-c-tasks)).

`graph.build()` generates task ids, `<type>-<8 hex>` like resource ids, and emits `name` as the task's debug name
(`RendTaskDescVK::debug_name`), which profiles and diagnostics show. Task methods return the task, so calls chain.

### Edges

Edges stay explicit and per resource. Each takes task handles and one resource handle or a non-empty list of them, and adds
one edge per resource:

```lua
graph.chain({ color, depth }, { first, second, third })        -- first -> second -> third on both resources
shadow_opaque:connect(shadow_masked, { shadow_color, shadow_depth })  -- a two-task chain
shadow_masked:connect_all({ main_opaque, main_masked }, shadow_depth) -- fan-out; the consumers stay unordered
build:inputs_from(dynamic_second, { shadow_moments = moments })  -- bindings plus one edge per distinct resource
```

Every adjacent pair of a chain must access every listed resource, and both tasks of an edge must be in the built task list.

### Building

`graph.build({ tasks = { ... }, exports = { <name> = resource } })` takes the tasks in order, which fixes their generated
ids, and emits the recipe. A task listed twice, or an edge to a task the build does not list, is an error. `exports` names
resources C++ reaches with `RenderGraphVK::acquire_resource(<name>)`, since it does not know their generated ids:

```lua
return graph.build({
    tasks = { count_mask, prefix_sum, register_keys, radix_sort, write_pixels, post_processing, present, draw_imgui },
    exports = { post_processing_output_image = resources.post_processing_output_image },
})
```

### Imports and C++ tasks

- Every import is supplied through `RendGraphRecipeImportsVK`; a declared-but-missing or supplied-but-undeclared import is
  an error, except that an unsupplied import resolves to a bindable object registered on the graph under its id
  (`graph.registered_image()`). Recipes cannot set initial data, so buffers that need it are application-owned imports.
- Tasks that need application objects (for example the ImGui overlay) are built in C++, supplied with
  `RendGraphRecipeImportsVK::add_task()`, referenced with `graph.import_task(id)`, and connected like any task. An imported
  task keeps its id and sets no type, params, trigger, or toggle; `:bind()` aliases the identifiers the C++ task uses to
  recipe resources, and what it leaves unmapped resolves to itself:

  ```lua
  local build_acceleration_structure = graph.import_task("build_mesh_acceleration_structure")
      :bind("top_level_acceleration_structure", graph.import("mesh_acceleration_structure"):member("top_level"))
  ```
- Everything else the application owns that shaders name, such as the deferred test's acceleration structure and dynamic
  mesh entities, is an import too; nothing is added to a built descriptor in C++.

### Triggers, toggles, and multiplexers

- `trigger = { event, activate }` wraps the task with `RTTriggeredVK`, which records only on triggered or enabled frames.
  Work that runs once is a trigger activated while pending, which the application disables after the first recorded frame.
- `toggle = { event, enabled }` wraps it with `RTToggleVK`, outside any trigger. Rebuilds re-run the recipe, so pass a
  toggle's current state through the context.
- `graph.multiplex{ name?, branches = { ... }, select = { event, branch }, bindings?, trigger?, toggle? }` records one of
  several typed branch tasks (`RTMultiplexerVK`). It is one graph node reporting every branch's accesses; the branches share
  its bindings and one queue family, carry no edges, triggers, or toggles, and `RTMultiplexerVK::Command` events select the
  branch. Pass the current branch through the context.

```lua
local acquire_params: graph.RTParamsCPUInFlightAcquireVK = { image_view = bloom_ping, event = context.bloom_image_event }
local acquire_bloom = graph.cpu_acquire_image(acquire_params, {
    name = "acquire_bloom",
    trigger = { event = context.bloom_acquire_event },
})
local indirect_lighting = graph.multiplex({
    name = "run_indirect_lighting",
    branches = branches,
    select = context.voxel_debug_view_toggles.lighting,
    bindings = indirect_lighting_bindings,
})
```

All three are driven by commands emitted through `EventManager`. On a subgraph task, a trigger or toggle wraps every member.

### Subgraphs and feature modules

`graph.subgraph{ name?, tasks = { ... }, ports = { <port> = <enclosing resource> }, trigger?, toggle? }` groups tasks as one
subgraph task, connected like any task. Its tasks name the ports with `graph.port(<port>)`, and reach the enclosing graph's
declared resources only through ports; imports stay graph-wide. Its own resources stay scoped to it, and its debug names
become `<subgraph name>.<name>`. Only the outermost recipe exports.

A reusable recipe part lives in `render_graph_utils/features/` and exports `subgraph(name, params, target)`, returning such a
task. Its C++ side has a `configure_recipe()` that serializes the feature's `WBE_REFLECT_SCRIPT` parameter struct (including
the Lua `module` name) into the context and supplies the feature's resources and C++ tasks to the imports. The recipe calls
`require(params.module).subgraph(name, params, target)`:

```lua
-- render_graph_utils/features/pixel_sort.tl
local color = graph.port("color")
-- ... tasks bound to color and to the feature's own resources, ordered with graph.chain() ...
return graph.subgraph({
    name = name,
    tasks = { copy_source, count_mask, prefix_sum, register_keys, radix_sort, write_pixels },
    ports = { color = target.color_view },
    toggle = { event = params.toggle_event, enabled = params.enabled },
})
```

The deferred test's pixel-sort, SVO, and octree features follow this pattern.

### Emitted recipe

`graph.build()` lowers handles to plain tables; nothing else crosses the Lua/C++ boundary:

- `imports`: the import ids.
- `registrations`: `{ key, type, description }`, where `type` is the reflected name of the description struct, applied
  before that level's imports and tasks.
- `resources`: each has an `id`, an optional `name`, and exactly one of `buffer`, `image`, `image_view`, or `sampler`.
- `tasks`: an `id`, an optional `name`, a registered task `type` with strictly deserialized `params`, `aliases` (its
  bindings), `connect` edges (`{ to = <task id>, resource = <resource id> }`), and optional `trigger`, `toggle`, `branches`
  and `select`, or `subgraph` and `ports`. Imported tasks carry `id`, `import = true`, `aliases`, and `connect`.
- `exports`: export names to resource ids.

### Context

`context` holds `RendRecipeContextVK` facts: `frames_in_flight`, `swap_chain_format`, and `swap_chain_extent`
(`RendGraphScriptVK::make_context()`). An application that adds entries declares a `WBE_REFLECT_SCRIPT` struct deriving from
`RendRecipeContextVK`, fills its base with the facts its bindings receive, and returns `serialize_context()` from
`prepare()`. `parameters` holds the definition's static choices (the deferred definitions differ only in
`DeferredRecipeParameters`). Size swap-chain-dependent resources and dispatches in Lua from `swap_chain_extent`.

### Task types

Recipes refer to task types by name. A type is registered by annotating its class
`WBE_META(WBE_RENDER_TASK(<method>), WBE_SERIALIZATION_ALIASES(<name>))` and declaring a reflected static
`create(const RenderContextVK*, const <Params>&, const RendGraphContextVK&)`, or
`create(const RenderContextVK*, IRenderGraph*, const <Params>&, const RendGraphContextVK&)` when it resolves registered
resources. The params struct is `WBE_SERIALIZABLE_STATIC, WBE_STRICT_DESERIALIZATION, WBE_REFLECT_SCRIPT`. An optional static
`prepare_graph()` lets the type add imports and aliases when it is added (`RTGraphicsDrawMeshVK` uses it to import meshes). A
bare `WBE_RENDER_TASK` registers the type without a constructor.

Reflection generates the `RenderTaskRegistryVK` implementation, the declarations in `render_graph_utils.render_graph`
(including a `graph.RTParams<Name>VK` interface for each params struct with resource fields), and, in
`render_graph_utils.render_graph_tasks`, the installer of each `graph.<method>()` constructor.

Annotate the params fields that hold ids, so the constructor accepts handles there and lowers them to ids:

- `WBE_RECIPE_RESOURCE` for graph resources, which the build binds to themselves;
- `WBE_RECIPE_RESOURCE(registered, RenderObjectType)` for keys registered on the graph, where `RenderObjectType` is a
  `WBE_RENDER_OBJECT` class that the handle's type is checked against.

The field holds a `std::string`, optional or in a vector, or a `DataValue` map of ids, directly or in nested params.

| Type | Constructor | Task | Purpose |
| --- | --- | --- | --- |
| `run_compute_pipeline` | `compute` | `RTRunComputePipelineVK` | One compute dispatch |
| `run_compute_pipeline_indirect` | `compute_indirect` | `RTRunComputePipelineIndirectVK` | Dispatch with counts read from a buffer |
| `run_compute_pipelines_sequential` | `compute_sequential` | `RTRunComputePipelinesSequentialVK` | Ordered compute passes in one command buffer |
| `ping_pong` | `ping_pong` | `RTPingPongVK` | Alternates two compute pipelines for N iterations |
| `max_downsample` | — | `RTMaxDownsampleVK` | Mip chain by 2x2 maximum |
| `run_graphics_pipeline` | `draw` | `RTRunGraphicsPipelineVK` | One graphics pipeline draw |
| `run_graphics_pipeline_indirect` | `draw_indirect` | `RTRunGraphicsPipelineIndirectVK` | Non-mesh indexed indirect draws |
| `graphics_draw_mesh` | `draw_mesh` | `RTGraphicsDrawMeshVK` | Draws parts of a compiled mesh |
| `generate_mipmaps` | `generate_mipmaps` | `RTGenerateMipmapsVK` | Color mip chain with filtered blits |
| `full_memory_barrier` | `full_memory_barrier` | `RTFullMemoryBarrierVK` | Orders a frame's mutation of persistent state after the previous frame's consumers |
| `cpu_acquire_image` / `cpu_acquire_buffer` | `cpu_acquire_image` / — | `RTCPUInFlightAcquireVK` / `RTCPUInFlightBufferAcquireVK` | Asynchronous CPU readback, delivered through `EventManager` under the params' `event` |

Compute types take a `queue_family` param and a `push_constants` table.

### Scripts

Recipes and their modules are Teal sources under `res/assets/scripts/teal/` (see
[resource_standard.md](../resource/resource_standard.md#script-resources)). They import the generated declarations of the
`WBE_REFLECT_SCRIPT` types as types only (`local type wbe = require("wbe")`) and annotate `build(context: <context type>):
wbe.RendGraphRecipeVK`. Enums serialize as their canonical names; compare `graph.enum_name(value)` with the canonical
upper-case name, which also accepts the lowercase aliases.

Importing `render_graph_utils.render_graph_utils` also brings the types `graph.Resource`, `graph.Task`, the descriptor
interfaces (`graph.BufferDescriptor`, `graph.ImageDescriptor`, ...), and `graph.RTParams<Name>VK`. For types only, import
the generated declarations with `local type graph = require("render_graph_utils.render_graph")`.

Other graph helpers live next to it and take and return handles: `render_graph_utils.sort` provides
`radix_sort_buffers({ name?, max_element_count, key_type? })` and `radix_sort_task(buffers, { name?, queue_family?,
begin_bit?, end_bit? })`, which sort `uint` keys, or `uint2` keys ordered by `x` and then `y`, by every bit or by a bit range
of an even number of 8-bit digits (bits `[32, 64)` of `uint2` keys sort by `x` and carry `y` as a payload, which
`radix_sort_key_value_task(buffers, options?)` does), and `radix_sort_dispatches(buffers, bits?)` for a longer
`compute_sequential` task; `render_graph_utils.large_prefix_scan` and `render_graph_utils.dedup_large` provide buffers and
dispatch sequences for a `compute_sequential` task; and `render_graph_utils.svo_3` gathers and deduplicates the leaves of
the third sparse voxel octree construction.

## C++ Descriptors

`RenderGraphVK::build_from_descriptor()` builds a fixed C++ `RendGraphDescVK`, which every rebuild applies again. It exists
for focused tests of the compiler (`tests/unit/function/render_graph_vk_test.cpp`); applications build only the
`RendTaskDescVK`s of the native tasks their bindings import.

## Debugging

- `RenderGraphVK::dump_graph_metadata()` describes tasks (`name`, queue, order, subgraph), edges (`resource_name`, stages,
  accesses, cross-queue), and subgraphs (`name`). Names are debug names, or ids where there are none, and diagnostics
  name resources as `'<name>' (<id>)`.
- With timestamp queries enabled, `gpu_timestamp_report.json` includes that structure, with each task's and subgraph's
  `name` and each edge's `resource_name`; `python export_render_graph.py` renders it as Mermaid or SVG.
- Graph-owned Vulkan objects carry their resources' debug names, which validation messages and debug tools show.
- `write_descriptor_debug_output(name, graph->list_descriptors())` dumps the bound descriptors.

## Examples

- `tests/executable/function/render/vulkan_draw_triangle_test.cpp` with `render_graphs/draw_triangle.tl`: the minimal
  definition, whose bindings supply nothing.
- `vulkan_draw_cube_test.cpp` and `vulkan_draw_sponza_test.cpp` with `shadowed_mesh_recipe_context.hh`: application-owned
  buffers, a derived context, and shadow maps.
- `vulkan_pixel_sorting_test.cpp` with `pixel_sorting_recipe_context.hh` and `render_graphs/pixel_sorting.tl`: one
  instance per orientation, an imported ImGui task, and an exported result image.
- `deferred_test/deferred_render_graph.cpp` with `render_graphs/deferred.tl` and `render_graph_utils/features/`: two
  definitions of one recipe, feature subgraphs, triggers, toggles, native tasks, and rebuilds.
