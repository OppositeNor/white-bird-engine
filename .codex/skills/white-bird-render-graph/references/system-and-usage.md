# Render Graph System and Usage

## Pipeline map

1. Application code constructs a `RendGraphDescVK` containing named resources and `RendTaskDescVK` factories.
2. `RendererVK::register_render_graph` stores the descriptor and immediately builds a `RenderGraphVK`.
3. `RenderGraphVK::build_from_descriptor` instantiates tasks, adds resource edges, rejects cycles while topologically sorting nodes, validates that edges on each resource order every conflict on it, derives same-queue barriers and cross-queue semaphores, allocates per-node command buffers, and creates terminal semaphores.
4. Each frame, the renderer acquires a swap-chain image, updates the explicitly bound swap-chain image-view container for the current frame slot, executes the graph, and presents.
5. `RenderGraphVK::execute` records and submits nodes individually in topological order to their selected graphics or compute queue.

## Core APIs

- `include/function/render_vk/render_graph_descriptor_vk.hh`
  - `RendTaskDescVK`: string `task_id`, fluent `connect()` / `read()` / `write()` declarations, and a task factory in `create`.
  - `RendGraphDescVK`: allocator-aware vectors of task and resource declarations, plus `get_task()` for connecting added tasks.
- `include/function/render_vk/i_render_task_vk.hh`
  - `QueueFamilyVK`, `RendEdgeTypeVK`, compact `RendResUsageVK` / `ResAccessVK`, and the `IRenderTaskVK` contract.
- `include/function/render_vk/render_graph_vk.hh`
  - Descriptor building, resource-edge validation, synchronization derivation, execution, bind-once resource contracts, and terminal semaphore access.
- `include/function/render/render_resource_list.hh`
  - CPU-side aggregate of render objects that can be bound once and accessed through one graph resource key.
- `include/function/render_vk/rt_run_compute_pipeline_vk.hh`
  - General compute dispatch task with compute-queue default and additional resource accesses.
- `include/function/render_vk/rt_generate_mipmaps_vk.hh`
  - Graphics-queue color mip generation with filtered blits and per-level image synchronization.

## Descriptor pattern

Declare resources before graph registration. Resource and task IDs use lower-snake-case strings. A resource declaration pairs the stable string ID used by tasks with the UUID of its concrete render-object type and a non-null `RenderObjectContainerVK`. `RenderGraphVK::build_from_descriptor()` now rejects null containers, so descriptor construction must be complete before registration.

`RenderResourceList` is the aggregate exception to the container rule. Binding a list registers the aggregate key and each entry
as `<resource_list_key>[<index>]`. A task read or write, or an edge, on the aggregate expands to all indexed entries before
validation and synchronization derivation. Populate the list before graph construction; the list itself is CPU-only and
does not create a GPU object.

All resources belong on `RendGraphDescVK` and are registered once for the graph. Tasks refer to those resources by the same public
IDs in their intrinsic or descriptor-level read and write declarations. Static-set vertex, index, opaque and masked indirect
draw-command, reflected shader, and texture-list resources follow this rule;
`REND_RES_STATIC_SET_OPAQUE_DRAW_BATCHES` and `REND_RES_STATIC_SET_MASKED_DRAW_BATCHES` are also storage-buffer capable for
explicitly ordered compute writes.

A pipeline-run task descriptor captures only construction data. The render graph binds the renderer-owned resource and sampler heaps for every command buffer, and the pipeline pushes its reflected descriptor indices when it records:

```cpp
RendTaskDescVK task_desc(render_allocator);
task_desc.task_id = "run_lighting";
task_desc.read("gbuffer_albedo", RendResUsageVK::SAMPLED_IMAGE)
    .write("lighting_output", RendResUsageVK::STORAGE_IMAGE)
    .connect("present", "lighting_output", RendEdgeTypeVK::READ, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
// The G-buffer pass was added earlier; connect it to the lighting task.
graph_desc.get_task("run_gbuffer")
    .connect("run_lighting", "gbuffer_albedo", RendEdgeTypeVK::READ, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
task_desc.create = [task_render_allocator = render_allocator,
                       task_base_render_data = base_render_data]() -> Unique<RenderTaskVK> {
    return make_unique<RTRunComputePipelineVK>(task_render_allocator,
        task_base_render_data,
        "lighting_pipeline",
        dispatch_group_count);
};
```

Keep task creation logic in the application class when it has meaningful setup. Preserve `RendTaskDescVK`'s one-shot direct-task ownership rule: copies share the same transfer state, so prefer factories for reusable or rebuildable descriptors.

## Resource dependency model

Edges are the only ordering mechanism. `connect()` on the earlier task's descriptor adds one edge carrying one resource:

- `RendEdgeTypeVK::READ`: the next task reads the resource after the previous task's use.
- `RendEdgeTypeVK::WRITE`: the next task writes the resource after the previous task's use. An edge into a task that declares a
  write to the resource must be `WRITE`.
