# Render Graph System and Usage

## Pipeline map

1. The application calls `RendererVK::acquire_render_graph(definition_key, bindings)`. On a miss it loads the
   `RURenderGraph` definition, creates a `RenderGraphVK`, compiles it immediately through `build_from_recipe()`,
   notifies plugins, and only then caches it weakly in `GlobalRenderCache` under
   `RenderGraphInstanceCache::make_cache_key(definition, instance)`; a live instance with the same identity and bindings
   object is returned instead.
2. A build prepares the bindings (`IRenderGraphBindingsVK::prepare()`) for the renderer facts of the current
   `RendGraphContextVK` (`swap_chain_extent`), runs the recipe's `build(context, parameters)`, builds the descriptor with
   `RendGraphRecipeBuilderVK` (supplying the swap chain when the recipe imports it), and applies it: it diffs resources
   against the current graph, creates or keeps containers, flattens subgraphs, calls every task factory, adds edges,
   topologically sorts and rejects cycles, validates that edges order every conflict, derives barriers and semaphores,
   and allocates command buffers. Each build logs `Compiling render graph '<name>' from recipe '<key>'`.
3. `execute()` first calls `update_context(RendGraphContextVK::from_render_data(...))`, which rebuilds when the context
   changed and otherwise runs no script. `rebuild()` forces a rebuild (for example after state the bindings read changed
   structurally).
4. Each frame the renderer acquires the swap-chain image, updates the current slot of the swap-chain view container,
   records and submits the active graph's nodes in topological order to their queues, presents, and waits for the frame.
5. `activate_render_graph(graph)` retains the graph until another is activated or `MEM_NULL` clears it. A graph is
   destroyed when its last reference is released; callers release theirs before the renderer shuts down.

When the graph has a name and a global render cache, graph-owned objects are cached there as `<graph name>.<resource id>`
(the definition key, followed by `[<instance key>]` for other instances),
with `[slot]` appended for per-frame copies. Every graph-owned object is named `<graph name>.<debug name>` (same suffix),
its resource's debug name (`RendGraphDescVK::set_debug_name()`, from the recipe's `name`) or else its id.

## Core APIs

- `function/render_vk/render_graph_descriptor_vk.hh`: `RendGraphDescVK` (tasks, resources, `add_*`, `set_debug_name`,
  `add_export`, `add_task`, `get_task`, `add_subgraph`, `get_subgraph`, `contains_resource`), `RendTaskDescVK` (`task_id`,
  `debug_name`, `factory`, `connect()`, `alias()`), `RendSubgraphDescVK` (with its `debug_name`), `RendSubgraphVK`, and the
  factory typedefs.
- `function/render/render_task_resource_map.hh`: `RenderTaskResourceMap`, the task-local to graph id map behind `alias()`;
  `require_bindings()` makes it strict, as recipe tasks' maps are, so every identifier the task uses must be mapped.
- `function/render/render_resource_unit.hh` (`RURenderGraph`), `function/render/i_render_graph_bindings.hh`,
  `function/render_vk/i_render_graph_bindings_vk.hh`, `function/render_vk/render_graph_bindings_function_vk.hh`, and
  `function/render/render_graph_instance_cache.hh`: definitions, bindings, and instance identity.
- `function/render_vk/render_graph_vk.hh`: `build_from_recipe()`, `build_from_descriptor()`, `update_context()`, `rebuild()`, `acquire_resource(export_name)`,
  `dump_graph_metadata()`, `list_descriptors()`.
- `function/render_vk/rend_graph_resource_diff_vk.hh`: rebuild diff planning.
- `function/render_vk/i_render_task_vk.hh`: `QueueFamilyVK`, `RendResUsageVK`, `ResAccessVK`, and the `IRenderTaskVK`
  contract (`queue_family()`, `append_accesses()`, `record()`).
