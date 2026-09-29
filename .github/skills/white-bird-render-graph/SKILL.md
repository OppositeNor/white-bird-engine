---
name: white-bird-render-graph
description: Build, extend, and diagnose White Bird Engine's Vulkan render graphs. Apply when declaring graph resources and task dependencies, implementing RenderTaskVK types, scheduling graphics or compute work, binding per-frame resources, or debugging render-graph synchronization and lifecycle behavior.
---

# White Bird Render Graph

Treat a render graph as a statically built DAG of `IRenderTaskVK` nodes connected by resource dependency edges on named `RendResVK` resources. Use `RendGraphDescVK` and `RendTaskDescVK` for application-owned graph descriptions; use `RenderGraphVK` directly only when lower-level graph construction is required.

## Build a graph descriptor

1. Declare every resource tasks use. The graph compiler allocates and owns resources declared with `add_buffer()`, `add_image()`, `add_image_view(view_id, image_id, ...)`, and `add_sampler()`: each takes a primary usage (`RendResPrimaryUsageVK`, such as `IMAGE_READ_WRITE`, `IMAGE_RENDER_TARGET`, `IMAGE_DEPTH_TARGET`, or `BUFFER_UNIFORM`) plus format, extent, mip levels, memory domain, per-frame allocation, and initial data as applicable. Import resources owned elsewhere (the swap chain, loaded textures, buffers shared between graphs) with `add_resource()`, and a compiled static set with `add_static_set()`.
2. Add task descriptors with unique lower-snake-case string `task_id` values and factories that return `Unique<IRenderTaskVK>`.
3. Connect tasks with `connect(next_task_id, resource_id, RendEdgeTypeVK::READ | WRITE, src_stage, dst_stage)` on the earlier task's descriptor. The edge names the resource, whether the next task reads or writes it, the stages where the previous task last uses it, and the stages where the next task first uses it. Use `RendGraphDescVK::get_task()` to connect a task that was already added. Declared resource accesses never create edges, and there is no other ordering mechanism.
4. Map task-local resource identifiers to graph resources with `RendTaskDescVK::alias()`: reflected shader ids that differ from the graph's, and the render targets `render_target_colors[<index>]` / `render_target_depth` of graphics tasks. The graph rejects aliases the task never uses.
5. Register and activate the graph through `RendererVK`; do not manually drive graph execution from application code.

Task factories run while the graph is built. Capture only data that remains valid through construction, and allocate tasks with the render allocator. A descriptor holding a direct task rather than a recreatable factory has one-shot ownership semantics, including across descriptor copies.

## Declare task access precisely

Every task implements `queue_family()`, `append_accesses()`, and `record()`, and reports all of its accesses itself: pipeline-run tasks report their reflected shader resources (access comes from the optional third `WBE_DESCRIPTOR` argument), graphics tasks report their render targets, and fixed-function tasks report their buffers and images. Each `ResAccessVK` uses the exact graph resource ID plus a compact `RendResUsageVK`. Vulkan access and pipeline-stage masks are derived privately by the graph.

Graph construction rejects cycles, self edges, unknown task ids, undeclared resources, empty stages, and graphics-only stages on compute tasks. Every read-write, write-read, or write-write conflict between declared accesses must be ordered by a chain of edges carrying that same resource; edges on other resources do not count. Read-read pairs do not conflict. Consecutive edges on one resource must share a stage at the middle task, and an edge into a task that writes the resource must be a `WRITE` edge.

Prefer existing task types such as `RTRunComputePipelineVK`, `RTRunGraphicsPipelineVK`, and `RTRunGraphicsPipelineIndirectVK` before adding a new `IRenderTaskVK`. Render-graph command buffers bind the renderer-owned descriptor heaps before task recording, and pipeline-run tasks bind their reflected fields to graph resources at graph build and push their descriptor indices automatically. `RTRunComputePipelineVK` defaults to `QueueFamilyVK::COMPUTE`; do not override it to graphics without a concrete queue constraint.

## Use queues and resources safely

Graphics and compute nodes record from queue-specific command pools and submit to their corresponding queues. Edges between tasks on the same queue become pipeline barriers recorded before the next task: buffer barriers for buffers, whole-image barriers for image views in the unified `GENERAL` layout, and global memory barriers otherwise. Edges crossing queues use one timeline semaphore per producer task. Never place graphics-only pipeline stages on compute tasks.

Images and buffers shared across renderer-selected queue families should retain their default queue-family configuration. Their builders then use concurrent sharing when multiple families exist. An explicit exclusive-sharing override requires queue-family ownership transfers, which render-graph resource barriers do not currently emit.

Presentation remains graphics-specific: swap-chain acquire waits attach to the first graphics node, and terminal signaling, presentation layout transition, and the in-flight fence attach to the last graphics node. Every task on another queue must have an edge path to a graphics task; graph construction fails otherwise.

## Bind runtime resources through the renderer

Render-graph resources are bind-once for the graph lifetime. `RenderGraphVK::build_from_descriptor()` allocates graph-owned resources before instantiating tasks, rejects duplicate ids, and rejects any imported `RendResVK` whose container is null. Graph-owned resources live in an `ROCSingleVK`, or an `ROCMultislotsVK` with one slot per frame in flight when declared per frame; a view of a per-frame image is per-frame. Update graph-owned buffers through `RenderGraphVK::acquire_resource(id)` on the rendering thread. When an imported resource varies by frame, bind one stable container up front and mutate the render objects inside it instead of rebinding the graph resource.

The swap-chain path uses this pattern: the application binds `REND_RES_SWAP_CHAIN` to `RendererVK::acquire_swap_chain_image_view_container()` before graph registration, and each frame the renderer replaces only the current slot's image view before execution. Construct swap-chain `ROCMultislotsVK` containers with `p_flush = false` so slot replacement does not trigger buffer-update generation tracking.

Do not rely on post-build `set_resource()` or transient graph-resource overrides; those late-binding paths are no longer supported. `ROCBindingVK` was removed from the codebase; if a resource must vary, represent that variation inside the bound container itself.

Graph rebuild frees existing command buffers and waits for the device to become idle. Unregistration defers destruction for the configured frames in flight. Preserve recreatable task factories when a descriptor may be rebuilt.

Render objects and Vulkan backend helpers are confined to the rendering thread. Submit cross-thread updates through `IRenderer::add_job()`; do not add locks to these objects merely to permit direct cross-thread access.

## Validate

Use the repository wrapper and select the smallest relevant render test filter first:

```sh
python validation.py -t debug -- --gtest_filter='WBERenderGraph*'
```

The wrapper runs the complete unit suite after the focused tests pass. Do not invoke CMake directly or run lint automatically.

## Advanced work

Read [references/system-and-usage.md](references/system-and-usage.md) before changing edge validation, synchronization, queue behavior, descriptor-heap binding, bind-once resource setup, graph rebuilding, or presentation integration. It also identifies canonical graphics and deferred-compute examples.
