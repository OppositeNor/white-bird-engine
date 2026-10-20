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
            RendGraphScriptVK::make_context(*base_render_data, p_context));
        return RendGraphRecipeBuilderVK().build(recipe, imports, base_render_data);
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

local exports = {}

function exports.build(_context: wbe.RendRecipeContextVK): wbe.RendGraphRecipeVK
    local params: wbe.RTParamsGraphicsDrawMeshVK = {
        mesh = "model_cube_static.mesh",
        pipeline = "draw_cube_unshaded_pipeline",
        parts = { "opaque" },
    }
    return {
        imports = { "swap_chain", "camera_info" },
        tasks = {
            {
                id = "run_static_mesh",
                type = "graphics_draw_mesh",
                params = params,
                aliases = { ["render_target_colors[0]"] = "swap_chain" },
            },
        },
    }
end

return exports
```

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
- A task with a nested recipe in `subgraph` and bound `ports` (built with `utils.render_graph.subgraph()`) becomes a
  subgraph. A trigger or toggle on it wraps every member.

The callback may add to the descriptor the builder returns for parts not yet expressible in Lua. The deferred test adds its
acceleration structure and dynamic mesh entities this way.

### Context

`context` holds `RendRecipeContextVK` facts: `frames_in_flight`, `swap_chain_format`, and `swap_chain_extent`
(`RendGraphScriptVK::make_context()`). An application that adds entries declares a `WBE_REFLECT_SCRIPT` struct deriving from
`RendRecipeContextVK`, fills its base with `get_recipe_context()`, and passes `serialize_context()` to `run()`. Size
swap-chain-dependent resources and dispatches in Lua from `swap_chain_extent`.

### Feature modules

A reusable recipe part lives in `render_graphs/features/` and exports `subgraph(id, params, target)`, returning a subgraph
task. Its C++ side has a `configure_recipe()` that serializes the feature's `WBE_REFLECT_SCRIPT` parameter struct (including the
Lua `module` name) into the context and supplies the feature's resources and C++ tasks to the imports. The recipe calls
`require(params.module).subgraph(id, params, target)`. The deferred test's pixel-sort, SVO, and octree features follow this
pattern.

### Task types

Recipes refer to task types by name. A type is registered by annotating its class
`WBE_META(WBE_RENDER_TASK, WBE_ALIASES = [ <name> ])` and declaring a reflected static
`create(const BaseRenderDataVK*, const <Params>&, const RendGraphContextVK&)`. The params struct is
`WBE_SERIALIZABLE_STATIC, WBE_STRICT_DESERIALIZATION, WBE_REFLECT_SCRIPT`. An optional static `prepare_graph()` lets the type add
imports and aliases when it is added (`RTGraphicsDrawMeshVK` uses it to import meshes). Reflection generates the
`RenderTaskRegistryVK` implementation and the Teal declarations of the params.

| Type | Task | Purpose |
| --- | --- | --- |
| `run_compute_pipeline` | `RTRunComputePipelineVK` | One compute dispatch |
| `run_compute_pipeline_indirect` | `RTRunComputePipelineIndirectVK` | Dispatch with counts read from a buffer |
| `run_compute_pipelines_sequential` | `RTRunComputePipelinesSequentialVK` | Ordered compute passes in one command buffer |
| `ping_pong` | `RTPingPongVK` | Alternates two compute pipelines for N iterations |
| `max_downsample` | `RTMaxDownsampleVK` | Mip chain by 2x2 maximum |
| `run_graphics_pipeline` | `RTRunGraphicsPipelineVK` | One graphics pipeline draw |
| `run_graphics_pipeline_indirect` | `RTRunGraphicsPipelineIndirectVK` | Non-mesh indexed indirect draws |
| `graphics_draw_mesh` | `RTGraphicsDrawMeshVK` | Draws parts of a compiled mesh |
| `generate_mipmaps` | `RTGenerateMipmapsVK` | Color mip chain with filtered blits |
| `full_memory_barrier` | `RTFullMemoryBarrierVK` | Orders a frame's mutation of persistent state after the previous frame's consumers |
| `cpu_acquire_image` / `cpu_acquire_buffer` | `RTCPUInFlightAcquireVK` / `RTCPUInFlightBufferAcquireVK` | Asynchronous CPU readback, delivered through `EventManager` |

Compute types take a `queue_family` param and a `push_constants` table.

### Scripts

Recipes and their modules are Teal sources under `res/assets/scripts/teal/` (see
[resource_standard.md](../resource/resource_standard.md#script-resources)). They import the generated declarations of the
`WBE_REFLECT_SCRIPT` types as types only (`local type wbe = require("wbe")`) and annotate `build(context: <context type>):
wbe.RendGraphRecipeVK`. Since `RendRecipeTaskVK.params` is untyped, type each task's params with its reflected struct. Enums
serialize as their canonical names; compare `utils.render_graph.enum_name(value)` with the canonical upper-case name, which also
accepts the lowercase aliases.

`utils.render_graph` provides resource builders (`buffer`, `storage_buffer`, `image`, `storage_image`, `image_view`,
`clamp_to_edge_sampler`), `indirect_dispatch`, `subgraph`, `enum_name`, and `task_list()`, whose `chain(resource, ids)`
connects a sequence of tasks on one resource.

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
        .create_unique(base_render_data);
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
- `deferred_test/deferred_render_graph.cpp` with `render_graphs/deferred.tl` and `render_graphs/features/`: full deferred
  pipeline with feature subgraphs, triggers, toggles, and C++ additions.
