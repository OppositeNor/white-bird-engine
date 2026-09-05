Tick render stages:

- pre-execution tasks: A series of callback functions, each taking a command
  buffer as a parameter. The callbacks record commands into the command buffer,
  which is submitted to the queue before any render-graph work executes in the
  next frame.

- Run render tasks: The active `RenderGraphVK` (built from a
  `RendGraphDescVK` via `RenderGraphVK::build_from_descriptor`) is executed
  in topological order. Each `RenderTaskVK` records into its own primary
  command buffer; cross-submit ordering between tasks is enforced with timeline
  semaphores attached to the graph's execution edges. Per-resource ordering is
  derived from compact `RendResUsageVK` declarations on the task and its
  `RendTaskDescVK`; Vulkan access and stage masks remain graph-internal. After the
  last graphics submission the graph signals a terminal binary semaphore, which
  `RendererVK::present()` waits on before `vkQueuePresentKHR`.

## Material Texture Binding

Material texture semantics are shader metadata, not renderer-wide enums. Integer descriptor-index fields in the global
`MaterialParams` struct declare their texture roles with `WBE_TEXTURE`. An explicit string argument names the role; omitting the
argument uses the annotated field name. `rma` is the canonical role for the roughness, metallic, and ambient-occlusion channel
pack.

The static-set task reflects this contract independently for every graphics pipeline batch. It builds that batch's
bindless `MaterialTextures.textures` descriptor array, initializes missing slots from the configured default material, and writes
indices into `MaterialParamsBuffer.materials` at the reflected field offsets. The renderer backend does not interpret texture
roles.

The nested contract member names are fixed in `utils/utils.slang`: `MaterialTextures.material_sampler`, `MaterialTextures.textures`, and
`MaterialParamsBuffer.materials`. Application configuration may vary only the `material_textures` and `material_params` base
variable names and the `default_material` resource key. Materials do not carry a separate `material_type`; the selected graphics
pipeline defines the shader model.

Texture and vertex annotations use one backend-neutral role resolver. Exact role names are authoritative and hashes are only a
matching acceleration, so duplicate names and distinct-name hash collisions are rejected consistently. Separate material and
vertex contract resolvers then apply their domain rules; Vulkan code only translates the validated vertex contract or binds the
validated texture table.

## Vertex Resource Layout

Mesh and static-geometry sidecar files are represented by `RUBinary` resources. Submeshes store `vertices` and `indices` views
into those binaries rather than owning copied geometry arrays. Vertex views are already tightly interleaved; every attribute
supplies an open string role, a byte offset within the stride, and a reflected scalar/vector type. Index views contain contiguous
`uint32` indices.

Supported attribute types are `float`, `vec2`, `vec3`, `vec4`, `int`, `ivec2`, `ivec3`, `ivec4`, `uint`, `uvec2`, `uvec3`, and
`uvec4`. The resulting attribute offsets, byte sizes, stride, and vertex count are fixed after loading. Attribute values may be
changed through `RUSubmesh3D`, but mutation is not thread-safe. Publish the updated `RUStaticOpaqueSet` or `RUStaticMaskedSet`
through the scene/render database and call `RendererVK::update_static_geometry_sets()` only when derived geometry state such as
ray tracing must be rebuilt.

Static geometry resources use `RUStaticGeometrySet`, with distinct `RUStaticOpaqueSet` and `RUStaticMaskedSet` resource types.
Each set stores declared submeshes and shared instance transforms. A submesh references a material, a vertex view, an index view,
and a `[first_instance, instance_count)` range into the set's transform array. Instance transforms are exactly 16 column-major
floats in the serialized resource and are loaded as `glm::mat4`.

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

## Static Set Tasks

`RTRunStaticSetVK` is the current Vulkan path for static geometry. One task instance consumes exactly one explicit
`RUStaticGeometrySet` resource and one explicit `RUGraphicsPipeline` resource. If a pass needs separate opaque and masked
pipelines, build two tasks and order them in the render graph. The second task should normally use `VK_ATTACHMENT_LOAD_OP_LOAD`
for the attachments written by the first task.

### Loading Static Geometry

Static geometry resources are loaded through the normal chunk pipeline. `ResourceUnitLoaderBinary` loads the shared `RUBinary`
payload, and `register_renderer_vk_resource_unit_loaders()` registers `ResourceUnitLoaderStaticOpaqueSet` and
`ResourceUnitLoaderStaticMaskedSet` so `static_opaque_set` and `static_masked_set` schemas resolve into `RUStaticOpaqueSet` and
`RUStaticMaskedSet` resources.

Each static set stores:

- A list of submeshes.
- For each submesh: a material, a `RUVertexView`, a `RUIndexView`, and a `[first_instance, instance_count)` slice.
- One shared instance array containing `glm::mat4` transforms.

All drawable submeshes in one `RTRunStaticSetVK` task must reference the same vertex `RUBinary` resource and the same index
`RUBinary` resource. Vertex and index binaries may be distinct from each other. The task validates that each vertex view and
index view stays within its binary bounds before it creates any Vulkan buffers.

### Vertex Assembly And Upload