- `function/render_vk/render_task_registry_vk.hh`: registered task types (see the recipes reference).
- `function/render_vk/render_resource_list_vk.hh`: CPU-side aggregate of render objects bound under one key; binding it
  registers `<key>[<index>]` entries, and accesses or edges on the aggregate expand to all entries
  (`MeshResourcesVK::acquire_textures()` uses it).

## Descriptor pattern (C++, compiler tests only)

`RendTaskDescVK` holds a `RendTaskFactoryVK` (`Unique<IRenderTaskVK>(const RendGraphContextVK&)`). It is copyable and
movable; copies share the factory's captures. The graph keeps the descriptors and calls each factory on every build, and
throws when a factory is missing or returns null. Applications only build `RendTaskDescVK`s for native tasks their bindings
import; complete C++ descriptors (`RenderGraphVK::build_from_descriptor()`) are for focused compiler tests.

```cpp
// A compiler test's descriptor, bound to the graph it is built for.
RendGraphDescVK graph_desc(render_allocator, &graph);
graph_desc.add_resource(REND_RES_SWAP_CHAIN, *swap_chain_container)
    .add_image("shadow_depth_image",
        [=](const RendGraphContextVK& /*p_context*/) {
            return RendImageDescVK{
                .usage = RendResPrimaryUsageVK::IMAGE_DEPTH_TARGET,
                .format = VK_FORMAT_D32_SFLOAT,
                .extent = shadow_extent,
            };
        })
    .add_image_view("shadow_depth", "shadow_depth_image");

RendTaskDescVK task_desc(render_allocator);
task_desc.task_id = "run_lighting";
task_desc.alias(RenderTargetsVK::color_id(0), REND_RES_SWAP_CHAIN);
task_desc.factory = [=](const RendGraphContextVK& p_context) {
    return RTRunGraphicsPipelineVK::Builder(render_allocator)
        .set_pipeline(lighting_pipeline)
        .set_render_extent(p_context.swap_chain_extent)
        .create_unique(render_context);
};
graph_desc.get_task("run_shadow").connect("run_lighting", "shadow_depth");
graph_desc.add_task(std::move(task_desc));
```

Resource factories return `RendBufferDescVK`, `RendImageDescVK`, `RendImageViewDescVK`, or `RendSamplerDescVK`; view and
sampler factories may be null to take defaults. The primary usage (`RendResPrimaryUsageVK`) selects Vulkan usage flags
(`additional_usage` adds more). Images are device-local; buffers take a `DeviceDomain`, `per_frame`, and optional initial
data. Unset view fields follow the viewed image, and depth formats default to the depth aspect. Size swap-chain-dependent
resources from the context. Ids are unique lower-snake-case strings across imports and graph-owned resources.

Every import is an `IBindableRenderResourceVK`; null containers are rejected. When an import varies by frame, bind one
stable container (`ROCMultislotsVK`) and mutate the objects inside it. Swap-chain-style containers use `p_flush = false`. The
renderer replaces the swap-chain container when `frames_in_flight` changes, so the graph acquires it again on every build.

