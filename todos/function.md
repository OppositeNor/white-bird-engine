## Reflection:

- WBE_SERIALIZE_SCRIPT

### WBE_RENDER_TASK

While creating render task, indicate the input and output type of the render task.

There are two options to construct a render pipeline:

1st option: Construct render pipeline statically from code.

2nd option: Use code generation for pipeline setup. Write render tasks in json file.

For example, the final pipeline file should be:

<!-- ```json -->
<!-- { -->
<!--     "render-tasks" : -->
<!--     { -->
<!--         "render_task_1" : -->
<!--         { -->
<!--             "vulkan" : "RenderTaskVK1", -->
<!--             "dx12" : "RenderTaskDX12_1", -->
<!--         }, -->
<!--         "render_task_2" : -->
<!--         { -->
<!--             "vulkan" : "RenderTaskVK2", -->
<!--             "dx12" : "RenderTaskDX12_2", -->
<!--         }, -->
<!--         ... -->
<!--     }, -->
<!--     "pipelines" : -->
<!--     [ -->
<!--         { -->
<!--             "name" : "pipeline_name_1", -->
<!--             "pipeline" : -->
<!--             [ -->
<!--                 "render_task_1", -->
<!--                 "render_task_2", -->
<!--                 ... -->
<!--             ], -->
<!--         }, -->
<!--         { -->
<!--             "name" : "pipeline_name_2", -->
<!--             "pipeline" : -->
<!--             [ -->
<!--                 "render_task_1", -->
<!--                 "render_task_2", -->
<!--                 ... -->
<!--             ], -->
<!--         }, -->
<!--         ... -->
<!--     ], -->
<!-- } -->
<!-- ``` -->

```json
{
    "render_passes" :
    [
        "render_pass_0" :
        {
            "input" : [],
            "output" : "data_1"
        },
        "render_pass_1" :
        {
            "input" :
            [
                "data_1",
                "data_2",
                ...
            ],
            "output" : "data_3"
        },
        "render_pass_2" :
        {
            "input" :
            [
                "data_2",
                "data_3",
                ...
            ],
            "output" : "data_4"
        },
        ...
    ]
}
```

And this should be completely dynamic, which means we could just make up an arbritrary
pipeline after compilation just by adding additional pipelines in the json file.

The pipelines the engine uses should be constructed with non-implemented macros, and
be reflected. Such as:

```cpp
WBE_RENDER_PASSES(
    RENDER_PASS_0(INPUT, OUTPUT, data_1),
    RENDER_PASS_1(INPUT, data_1, data_2,/*, ...,*/ OUTPUT, data_3),
    RENDER_PASS_2(INPUT, data_2, data_3,/*, ...,*/ OUTPUT, data_4),
    // ...
)

```

The render task don't have to be dynamic. Though it would be nice if it could... but
just keep it for now.