`RTRunStaticSetVK` does not flatten mesh data through `RenderDatabase`. Instead, construction follows this path in
`src/function/render_vk/rt_run_static_set_vk.cpp`:

1. `StaticSetBufferCompilerVK` verifies submesh storage, vertex stride, vertex byte ranges, index ranges, and instance ranges. It
  also enforces that every drawable submesh shares one vertex binary resource and one index binary resource.
2. `build_static_binary_buffer()` uploads the vertex binary into an `ROBufferVK` with the effective vertex usage flags, and uploads
  the index binary into a separate `ROBufferVK` with the effective index usage flags.
3. Each upload is cached in `RenderDatabase::register_compiled_render_resource()` under the binary resource identity plus Vulkan
  usage flags, so later tasks and renderer plugins can reuse compatible buffers without re-uploading identical geometry bytes.
4. `build_static_instance_buffer_data()` assembles three CPU-side tables from the set declaration:
  `StaticSetSubmeshGPU`, `StaticSetNodeGPU`, and `StaticSetInstanceGPU`.
5. `build_storage_buffer()` uploads those tables as storage buffers for submesh metadata, node transforms, and per-instance draw
  indirection.

The important distinction is that vertex and index bytes stay in their original binary layouts. The task uploads those binaries
as-is into separate buffers, then uploads side-band metadata buffers that tell the shader which submesh and instance transform to
use for each indirect draw.

### Feeding The Graphics Pipeline

After buffer creation, `compile_static_set_pipeline()` compiles the selected graphics pipeline once for the task and binds three
named storage buffers into the reflected shader contract:

- `static_submeshes`
- `static_nodes`
- `static_instances`

The task still uses shader reflection for material fallback textures. `get_default_image()` resolves the configured default
material, and `GraphicsPipelineCompilerVK` binds any reflected texture and parameter resources needed by the selected pipeline.

`StaticSetDrawBatchPlannerVK::build()` groups drawable submeshes for the task's selected pipeline by effective reflected vertex
layout. Matching layouts can share a batch when their vertex-view offsets have the same remainder modulo stride and their
relative vertex offsets fit `int32_t`. Groups follow first appearance and retain source order within each group. Each group
binds its minimum vertex-view byte offset, and is split at the device's `maxDrawIndirectCount` limit. Each submesh retains one
`VkDrawIndexedIndirectCommand`; each batch submits several commands through one `vkCmdDrawIndexedIndirect` call. Commands store:

- `indexCount` from the submesh's index view.
- `firstIndex` from the index-view byte offset divided by `sizeof(uint32_t)`.
- `vertexOffset` from the vertex-view byte offset relative to its batch's binding offset, divided by its stride.
- `firstInstance` from the precomputed first-instance table.
- `instanceCount` from the submesh's instance count.

`build_indirect_buffer()` uploads that command array, and `build_non_empty_indirect_task()` packages the geometry buffer, indirect
buffer, pipeline batch, attachment ids, clear values, and load ops into one `RTRunGraphicsPipelineIndirectVK`. During recording,
`RTRunStaticSetVK::record()` delegates to that indirect task, which binds the compiled graphics pipeline, binds the cached binary
buffer as vertex and index input, binds the pipeline-owned descriptor sets, and issues the indirect indexed draws.

Before constructing the task:

1. Load the static sets, materials, images, shaders, and graphics pipelines through `ResourceManager`.
2. Create every attachment resource referenced by the task configuration. The swap-chain image view is represented in the graph by
  a `REND_RES_SWAP_CHAIN_IMAGE` resource with a null object; the renderer resolves it for each frame.
3. Construct one `RTRunStaticSetVK` per set-plus-pipeline combination and order them explicitly with `RendTaskDescVK::after()`.

```cpp
RendererVK* renderer = RendererVK::get_singleton();
RenderAllocator* render_allocator = renderer->get_base_render_data()->allocator;
ResourceManager* resource_manager = ResourceManager::get_singleton();

RefRes<RUStaticOpaqueSet> opaque_set = resource_manager->get_resource<RUStaticOpaqueSet>("scene.static_opaque_set");
RefRes<RUGraphicsPipeline> opaque_pipeline = resource_manager->get_resource<RUGraphicsPipeline>("forward_cook_torrance_pbr_static");

RTRunStaticSetVK::Config config;
config.set_depth_attachment("scene_depth_view", VK_FORMAT_D32_SFLOAT);
auto task = make_unique<RTRunStaticSetVK>(render_allocator, renderer, opaque_set, opaque_pipeline, config);

RendGraphDescVK graph_desc(render_allocator);
graph_desc
   .add_resource(REND_RES_SWAP_CHAIN_IMAGE,
      TypeUUID<ROImageViewVK>::UUID,
      Ref<RenderObjectContainerVK>(MEM_NULL))
   .add_resource("scene_depth_view", TypeUUID<ROImageViewVK>::UUID, depth_image_view)
   .add_task(RT_RUN_STATIC_SET, std::move(task));

renderer->register_render_graph("scene", std::move(graph_desc));
renderer->activate_render_graph("scene");
```

