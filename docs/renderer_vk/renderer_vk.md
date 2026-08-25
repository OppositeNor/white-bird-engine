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

The static opaque task reflects this contract independently for every graphics pipeline batch. It builds that batch's
bindless `MaterialTextures.textures` descriptor array, initializes missing slots from the configured default material, and writes
indices into `MaterialParamsBuffer.materials` at the reflected field offsets. The renderer backend does not interpret texture
roles.

The nested contract member names are fixed in `defs.slang`: `MaterialTextures.material_sampler`, `MaterialTextures.textures`, and
`MaterialParamsBuffer.materials`. Application configuration may vary only the `material_textures` and `material_params` base
variable names and the `default_material` resource key. Materials do not carry a separate `material_type`; the selected graphics
pipeline defines the shader model.

Texture and vertex annotations use one backend-neutral role resolver. Exact role names are authoritative and hashes are only a
matching acceleration, so duplicate names and distinct-name hash collisions are rejected consistently. Separate material and
vertex contract resolvers then apply their domain rules; Vulkan code only translates the validated vertex contract or binds the
validated texture table.

## Vertex Resource Layout

Mesh sidecar files keep geometry attributes as separate arrays. When a submesh resource is loaded, its non-index
`geometry_sections` are packed in declaration order into one tightly interleaved vertex buffer. Every section supplies an open
string role, and every section must contain the same number of elements. Indices remain a contiguous `uint32` array.

Supported attribute types are `float`, `vec2`, `vec3`, `vec4`, `int`, `ivec2`, `ivec3`, `ivec4`, `uint`, `uvec2`, `uvec3`, and
`uvec4`. The resulting attribute offsets, byte sizes, stride, and vertex count are fixed after loading. Attribute values may be
changed through `RUSubmesh3D`, but mutation is not thread-safe. Call `RendererVK::update_static_opaque_triangles()` after a
mutation to rebuild and upload the current interleaved data before subsequent draws.

Vertex shader fields opt into resource matching with `WBE_VERTEX_INPUT`. An explicit string selects a resource role, while an
omitted argument uses the annotated field name:

```slang
struct VertexInput
{
    [WBE_VERTEX_INPUT("position")]
    float3 object_position;

    [WBE_VERTEX_INPUT]
    float2 uv;
};
```

The selected vertex entry point must contain at most one annotation per field, role, and reflected location. Every annotated
shader role must exist in the resource with the same scalar kind and vector width; unconsumed resource attributes are allowed.
Locations come from the shader's reflected varying-input bindings rather than fixed engine locations.

The Vulkan backend requires `VK_EXT_vertex_input_dynamic_state` and its `vertexInputDynamicState` feature. It supplies binding
zero's stride and the matched formats, locations, and offsets immediately before each draw batch. Physical devices without the
extension and feature are rejected; there is no static vertex-input fallback. Direct indexed graphics tasks receive their
reflected vertex-input description and vertex-buffer byte offset at construction, while vertex-less draws use an empty dynamic
state.

## Static Opaque Triangles Task

`RTRunStaticOpaqueTrianglesVK` compiles the render database's static opaque mesh into one indexed-indirect draw task. Construction
compiles one logical Vulkan graphics pipeline per pipeline and attachment configuration, creates material descriptor data for each
pipeline batch, and uploads the indirect commands. Flattened vertex data remains raw interleaved bytes. Geometry is merged only
when its pipeline, material, and vertex layout match; command batches split whenever the layout or vertex-buffer byte offset
changes. The task owns the compiled pipelines, descriptor sets, and indirect-command buffer.

Before constructing the task:

1. Load the mesh, materials, images, shaders, and graphics pipelines through `ResourceManager`.
2. Store the mesh in `RenderDatabase` with `set_static_opaque_triangles()`.
3. Call `RendererVK::update_static_opaque_triangles()` so the database contains the flattened vertices, indices, materials, and
   graphics-pipeline batches consumed by the task.
4. Create every attachment resource referenced by the task configuration. The swap-chain image view is represented in the
   graph by a `REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK` resource with a null object; the renderer resolves it for each frame.

The default configuration draws to the swap chain at the swap-chain extent. It uses the graphics pipeline attached to each mesh
material, the `default_material` fallback resource, and the `material_textures` and `material_params` shader variable names.

