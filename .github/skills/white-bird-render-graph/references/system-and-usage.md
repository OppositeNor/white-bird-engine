# Render Graph System and Usage

## Pipeline map

1. Application code constructs a `RendGraphDescVK` containing named resources and `RendTaskDescVK` factories.
2. `RendererVK::register_render_graph` stores the descriptor and immediately builds a `RenderGraphVK`.
3. `RenderGraphVK::build_from_descriptor` instantiates tasks, adds explicit predecessor edges, topologically sorts nodes, infers resource dependencies, allocates per-node command buffers, and creates terminal semaphores.
4. Each frame, the renderer acquires a swap-chain image, binds current runtime resources, executes the graph, presents, and resets transient bindings.
5. `RenderGraphVK::execute` records and submits nodes individually in topological order to their selected graphics or compute queue.

## Core APIs

- `include/function/render_vk/render_graph_descriptor_vk.hh`
  - `RendTaskDescVK`: string `task_id`, fluent `after()` / `read()` / `write()` declarations, and a task factory in `create`.
  - `RendGraphDescVK`: allocator-aware vectors of task and resource declarations.
- `include/function/render_vk/render_task_vk.hh`
  - `QueueFamilyVK`, compact `RendResUsageVK` / `ResAccessVK`, and the `RenderTaskVK` contract.
- `include/function/render_vk/render_graph_vk.hh`
  - Descriptor building, internal dependency inference, execution, runtime resource binding, and terminal semaphore access.
- `include/function/render_vk/rt_run_compute_pipeline_vk.hh`
  - General compute dispatch task with compute-queue default and additional resource accesses.
- `include/function/render_vk/rt_generate_mipmaps_vk.hh`
  - Graphics-queue color mip generation with filtered blits and per-level image synchronization.

## Descriptor pattern

Declare resources before graph registration. Resource and task IDs use lower-snake-case strings. A resource declaration pairs the stable string ID used by tasks with the UUID of its concrete render-object type. Type matching is checked when runtime resources are bound.

A pipeline-run task descriptor captures only construction data; the pipeline owns and binds its descriptor sets:

```cpp
RendTaskDescVK task_desc(render_allocator);
task_desc.task_id = "run_lighting";
task_desc.after("run_gbuffer")
    .read("gbuffer_albedo", RendResUsageVK::SAMPLED_IMAGE)
    .write("lighting_output", RendResUsageVK::STORAGE_IMAGE);
task_desc.create = [task_render_allocator = render_allocator,
                       task_base_render_data = base_render_data]() -> Unique<RenderTaskVK> {
    return make_unique<RTRunComputePipelineVK>(task_render_allocator,
        task_base_render_data,
        "lighting_pipeline",
        dispatch_group_count);
};
```

Keep task creation logic in the application class when it has meaningful setup. Preserve `RendTaskDescVK`'s one-shot direct-task ownership rule: copies share the same transfer state, so prefer factories for reusable or rebuildable descriptors.

## Dependency model

`prev` creates explicit execution edges by task ID. Use it for:

- phase boundaries;
- write-after-write ordering;
- ordering with no named resource dependency;
- a required compute-to-presentation path.

After topological sorting, inference tracks the most recent writer for each resource. A later read creates an execution edge from that writer to the reader. A later write replaces the tracked writer. Read-read accesses do not create edges. The compact usage is translated internally to semaphore stage scopes and image-transition access masks filtered to the producer or consumer queue family.

Every inferred dependency creates or reuses an execution edge whose timeline semaphore supplies execution ordering and memory visibility. Explicit predecessor dependencies use the same execution-edge mechanism. Image layout transitions remain explicit image barriers; queue ownership transfers are not encoded by inferred dependencies.

## Queue behavior

`QueueFamilyVK::GRAPHICS` selects the graphics queue and command pool. `QueueFamilyVK::COMPUTE` selects the compute queue and command pool. A compute-capable graphics family may serve both when no dedicated compute family exists.

`RTRunComputePipelineVK` defaults to compute. Keep that default for compute-only dispatches unless they must share graphics-queue ordering unavailable through graph dependencies.

`ROImageVK` and `ROBufferVK` default to all unique renderer-selected queue families. With multiple families they use `VK_SHARING_MODE_CONCURRENT`, so no ownership transfer is needed between graph queues. If a builder supplies a restricted family set and creates an exclusive resource, the caller must account for ownership; the current graph does not generate the required release/acquire transfers.

## Presentation and frame completion

Presentation integration assumes at least one graphics node:

- external acquire waits attach to the first graphics submission;
- the last graphics node transitions the swap-chain image to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`;
- terminal binary semaphore signaling and the in-flight fence attach to the last graphics submission.

Ensure every compute node whose completion matters to presentation reaches the final graphics node through inferred execution edges or explicit `prev` dependencies. The fence represents the dependent work leading to that last graphics submission, not unrelated graph branches.

## Runtime lifecycle

Registration builds the graph immediately. During a frame the renderer binds its current swap-chain resource and applies application-provided resource updates before execution. Transient bindings are cleared after execution.

Rebuilding a graph with allocated command buffers waits for device idle, frees the old buffers, and reconstructs graph state. Unregistration retains the graph and descriptor until the configured frames-in-flight delay expires.

## Failure checklist

- Confirm every read ID appears in the graph resource declarations.
- Confirm runtime-bound object UUIDs match their `RendResVK` declarations.
- Confirm task IDs and `prev` references are valid and acyclic.
- Confirm each compact usage describes the actual command and that reads and writes are declared on the correct side.
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