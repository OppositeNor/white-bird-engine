# Render Graph System and Usage

## Pipeline map

1. Application code constructs a `RendGraphDescVK` containing named resources and `RendTaskDescVK` factories.
2. `RendererVK::register_render_graph` stores the descriptor and immediately builds a `RenderGraphVK`.
3. `RenderGraphVK::build_from_descriptor` instantiates tasks, adds explicit `prev` edges, topologically sorts nodes, infers resource edges, allocates per-node command buffers, and creates terminal semaphores.
4. Each frame, the renderer acquires a swap-chain image, binds current runtime resources, executes the graph, presents, and resets transient bindings.
5. `RenderGraphVK::execute` records and submits nodes individually in topological order to their selected graphics or compute queue.

## Core APIs

- `include/function/render_vk/render_graph_descriptor_vk.hh`
  - `RendTaskDescVK`: `task_id`, predecessor IDs in `prev`, and a task factory in `create`.
  - `RendGraphDescVK`: allocator-aware vectors of task and resource declarations.
- `include/function/render_vk/render_task_vk.hh`
  - `QueueFamilyVK`, `ResAccessVK`, and the `RenderTaskVK` contract.
- `include/function/render_vk/render_graph_vk.hh`
  - Low-level node/edge construction, descriptor building, execution, runtime resource binding, and terminal semaphore access.
- `include/function/render_vk/rt_run_compute_pipeline_vk.hh`
  - General compute dispatch task with compute-queue default and additional resource accesses.

## Descriptor pattern

Declare resources before graph registration. A resource declaration pairs the stable string ID used by tasks with the UUID of its concrete render-object type. Type matching is checked when runtime resources are bound.

A task descriptor factory commonly captures the render allocator, base render data, and descriptor sets:

```cpp
RendTaskDescVK task_desc(render_allocator);
task_desc.task_id = RT_RUN_LIGHTING;
task_desc.prev.emplace_back(RT_RUN_GBUFFER);
task_desc.create = [task_render_allocator = render_allocator,
                       task_base_render_data = base_render_data,
                       task_descriptor_sets = descriptor_sets]() -> Unique<RenderTaskVK> {
    return make_unique<RTRunComputePipelineVK>(task_render_allocator,
        task_base_render_data,
        REND_RES_LIGHTING_PIPELINE,
        dispatch_group_count,
        std::span<const Ref<RODescriptorSetVK>>(task_descriptor_sets.data(), task_descriptor_sets.size()),
        read_accesses,
        write_accesses);
};
```

Keep task creation logic in the application class when it has meaningful setup. Preserve `RendTaskDescVK`'s one-shot direct-task ownership rule: copies share the same transfer state, so prefer factories for reusable or rebuildable descriptors.

## Dependency model

`prev` creates explicit execution edges by task ID. Use it for:

- phase boundaries;
- write-after-write ordering;
- ordering with no named resource dependency;
- a required compute-to-presentation path.

After topological sorting, inference tracks the most recent writer for each resource. A later read creates an edge from that writer to the reader. A later write replaces the tracked writer. Read-read accesses do not create edges.

Execution edges carry timeline semaphore waits and signals. Incoming resource edges also produce a `VkMemoryBarrier2` before the consuming task. The implementation currently uses general memory barriers rather than `VkBufferMemoryBarrier2` or `VkImageMemoryBarrier2`; resource-specific transitions and queue ownership are not encoded by inferred edges.

## Queue behavior

`QueueFamilyVK::GRAPHICS` selects the graphics queue and command pool. `QueueFamilyVK::COMPUTE` selects the compute queue and command pool. A compute-capable graphics family may serve both when no dedicated compute family exists.

`RTRunComputePipelineVK` defaults to compute. Keep that default for compute-only dispatches unless they must share graphics-queue ordering unavailable through graph dependencies.

`ROImageVK` and `ROBufferVK` default to all unique renderer-selected queue families. With multiple families they use `VK_SHARING_MODE_CONCURRENT`, so no ownership transfer is needed between graph queues. If a builder supplies a restricted family set and creates an exclusive resource, the caller must account for ownership; the current graph does not generate the required release/acquire transfers.

## Presentation and frame completion

Presentation integration assumes at least one graphics node:

- external acquire waits attach to the first graphics submission;
- the last graphics node transitions the swap-chain image to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`;
- terminal binary semaphore signaling and the in-flight fence attach to the last graphics submission.

Ensure every compute node whose completion matters to presentation reaches the final graphics node through inferred resource edges or explicit `prev` dependencies. The fence represents the dependent work leading to that last graphics submission, not unrelated graph branches.

## Runtime lifecycle

Registration builds the graph immediately. During a frame the renderer binds its current swap-chain resource and applies application-provided resource updates before execution. Transient bindings are cleared after execution.

Rebuilding a graph with allocated command buffers waits for device idle, frees the old buffers, and reconstructs graph state. Unregistration retains the graph and descriptor until the configured frames-in-flight delay expires.

## Failure checklist

- Confirm every read ID appears in the graph resource declarations.
- Confirm runtime-bound object UUIDs match their `RendResVK` declarations.
- Confirm task IDs and `prev` references are valid and acyclic.
- Confirm access and stage masks describe the actual Vulkan command.
- Add explicit ordering for write-write hazards and dependencies without a later read.
- Confirm compute outputs reach a graphics node before presentation.
- Preserve concurrent sharing for resources crossing distinct queue families.
- Check factory captures remain alive and recreate tasks when rebuilding.

## Canonical examples

- `tests/executable/function/render/vulkan_draw_cube_test.cpp`: compact graphics graph construction and explicit predecessor ordering.
- `tests/executable/function/render/vulkan_draw_sponza_deferred_test.cpp`: graphics-to-compute deferred pipeline, declared shader accesses, and final presentation dependency.
- `tests/unit/function/render_graph_descriptor_vk_test.cpp`: direct-task ownership and descriptor-copy behavior.
- `src/function/render_vk/render_graph_vk.cpp`: authoritative build, inference, recording, barrier, and submission behavior.
- `src/function/render_vk/renderer_vk.cpp`: graph registration, per-frame resource binding, execution, presentation, and delayed destruction.