```cpp
RendererVK* renderer = RendererVK::get_singleton();
RenderAllocator* render_allocator = renderer->get_base_render_data()->allocator;

RefRes<RUMesh3D> mesh = ResourceManager::get_singleton()->get_resource<RUMesh3D>("scene.mesh");
renderer->get_render_database()->set_static_opaque_triangles(mesh.get());
renderer->update_static_opaque_triangles();

RTRunStaticOpaqueTrianglesVK::Config config;
config.set_depth_attachment("scene_depth_view", VK_FORMAT_D32_SFLOAT);
auto task = make_unique<RTRunStaticOpaqueTrianglesVK>(render_allocator, renderer, config);

RendGraphDescVK graph_desc(render_allocator);
graph_desc.resources.emplace_back(RendResVK{
    .res_id = std::string(REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK),
    .res_type_id = TypeUUID<ROImageViewVK>::UUID,
    .res = MEM_NULL,
});
graph_desc.resources.emplace_back(RendResVK{
    .res_id = "scene_depth_view",
    .res_type_id = TypeUUID<ROImageViewVK>::UUID,
    .res = depth_image_view,
});
graph_desc.tasks.emplace_back(render_allocator, RT_RUN_STATIC_OPAQUE_TRIANGLES_INDIRECT_VK, std::move(task));

renderer->register_render_graph("scene", std::move(graph_desc));
renderer->activate_render_graph("scene");
```

The application adds only the color and depth attachment resources named in `Config`; the task retains its private rendering
resources directly. The direct-task `RendTaskDescVK` constructor transfers unique ownership when the graph is built. Its factory is
one-shot, matching the renderer's one-time descriptor build during `register_render_graph()`.

### Configuring Passes

- `set_graphics_pipeline_resource_key()` replaces every material pipeline with one resource. When set, it takes precedence over
  per-pipeline overrides.
- `add_graphics_pipeline_override()` replaces only batches whose source pipeline key matches. Unmatched batches keep their material
  pipeline.
- `add_color_attachment()` adds color attachments in rendering order. With no explicit color attachment, the swap-chain image view
  and format are used.
- `set_depth_attachment()` requires both a graph resource id and its Vulkan format. `set_render_extent()` overrides the swap-chain
  extent.
- `add_clear_value()` follows attachment order: all color values first, then depth. Missing color values default to opaque black;
  when depth is configured and only the color values are supplied, depth defaults to `1.0` and stencil to `0`.
- Task ids and dependencies belong to `RendTaskDescVK`, not the task configuration.

For a shadow, G-buffer, or other replacement pass, set a single pipeline resource and the pass attachments:

```cpp
RTRunStaticOpaqueTrianglesVK::Config config;
config.set_graphics_pipeline_resource_key("deferred_gbuffer")
    .add_color_attachment("gbuffer_albedo_view", VK_FORMAT_R8G8B8A8_SRGB)
    .add_color_attachment("gbuffer_normal_view", VK_FORMAT_R16G16B16A16_SFLOAT)
    .set_depth_attachment("gbuffer_depth_view", VK_FORMAT_D32_SFLOAT);
```

### Writing Application Descriptors

Material descriptors are populated during construction. Other shader parameters, such as camera or light buffers, can be written
before the task is transferred into its descriptor. Descriptor locations are reflected per compiled batch, so update every batch:

```cpp
for (size_t batch_index = 0; batch_index < task->get_compiled_batch_count(); ++batch_index) {
    auto [set_index, binding] = task->get_descriptor_binding(batch_index, "camera_info");
    task->get_descriptor_set(batch_index, set_index)
        ->write_descriptor_set(RODescriptorSetVK::Writer()
                .set_binding(binding)
                .add_uniform_buffer_info(camera_buffer, 0, sizeof(CameraInfoVKUBO)));
}
```

Field paths come from shader reflection. `get_descriptor_binding()` throws when the requested field is absent from a batch, so all
pipelines selected for a pass must expose the application-managed fields that the pass writes.

To add an execution dependency, construct the descriptor explicitly before moving it into the graph:

```cpp
RendTaskDescVK task_desc(render_allocator, RT_RUN_GBUFFER, std::move(task));
task_desc.prev.emplace_back(RT_RUN_SHADOW);
graph_desc.tasks.emplace_back(std::move(task_desc));
```
