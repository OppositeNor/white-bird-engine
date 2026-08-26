# Render Graph System and Extension Guide

## System boundary

White Bird Engine's Vulkan render graph is the runtime scheduling layer between `RendererVK` and concrete `RenderTaskVK` passes.

The core files are:

- `include/function/render_vk/render_graph_descriptor_vk.hh`
- `include/function/render_vk/render_graph_vk.hh`
- `include/function/render_vk/render_task_vk.hh`
- `src/function/render_vk/render_graph_vk.cpp`
- `docs/renderer_vk/renderer_vk.md`

The render graph is not a generic frame-graph compiler. It builds once from a `RendGraphDescVK`, instantiates concrete `RenderTaskVK` objects, infers resource barriers from declared accesses, and executes nodes in topological order.

## Main types and responsibilities

### `RendGraphDescVK`

High-level descriptor containing:

- `tasks`: `Vector<RendTaskDescVK, RenderAllocator>`
- `resources`: `Vector<RendResVK, RenderAllocator>`

Task list order does not determine execution order. Dependencies are expressed by `RendTaskDescVK::prev` and resolved by the graph.

### `RendTaskDescVK`

Describes one graph node before instantiation:

- `task_id`: stable dependency identifier
- `prev`: upstream task ids that must finish first
- `create`: one-shot factory returning `Unique<RenderTaskVK>`

The direct constructor taking `Unique<RenderTaskVK>` wraps the task in a one-shot factory. Reusing the same descriptor for repeated graph builds is not a safe extension strategy unless you control that one-shot lifetime explicitly.

### `RendResVK`

Graph-visible resource binding:

- `res_id`: lookup label used by tasks and graph descriptor
- `res_type_id`: runtime type id, usually `TypeUUID<T>::UUID`
- `res`: `Ref<RenderObjectVK>` instance

The graph stores resources in a string-keyed map and resolves them through `RendResViewVK`.

### `RenderTaskVK`

Every Vulkan graph task must define four things:

1. `queue_family()`
2. `read_resources()`
3. `write_resources()`
4. `record(VkCommandBuffer, const RendResViewVK&, double)`

The graph never inspects the recorded Vulkan commands to infer hazards. The declarations in `read_resources()` and `write_resources()` are the contract.

## Build and execution flow

`RenderGraphVK::build_from_descriptor()` performs these steps:

1. clear previous nodes, edges, and resource state
2. index resources from `RendGraphDescVK`
3. instantiate concrete tasks from every `RendTaskDescVK`
4. add execution edges from `prev`
5. topologically sort the resulting DAG
6. infer resource edges from task read/write declarations in execution order
7. allocate per-node command buffers
8. allocate one terminal binary semaphore per swap-chain image

`RenderGraphVK::execute()` then:

1. walks nodes in topological order
2. inserts resource transitions before each node based on incoming resource edges
3. records the node's commands into that node's command buffer
4. submits the node to the queue selected by `queue_family()`
5. attaches timeline semaphore waits/signals for execution edges
6. signals a terminal semaphore on the last graphics submission for present interlock

## Resource synchronization model

Resource barriers are inferred from `ResAccessVK` declarations:

- `res_id`
- `access`
- `stage`

Each task must declare every graph-visible read and write that matters for inter-task ordering. If a task uses a resource in `record()` but omits it from these access lists, the graph will not create the necessary resource edge or pipeline barrier.

The resource view passed into `record()` is read-only lookup, not ownership:

```cpp
const RendResVK& resource = p_resources.get("scene_depth_view");
```

Missing ids throw immediately.

## Queue families

`QueueFamilyVK` currently exposes:

- `GRAPHICS`
- `COMPUTE`

The graph maps these logical families to the concrete Vulkan queues and command pools in `BaseRenderDataVK`. Queue selection affects both command-buffer allocation and cross-submit ordering.

When adding a new queue family, update both the public enum and the private queue/pool selection logic in `render_graph_vk.cpp`, then verify every task and submission path that assumes the current two-family model.

## Resource override model

### `set_resource()`

Persistent runtime rebinding. Updates or inserts a graph resource binding and leaves it in place.

### `set_transient_resource()`

Temporary runtime rebinding. Stores the previous value in `transient_resource_defaults` and restores it through `reset_transient_resources()` after the override scope ends.

Use transient overrides for resources whose per-frame identity changes without requiring graph rebuild, such as frame-local images or views. Preserve type consistency with the descriptor-time binding.

## Swap-chain and present integration

The graph allocates one terminal binary semaphore per swap-chain image. The last graphics submission signals the semaphore for the currently acquired image, and presentation waits on it.

The swap-chain image view is represented in graph resources as `REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK`, often with a null resource in the descriptor so the renderer can resolve it at frame time.

## Known usage pattern: static opaque pass

`RTRunStaticOpaqueTrianglesVK` is the main example of descriptor-built graph usage:

1. external attachments are added to `RendGraphDescVK::resources`
2. the task is constructed up front
3. the task is wrapped in `RendTaskDescVK`
4. the graph is registered and activated through `RendererVK`

Use `docs/renderer_vk/renderer_vk.md` as the canonical end-to-end example for graph registration and pass configuration.

## Common failure modes

- `Render graph resource was not bound`: task requested an id not present in the graph resource map.
- resource type mismatch in `set_resource()` or `set_transient_resource()`: runtime override used a different type id than the graph binding.
- cycle detected in render graph: `prev` dependencies created a loop.
- missing barrier behavior: task used a resource without declaring it in `read_resources()` or `write_resources()`.
- wrong queue execution: task returned the wrong `QueueFamilyVK`.

## Extension checklist

1. Start from the owning task or descriptor, not broad renderer exploration.
2. Verify the task's resource declarations match what `record()` actually touches.
3. Keep resource ids stable and descriptive.
4. Prefer adding the smallest explicit `prev` dependency that explains an execution requirement.
5. Use graph-visible resources only for objects that truly cross task boundaries.
6. Validate swap-chain and depth attachments through the graph resource map, not task-private state.
7. Rebuild with `python build.py -t debug` after interface changes.

## Related files

- `include/function/render_vk/rt_run_graphics_pipeline_vk.hh`
- `include/function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh`
- `include/function/render_vk/rt_run_compute_pipeline_vk.hh`
- `include/function/render/render_graph_resource_binding.hh`

Use those task headers when you need concrete task examples for graph access declarations and `record()` contracts.