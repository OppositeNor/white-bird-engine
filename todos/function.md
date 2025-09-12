## Reflection:

- WBE_SERIALIZE_SCRIPT

### WBE_RENDER_TASK

While creating render task, indicate the input and output type of the render task.

There are two options to construct a render pipeline:

1st option: Construct render pipeline statically from code.

2nd option: Use code generation for pipeline setup. Write render tasks in json file.

For example, the final pipeline file should be:

```json
{
    "render-tasks" :
    {
        "render_task_1" :
        {
            "vulkan" : "RenderTaskVK1",
            "dx12" : "RenderTaskDX12_1",
        },
        "render-task_2" :
        {
            "vulkan" : "RenderTaskVK2",
            "dx12" : "RenderTaskDX12_2",
        },
        ...
    },
    "pipelines" :
    [
        {
            "name" : "pipeline-name-1",
            "pipeline" :
            [
                "render_task_1",
                "render_task_2",
                ...
            ],
        },
    ],
}
```

And this should be completely dynamic, which means we could just make up an arbritrary
pipeline after compilation just by adding additional pipelines in the json file.

The pipelines the engine uses should be constructed with non-implemented macros, and
be reflected. Such as:

```cpp
WBE_RENDER_TASK(render_task_1, RenderTaskVK1, RenderTaskDX12_1)  // No semicolons!
WBE_RENDER_TASK(render_task_2, RenderTaskVK2, RenderTaskDX12_2)
// ...
WBE_RENDER_PIPELINE(
    render_task1,
    render_task2,
    // ...
)

```

The render task don't have to be dynamic. Though it would be nice if it could... but
just keep it for now.