Update graph-owned buffers through `acquire_resource(export_name)` on the rendering thread, and re-acquire after a rebuild
that replaced the container. Only exported resources (`add_export()`, a recipe's `graph.build{ exports = ... }`) can be
acquired, since recipe resource ids are generated.

## Rebuild diffing

Each resource has a `RendResSignatureVK`: kind, evaluated description, `image_id`, `per_frame`, and import identity.
`RendGraphResourceDiffVK::plan()` returns one action per id:

- `CREATE`: new id.
- `KEEP`: same kind, per-frame layout, and description; imports bind the same object; views view the same, kept image.
  Content is preserved.
- `RECREATE`: the description changed; objects are recreated in place in the same container, losing content.
- `REPLACE`: kind, per-frame layout, or import identity changed; a new container is created.

Views of recreated images are recreated, ids missing from the new descriptor are released, and every task is recreated.
Rebuilding frees existing command buffers after `vkDeviceWaitIdle`.

## Subgraphs

`RendGraphDescVK::add_subgraph(id, RendSubgraphDescVK, port bindings)` groups tasks, owned resources, and internal edges under
one id and returns a `RendSubgraphVK&` to `connect` from. It is flattened on add: member tasks and owned resources become
`<id>.<name>`; names resolve to a port's bound resource, else the subgraph's own resource, else the enclosing graph's.
Imports stay graph-wide, and importing the same object again merges. Edges to and from the subgraph id resolve at build from
accesses: an incoming edge on R reaches members that access R without an edge on R from another member, and an outgoing one
leaves members that access R without an edge on R to another member. Order a subgraph's port accesses with internal edges.
Debug names of its resources, tasks, and nested subgraphs (their ids when unnamed) become `<subgraph name>.<name>`, and a
subgraph cannot export.

## Resource dependency model

- Edge source: the union of the earlier task's usage stages; source access is its writes only.
- Edge destination: the union of the later task's usage stages, reads, and writes.
- A task whose barrier covers everything reports `RendResUsageVK::ALL_COMMANDS`.

Construction fails on cycles, self edges, unknown tasks, undeclared resources, edges whose tasks do not both access the
resource, graphics-only usages on compute tasks, and conflicts not ordered by a same-resource edge chain. Views of one image
are separate ids; connect the id each task actually accesses.

Same-queue edges become one pipeline barrier per resource before the later task: buffer barriers for buffers, whole-image
barriers in the unified `GENERAL` layout for image views, global memory barriers otherwise. Cross-queue edges signal the
producer's timeline semaphore at merged source stages and wait at merged destination stages. Layout transitions out of
`UNDEFINED` / `PRESENT_SRC_KHR` are automatic for written images; queue-family ownership transfers are not emitted.

## Queue behavior

`GRAPHICS` selects the graphics queue. `COMPUTE_1` and `COMPUTE_2` select two queues of the compute family, sharing its
command pool. Logical queues get their own `VkQueue` while the family has unused queues, and otherwise share one (edges on a
shared `VkQueue` become barriers). A compute-capable graphics family may serve all logical queues.

`ROImageVK` and `ROBufferVK` default to all renderer-selected queue families with `VK_SHARING_MODE_CONCURRENT`. An explicit
exclusive override needs ownership transfers the graph does not generate.

## Presentation and frame completion

Acquire waits attach to the first graphics submission; the last graphics node transitions the swap-chain image to
`PRESENT_SRC_KHR` and carries the terminal semaphore and in-flight fence. Every task on a non-graphics `VkQueue` must reach
a graphics task through edges (skipped when the graph has no graphics task).

## Built-in task types

All in `include/function/render_vk/`. Registered types are usable from recipes by their recipe type name; the
constructor column names the generated `graph.<constructor>()`.

| Class | Recipe type | Constructor | Purpose |
| --- | --- | --- | --- |
| `RTRunComputePipelineVK` | `run_compute_pipeline` | `compute` | One compute dispatch; queue configurable, default graphics |
| `RTRunComputePipelineIndirectVK` | `run_compute_pipeline_indirect` | `compute_indirect` | Dispatch with counts read from a buffer |
| `RTRunComputePipelinesSequentialVK` | `run_compute_pipelines_sequential` | `compute_sequential` | Ordered compute passes in one command buffer |
| `RTPingPongVK` | `ping_pong` | `ping_pong` | Alternates two compute pipelines for N iterations |
| `RTMaxDownsampleVK` | `max_downsample` | — | Mip chain by 2x2 max, one view per mip |
| `RTRunGraphicsPipelineVK` | `run_graphics_pipeline` | `draw` | One graphics pipeline draw (`Builder`) |
| `RTRunGraphicsPipelineIndirectVK` | `run_graphics_pipeline_indirect` | `draw_indirect` | Indexed indirect draws for non-mesh geometry |
| `RTGraphicsDrawMeshVK` | `graphics_draw_mesh` | `draw_mesh` | Draws parts of a compiled mesh; imports and aliases it |
| `RTGenerateMipmapsVK` | `generate_mipmaps` | `generate_mipmaps` | Color mip chain with filtered blits |
| `RTFullMemoryBarrierVK` | `full_memory_barrier` | `full_memory_barrier` | Orders a frame's persistent-state mutation after the previous frame's consumers |
| `RTCPUInFlightAcquireVK` | `cpu_acquire_image` | `cpu_acquire_image` | Async image readback, emitted through `EventManager` under the params' `event` |
| `RTCPUInFlightBufferAcquireVK` | `cpu_acquire_buffer` | — | Async buffer readback, emitted through `EventManager` under the params' `event` |
| `RTRunGraphicsPipelinesSequentialVK` | — | — | Ordered graphics passes in one command buffer |
| `RTBuildAccelerationStructuresVK` | — | — | Records a prepared acceleration-structure build |
| `RTTriggeredVK` | task `trigger` option | — | Records its child on triggered or enabled frames |
| `RTToggleVK` | task `toggle` option | — | Records its child while enabled |

Types without a constructor are created with `graph.task(type, params, options?)`, whose params name no resource. Compute tasks default to `GRAPHICS`; graphics
tasks always run on graphics. Prefer an existing type before adding one.

## Debug output

- `RenderGraphVK::dump_graph_metadata()` returns `RendGraphMetadataVK` (tasks with `name`, queue, order, and subgraph; edges
  with `resource_name`, stages, accesses, and `cross_queue`; subgraphs with `name`). Names are debug names, or ids where
  there are none; diagnostics describe resources as `'<name>' (<id>)`.
- `TimestampQueryPluginVK` (enabled by `RendererCapabilities::timestamp_query`) writes `gpu_timestamp_report.json`, whose
  tasks and subgraphs carry `name` and edges `resource_name`; `python export_render_graph.py` turns it into Mermaid
  Markdown or Graphviz SVG.
- `write_descriptor_debug_output(name, graph->list_descriptors())` dumps descriptor rows (to `test_env/descriptor_debug/` in
  tests).

## Failure checklist

- Every name a recipe task uses is bound to a resource handle (recipe task maps are strict), every id a C++ task accesses
  is declared, imported, or aliased to one, and every import is supplied with a non-null container.
- Every conflict has a same-resource edge chain, and both ends of each edge access the resource.
- Storage resources that are only read are annotated `read`; otherwise they count as writes.
- Compute tasks on another queue reach a graphics task.
- The bindings do not compile pipelines or snapshot initial data; they recreate native task factories on every `prepare()`.
- After a rebuild, re-acquire containers that were `REPLACE`d.
- Concurrent sharing is preserved for resources crossing queue families.

## Canonical examples

- `tests/executable/function/render/vulkan_draw_triangle_test.cpp`: the smallest definition, with bindings that supply nothing.
- `vulkan_draw_cube_test.cpp`, `vulkan_draw_sponza_test.cpp`: application-owned buffers with initial data, shadow maps, and a
  derived context; `vulkan_draw_skinned_mesh_test.cpp`: an imported native ImGui task.
- `vulkan_pixel_sorting_test.cpp`: one instance per orientation, switched by activation, reading its result image back
  through an export;
  `deferred_test/deferred_render_graph.cpp`: two definitions differing in parameters, feature modules, and rebuilds.
- `src/function/render_vk/render_graph_vk.cpp`: authoritative build, diff, validation, barrier, and submission behavior.
- `src/function/render_vk/renderer_vk.cpp`: acquisition, activation, swap-chain container, execution, presentation, delayed
  destruction.
- `tests/unit/function/render_graph_vk_test.cpp`, `render_graph_instance_cache_test.cpp`, `render_graph_descriptor_vk_test.cpp`,
  `rend_graph_resource_diff_vk_test.cpp`, `render_resource_list_vk_test.cpp`.
