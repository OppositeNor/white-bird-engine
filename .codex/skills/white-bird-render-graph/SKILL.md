---
name: white-bird-render-graph
description: Build, extend, and diagnose White Bird Engine's Vulkan render graphs. Apply when declaring graph resources and task dependencies, implementing RenderTaskVK types, scheduling graphics or compute work, binding per-frame resources, or debugging render-graph synchronization and lifecycle behavior.
---

# White Bird Render Graph

Treat a render graph as a statically built DAG of `RenderTaskVK` nodes operating on named `RendResVK` resources. Use `RendGraphDescVK` and `RendTaskDescVK` for application-owned graph descriptions; use `RenderGraphVK` directly only when lower-level graph construction is required.

## Build a graph descriptor

1. Add one `RendResVK` declaration for every resource ID that tasks read. Set its type to `TypeUUID<ConcreteRenderObject>::UUID`.
2. Add task descriptors with unique lower-snake-case string `task_id` values and factories that return `Unique<RenderTaskVK>`.
3. Use `after()` for phase ordering and dependencies that resource inference cannot establish.
4. Use descriptor `read()` / `write()` calls for shader resources not declared intrinsically by the task.
5. Register and activate the graph through `RendererVK`; do not manually drive graph execution from application code.

Task factories run while the graph is built. Capture only data that remains valid through construction, and allocate tasks with the render allocator. A descriptor holding a direct task rather than a recreatable factory has one-shot ownership semantics, including across descriptor copies.

## Declare task access precisely

Every task implements `queue_family()`, `read_resources()`, `write_resources()`, and `record()`. Each `ResAccessVK` uses the exact graph resource ID plus a compact `RendResUsageVK`. Vulkan access and pipeline-stage masks are derived privately by the graph.

Resource inference tracks the last writer in topological order and adds read-after-write execution edges. It does not infer read-read ordering or reliably express arbitrary write-write phase ordering. Use `prev` whenever ordering is required independently of a read from the prior writer. Undeclared read resource IDs make graph construction fail.

Prefer existing task types such as `RTRunComputePipelineVK` and graphics task implementations before adding a new `RenderTaskVK`. Pipeline-run tasks bind pipeline-owned descriptor sets automatically; higher-level tasks may expose reflected write points when they retain private compiled pipelines. `RTRunComputePipelineVK` defaults to `QueueFamilyVK::COMPUTE`; do not override it to graphics without a concrete queue constraint.

## Use queues and resources safely

Graphics and compute nodes record from queue-specific command pools and submit to their corresponding queues. Execution edges use timeline semaphores between task submissions, including submissions to the same queue. Queue-aware scope translation must never place graphics-only pipeline stages on compute submissions or barriers.

Images and buffers shared across renderer-selected queue families should retain their default queue-family configuration. Their builders then use concurrent sharing when multiple families exist. An explicit exclusive-sharing override requires queue-family ownership transfers, which render-graph resource barriers do not currently emit.

Presentation remains graphics-specific: swap-chain acquire waits attach to the first graphics node, and terminal signaling, presentation layout transition, and the in-flight fence attach to the last graphics node. Preserve a dependency path from compute producers to the final graphics/presentation task.

## Bind runtime resources through the renderer

Register static graph resources in the descriptor when possible. Let the renderer inject frame-dependent resources such as the current swap-chain image. Use its render-graph resource update path for application-provided transient bindings; transient resources are reset after execution.

Graph rebuild frees existing command buffers and waits for the device to become idle. Unregistration defers destruction for the configured frames in flight. Preserve recreatable task factories when a descriptor may be rebuilt.

## Validate

Use the repository wrapper and select the smallest relevant render test filter first:

```sh
python validation.py -t debug -- --gtest_filter='WBERenderGraph*'
```

The wrapper runs the complete unit suite after the focused tests pass. Do not invoke CMake directly or run lint automatically.

## Advanced work

Read [references/system-and-usage.md](references/system-and-usage.md) before changing dependency inference, synchronization, queue behavior, resource binding, graph rebuilding, or presentation integration. It also identifies canonical graphics and deferred-compute examples.
