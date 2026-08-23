Tick render stages:

- pre-execution tasks: A series of callback functions, each taking a command
  buffer as a parameter. The callbacks record commands into the command buffer,
  which is submitted to the queue before any render-graph work executes in the
  next frame.

- Run render tasks: The active `RenderGraphVK` (built from a
  `RendGraphDescVK` via `RenderGraphVK::build_from_descriptor`) is executed
  in topological order. Each `RenderTaskVK` records into its own primary
  command buffer; cross-submit ordering between tasks is enforced with timeline
  semaphores attached to the graph's execution edges, and per-resource hazards
  are resolved with `vkCmdPipelineBarrier2` driven by each task's declared
  `read_resources()` / `write_resources()` access and stage masks. After the
  last graphics submission the graph signals a terminal binary semaphore, which
  `RendererVK::present()` waits on before `vkQueuePresentKHR`.

## Material Texture Binding

Material texture semantics are shader metadata, not renderer-wide enums. Integer descriptor-index fields in the global
`MaterialParams` struct declare their texture roles with `WBE_TEXTURE`. An explicit string argument names the role; omitting the
argument uses the annotated field name. `rma` is the canonical role for the roughness, metallic, and ambient-occlusion channel
pack.

The static opaque pipeline reflects this contract independently for every graphics pipeline batch. It builds that batch's
bindless `MaterialTextures.textures` descriptor array, initializes missing slots from the configured default material, and writes
indices into `MaterialParamsBuffer.materials` at the reflected field offsets. The renderer backend does not interpret texture
roles.

The nested contract member names are fixed in `defs.slang`: `MaterialTextures.material_sampler`, `MaterialTextures.textures`, and
`MaterialParamsBuffer.materials`. Application configuration may vary only the `material_textures` and `material_params` base
variable names and the `default_material` resource key. Materials do not carry a separate `material_type`; the selected graphics
pipeline defines the shader model.