The application contributes only graph-visible attachments and any later descriptor writes. The task retains its compiled Vulkan
objects directly, and the `RendTaskDescVK` move into the graph remains one-shot.

### Configuring Explicit Passes

- `add_color_attachment()` adds color attachments in rendering order. With no explicit color attachment, the swap-chain image view
  and format are used.
- `set_depth_attachment()` requires both a graph resource id and its Vulkan format. `set_render_extent()` overrides the swap-chain
  extent.
- `add_clear_value()` follows attachment order: all color values first, then depth. Missing color values default to opaque black;
  when depth is configured and only the color values are supplied, depth defaults to `0.0` and stencil to `0`.
- `set_color_attachment_load_op()` and `set_depth_attachment_load_op()` control how a follow-up task in the same logical pass
  reuses attachments produced by an earlier task.
- Task ids, resource usages, and dependencies belong to `RendTaskDescVK`, not the task configuration.

### Static Geometry Updates

`RendererVK::update_static_geometry_sets()` no longer rebuilds raster draw tasks or flatten `RUMesh3D` data. Raster passes stay
application-owned through explicit `RTRunStaticSetVK` tasks. The renderer-side update path exists only for derived shared state
that is keyed by the published static sets.

The current opaque update flow is:

1. `Scene::update()` publishes `RUStaticOpaqueSet` and `RUStaticMaskedSet` references into `RenderDatabase`, including null clears.
2. `RendererVK::update_static_geometry_sets()` reads the published opaque set and asks each enabled Vulkan plugin for any extra
  shared-buffer usage bits.
3. `StaticSetBufferCompilerVK` validates the set's shared binaries, caches vertex/index uploads by binary identity plus effective
  Vulkan usage flags, and returns reusable `ROBufferVK` objects.
4. The renderer invokes `RenderPluginVK::on_static_opaque_set_updated()` once with the explicit set and the shared vertex/index
  buffers.
5. `RayTracingPluginVK` builds one BLAS per drawable submesh and one TLAS instance per published transform, then publishes the
  resulting `VkAccelerationStructureKHR` through `BaseRenderDataVK::static_opaque_acceleration_structure`.

Null, empty, and fully skipped sets clear the published acceleration structure without zero-sized allocations.

### Writing Pipeline-Owned Descriptors

Material descriptors are populated during construction. Static-set tasks may still expose reflected descriptor fields for
application-provided uniform or sampled resources, but the descriptor objects remain pipeline-owned.

For `RTRunStaticSetVK`, field lookup is per-task because there is exactly one compiled pipeline:

```cpp
auto [set_index, binding] = task->get_descriptor_binding("camera_info");
task->write_descriptor_set(set_index,
   RODescriptorSetVK::Writer()
      .set_binding(binding)
      .add_uniform_buffer_info(camera_buffer, 0, sizeof(CameraInfoVKUBO)));
```

For frame-local buffers, pass the `ROCPerFrameVK` container instead of resolving one buffer manually. Pipeline descriptor writes are
versioned per frame slot, and the container retains the latest published bytes until every slot has applied them:

```cpp
task->write_descriptor_set(batch_index,
    set_index,
    RODescriptorSetVK::Writer()
        .set_binding(binding)
        .add_uniform_buffer_info(camera_buffers, 0, sizeof(CameraInfoVKUBO)));

camera_buffers->publish_buffer_update(&camera_info, 0, sizeof(camera_info));
```

Publish buffer contents from a render-thread job. The selected slot applies the newest pending bytes after its fence has completed,
before its pipeline descriptor set is bound. A slot may skip intermediate versions, but it cannot miss the latest version.

Field paths come from shader reflection. `get_descriptor_binding()` throws when the requested field is absent from a batch, so all
pipelines selected for a pass must expose the fields that the pass writes.

To add an execution dependency, construct the descriptor explicitly before moving it into the graph:

```cpp
RendTaskDescVK task_desc(render_allocator, "run_gbuffer", std::move(task));
task_desc.after("run_shadow")
    .read("shadow_moments", RendResUsageVK::SAMPLED_IMAGE)
    .write("gbuffer_albedo", RendResUsageVK::COLOR_ATTACHMENT);
graph_desc.tasks.emplace_back(std::move(task_desc));
```

Compute and graphics pipeline-run tasks bind their pipeline-owned descriptor sets automatically. Shader resources that are not
intrinsic task inputs are declared fluently on the task descriptor, and the graph derives queue-aware synchronization scopes from
their compact usages:

```cpp
RendTaskDescVK lighting(render_allocator);
lighting.task_id = "run_lighting";
lighting.after("run_gbuffer")
    .read("gbuffer_albedo", RendResUsageVK::SAMPLED_IMAGE)
    .read("gbuffer_depth", RendResUsageVK::SAMPLED_IMAGE)
    .write("lighting_output", RendResUsageVK::STORAGE_IMAGE);
lighting.create = [render_allocator, render_data]() -> Unique<RenderTaskVK> {
    return make_unique<RTRunComputePipelineVK>(render_allocator,
        render_data,
        "lighting_pipeline",
        VkExtent3D{.width = 160, .height = 90, .depth = 1});
};
```