- `src_stage`: stages where the previous task last uses the resource. `dst_stage`: stages where the next task first uses it.

Graph construction fails on cycles, self edges, unknown tasks, undeclared resources, empty stages, and graphics-only stages on
compute tasks. After topological sorting, every read-write, write-read, or write-write pair of declared accesses on a resource
must be connected by a chain of edges on that same resource; a transitive chain is sufficient, but edges on other resources are
not. Read-read accesses do not conflict. Consecutive edges of a chain must share a stage at the middle task so the dependencies
chain. Views of the same image are separate resource ids; connect the id each task actually accesses.

Synchronization is derived per edge. Same-queue edges become one pipeline barrier recorded before the next task, merged per
resource: a buffer barrier for buffers, a whole-image barrier (every mip level and layer) for image views in `GENERAL`, and a global
memory barrier for other resources. The source access is memory write; the destination access is memory read, plus memory write
for `WRITE` edges. Cross-queue edges signal the producer's timeline semaphore at the merged source stages and wait on it at the
merged destination stages. Image layout transitions out of `UNDEFINED` or `PRESENT_SRC_KHR` remain automatic for written images;
queue ownership transfers are not encoded.

## Queue behavior

`QueueFamilyVK::GRAPHICS` selects the graphics queue and command pool. `QueueFamilyVK::COMPUTE` selects the compute queue and command pool. A compute-capable graphics family may serve both when no dedicated compute family exists.

`RTRunComputePipelineVK` defaults to compute. Keep that default for compute-only dispatches unless they must share graphics-queue ordering unavailable through graph dependencies.

`ROImageVK` and `ROBufferVK` default to all unique renderer-selected queue families. With multiple families they use `VK_SHARING_MODE_CONCURRENT`, so no ownership transfer is needed between graph queues. If a builder supplies a restricted family set and creates an exclusive resource, the caller must account for ownership; the current graph does not generate the required release/acquire transfers.

## Presentation and frame completion

Presentation integration assumes at least one graphics node:

- external acquire waits attach to the first graphics submission;
- the last graphics node transitions the swap-chain image to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`;
- terminal binary semaphore signaling and the in-flight fence attach to the last graphics submission.

Every task on a queue other than graphics must reach a graphics task through edges; graph construction fails otherwise. The fence then covers that work through the graphics queue's submission order.

## Runtime lifecycle

Registration builds the graph immediately. During a frame the renderer updates the contents of already-bound containers before execution; it does not rebind graph resources after build.

The swap-chain image view is held in a renderer-owned `ROCMultislotsVK` container. Applications acquire shared ownership through `RendererVK::acquire_swap_chain_image_view_container()` and bind it under `REND_RES_SWAP_CHAIN` before graph registration; registration never inserts or replaces descriptor resources. Swap-chain recreation keeps that container stable when possible and replaces its per-slot image views in place. Use `ROCMultislotsVK(..., false)` for swap-chain-style replacement so `set()` skips buffer-update generation flushing. `ROPipelineVK` likewise resolves descriptor bindings backed by a `RenderObjectContainerVK` from `current_frame_slot` before pushing indices. All render objects and backend helpers remain rendering-thread-confined; cross-thread work goes through `IRenderer::add_job()`.

`ROCBindingVK` has been removed from the codebase. Late persistent rebinding and transient graph-resource override APIs were removed from the graph path; if a resource must vary, represent that variation inside its bound container instead.

Rebuilding a graph with allocated command buffers waits for device idle, frees the old buffers, and reconstructs graph state. Unregistration retains the graph and descriptor until the configured frames-in-flight delay expires.

## Failure checklist

- Confirm every read ID appears in the graph resource declarations.
- Confirm every `RendResVK` is bound to a non-null container before registration and that container UUIDs match their declarations.
- For a `RenderResourceList`, confirm every entry is a render object for the active backend and populate the list before graph
  registration.
- Confirm task IDs and `connect()` targets are valid and acyclic.
- Confirm each compact usage describes the actual command and that reads and writes are declared on the correct side.
- Add a chain of edges on the resource for every read-write, write-read, and write-write hazard, with stages that chain.
- Confirm compute tasks reach a graphics task through edges.
- Preserve concurrent sharing for resources crossing distinct queue families.
- Check factory captures remain alive and recreate tasks when rebuilding.

## Canonical examples

- `tests/executable/function/render/vulkan_draw_cube_test.cpp`: compact graphics graph construction with one resource edge.
- `tests/executable/function/render/deferred_test/deferred_render_graph.cpp`: graphics-to-compute deferred pipeline wired with `connect_resource_chain()` from `render_test.hh`.
- `tests/unit/function/render_graph_descriptor_vk_test.cpp`: direct-task ownership and descriptor-copy behavior.
- `src/function/render_vk/render_graph_vk.cpp`: authoritative build, edge validation, heap binding, barrier, and submission behavior.
- `src/function/render_vk/renderer_vk.cpp`: graph registration, swap-chain image-view container acquisition and per-frame updates, execution, presentation, and delayed destruction.
