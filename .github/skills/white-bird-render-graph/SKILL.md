---
name: white-bird-render-graph
description: Use and extend White Bird Engine's Vulkan render graph feature. Apply when building a RendGraphDescVK, wiring RenderTaskVK nodes and prev dependencies, binding graph resources, adding transient resource overrides, diagnosing resource barriers, queue-family scheduling, or present/swap-chain graph integration.
---

# White Bird Render Graph

Use this skill when work touches the Vulkan render graph layer rather than one isolated render task. The render graph owns task instantiation, resource lookup, execution ordering, resource hazard barriers, queue-family submission, and swap-chain interlock.

## When to use

- Build or modify a `RendGraphDescVK`.
- Add a new `RenderTaskVK` to a graph.
- Wire `prev` dependencies between passes.
- Bind graph resources by `res_id`.
- Use `set_resource()` or `set_transient_resource()`.
- Diagnose missing graph resources, bad task ordering, or barrier issues.
- Integrate a pass with swap-chain color or depth attachments.

Do not use this skill for shader metadata contracts, material role resolution, or vertex-input reflection unless the task also changes how a render task participates in the graph.

## Core model

- `RendGraphDescVK` is the high-level graph descriptor: it owns `tasks` and `resources`.
- `RendTaskDescVK` names one task with a `task_id`, an optional `prev` dependency list, and a one-shot `create` factory or transferred `Unique<RenderTaskVK>`.
- `RendResVK` binds one graph-visible resource under a string `res_id` plus a runtime type id.
- `RenderTaskVK` declares `queue_family()`, `read_resources()`, `write_resources()`, and `record()`.
- `RenderGraphVK::build_from_descriptor()` instantiates tasks, creates execution edges from `prev`, infers additional execution edges from declared accesses, topologically sorts the graph, and allocates command buffers.

## Procedure

1. Decide whether you are changing the graph descriptor, one render task, or both.
2. For a new task, implement or inspect its `RenderTaskVK` contract first:
   - return the correct `QueueFamilyVK`
   - declare every graph-visible read in `read_resources()`
   - declare every graph-visible write in `write_resources()`
   - resolve runtime resources from `RendResViewVK` inside `record()`
3. Add the task to a `RendGraphDescVK` using `RendTaskDescVK` and stable `task_id` values.
4. Add explicit ordering with `prev` for execution dependencies that are not guaranteed by last-writer inference alone, especially write-after-write or phase boundaries.
5. Add every externally supplied resource to `graph_desc.resources` with the correct `res_id` and `TypeUUID<T>::UUID`.
6. For per-frame or temporary rebinding, use `RenderGraphVK::set_transient_resource()`; use `set_resource()` for persistent overrides.
7. Register and activate the graph through `RendererVK` after the descriptor is complete.

## Common patterns

### Descriptor-built graph

```cpp
RendGraphDescVK graph_desc(render_allocator);

graph_desc
    .add_resource(REND_RES_SWAP_CHAIN_IMAGE,
        TypeUUID<ROImageViewVK>::UUID,
        Ref<RenderObjectContainerVK>(MEM_NULL))
    .add_task(RT_RUN_STATIC_OPAQUE, std::move(task));

renderer->register_render_graph("scene", std::move(graph_desc));
renderer->activate_render_graph("scene");
```

### Task contract

Declare only the resources the graph must synchronize. The graph adds execution edges from these declarations and never infers hazards from Vulkan command inspection.

### Transient overrides

`set_transient_resource()` temporarily replaces a bound resource until the next reset path after graph execution. Use it for per-frame images or views whose identity changes without rebuilding the graph descriptor.

## Important invariants

- Resource ids must match exactly between the graph descriptor, task access declarations, and `RendResViewVK::get()` lookups.
- Resource type ids must remain consistent across `resources`, `set_resource()`, and `set_transient_resource()`.
- Missing resources fail at lookup time with `Render graph resource was not bound`.
- Cycles in `prev` dependencies fail during topological sort.
- Dependency inference only knows about resources declared in `read_resources()` and `write_resources()`.
- `RendTaskDescVK` direct-task construction is one-shot; build the descriptor once, then register the graph.
- Queue-family selection matters because the graph maps it to Vulkan queues and command pools.

## Before changing support

Read [references/system-and-extension.md](./references/system-and-extension.md) before changing graph construction, dependency inference, transient-resource semantics, command-buffer allocation, queue-family submission, or present interlock. Trace one real task from descriptor entry to `record()` before editing the graph core.

## Validate

Build with the repository wrapper:

```sh
python build.py -t debug
```

For render-graph changes, prefer a focused validation run that exercises the touched render task or graph path first, then let the wrapper complete the full suite when appropriate:

```sh
python validation.py -t debug
```

Do not run lint automatically.
