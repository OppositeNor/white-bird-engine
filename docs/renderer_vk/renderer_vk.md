Tick render stages:

- pre-execution tasks: A series of callback functions, each taking a command
  buffer as a parameter. The callbacks record commands into the command buffer,
  which is submitted to the queue before any render-graph work executes in the
  next frame.

- Run render tasks: The active `RenderGraphVK` (built from a
  `RendGraphDescVK` via `RenderGraphVK::build_from_descriptor`) is executed
  in topological order. Each `RenderTaskVK` records into its own primary
  command buffer; cross-submit ordering between tasks is enforced with timeline
  semaphores attached to the graph's explicit execution edges. Compact
  `RendResUsageVK` declarations describe accesses but do not create edges;
  `RendTaskDescVK::after()` must establish a path for every conflicting access pair.
  Vulkan access and stage masks remain graph-internal. After the
  last graphics submission the graph signals a terminal binary semaphore, which
  `RendererVK::present()` waits on before `vkQueuePresentKHR`.

## Material Texture Binding

Material texture semantics are shader metadata, not renderer-wide enums. Integer descriptor-index fields in the global
`MaterialParams` struct declare their texture roles with `WBE_TEXTURE`. An explicit string argument names the role; omitting the
argument uses the annotated field name. `rma` is the canonical role for the roughness, metallic, and ambient-occlusion channel
pack.

The static-set task reflects this contract independently for every graphics pipeline. It resolves each texture to a global
sampled-image descriptor-heap index, initializes missing roles from the configured default material, and writes those indices into
`MaterialParamsBuffer.materials` at the reflected field offsets. The renderer backend does not interpret texture roles.

The contract member names are fixed in `utils/utils.slang`: the reflection-only
`MaterialTextureContractMetadata.material_sampler` and `MaterialTextureContractMetadata.textures` members describe the resource
shape, while `MaterialParamsBuffer.materials` is the runtime material table. Application configuration may vary only the
`material_textures` and `material_params` base variable names and the `default_material` resource key. Materials do not carry a
separate `material_type`; the selected graphics pipeline defines the shader model.

Texture and vertex annotations use one backend-neutral role resolver. Exact role names are authoritative and hashes are only a
matching acceleration, so duplicate names and distinct-name hash collisions are rejected consistently. Separate material and
vertex contract resolvers then apply their domain rules; Vulkan code only translates the validated vertex contract or binds the
validated texture table.

## Descriptor Heap Binding

The Vulkan backend requires `VK_EXT_descriptor_heap`, shader untyped pointers, maintenance5, buffer device address, and memory that
is both host-visible and device-local for heap storage. `DescriptorHeapAllocatorVK` owns one resource heap divided into buffer,
sampled-image, and storage-image arenas plus a separate sampler heap. `ROBufferVK`, `ROImageViewVK`, and `ROSamplerVK` own indices
allocated lazily by their descriptor getters or transferred through their builders, and defer release for the configured frames
in flight.

Shaders represent each resource as an aligned `uint2` field in push data marked with `WBE_DESCRIPTOR("uniform_buffer")`,
`storage_buffer`, `sampled_image`, `storage_image`, or `sampler`. The render graph binds both heaps before each task records, and
`ROPipelineVK` pushes the reflected index block before dispatch or draw. Pipeline binding paths are relative to that push-data
block and may traverse nested structs and fixed-size struct arrays.

Vulkan plugins contribute creation requirements through `RenderPluginVK::on_before_instance_creation()` and
`on_before_device_creation()`. Device-feature requests should use the protected feature helper so plugins merge an existing
`pNext` structure instead of appending duplicate structure types. `on_device_created()` remains the point for loading commands and
finishing plugin setup after logical-device creation.

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

## Static Set Resources

`StaticSetCompilerVK::compile_resources()` compiles opaque and masked geometry into one pipeline-independent
`StaticSetRenderResourcesVK`. It owns shared vertex and index buffers, separate opaque and masked indirect command buffers,
submesh/node/instance tables, material parameters, sampler, and textures. Rendering is performed by the generic
`RTRunGraphicsPipelinesSequentialVK`; callers provide the graphics pipeline when constructing each regular or indirect dispatch.

### Loading Static Geometry

Static geometry resources are loaded through the normal chunk pipeline. `ResourceUnitLoaderBinary` loads the shared `RUBinary`
payload, and `register_renderer_vk_resource_unit_loaders()` registers `ResourceUnitLoaderStaticOpaqueSet` and
`ResourceUnitLoaderStaticMaskedSet` so `static_opaque_set` and `static_masked_set` schemas resolve into `RUStaticOpaqueSet` and
`RUStaticMaskedSet` resources.

Each static set stores:

- A list of submeshes.
- For each submesh: a material, a `RUVertexView`, a `RUIndexView`, and a `[first_instance, instance_count)` slice.
- One shared instance array containing `glm::mat4` transforms.

All drawable submeshes in one compiled static set must reference the same vertex `RUBinary` resource and the same index
`RUBinary` resource. Vertex and index binaries may be distinct from each other. The task validates that each vertex view and
index view stays within its binary bounds before it creates any Vulkan buffers.

### Vertex Assembly And Upload

`StaticSetCompilerVK` does not flatten mesh data through `RenderDatabase`. Instead, compilation follows this path in
`src/function/render_vk/static_set_compiler_vk.cpp`:

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

- `static_set.submeshes`
- `static_set.nodes`
- `static_set.instances`

The task still uses shader reflection for material fallback textures. `get_default_image()` resolves the configured default
material, and `GraphicsPipelineCompilerVK` binds any reflected texture and parameter resources needed by the selected pipeline.
The compiled material image views are also returned as a CPU-side `RenderResourceList`, available through
`CompiledStaticSetVK::acquire_texture_list()`.

The application registers each compiled static-set resource once on `RendGraphDescVK`. Tasks refer to the shared vertex, index,
indirect-command, metadata, material, and texture resources by their graph resource identifiers. A `RenderResourceList` does not
create GPU resources; graph construction retains the list under its aggregate key and registers its entries as
`<resource_list_key>[<index>]`. Declaring one access to the aggregate expands to accesses on all indexed entries, so a task can
declare one sampled-image read on `REND_RES_STATIC_SET_TEXTURES` while graph dependency validation still sees every texture.

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

The indirect command buffer has both `VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT` and `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`. Each
sequential graphics entry selects a contiguous command range and records one `vkCmdDrawIndexedIndirect`; a future compute task can
write the same graph resource as a storage buffer when an explicit execution path orders it before the graphics task.

Before constructing the task:

1. Load the static sets, materials, images, shaders, and graphics pipelines through `ResourceManager`.
2. Create every attachment resource referenced by the task configuration. Bind the renderer's swap-chain image-view container
  explicitly under `REND_RES_SWAP_CHAIN` when the graph renders to the swap chain.
3. Compile one `CompiledStaticSetVK` per set-plus-pipeline combination and order its generic graphics task explicitly with
  `RendTaskDescVK::after()`.

```cpp
RendererVK* renderer = RendererVK::get_singleton();
RenderAllocator* render_allocator = renderer->get_base_render_data()->allocator;
ResourceManager* resource_manager = ResourceManager::get_singleton();

RefRes<RUStaticOpaqueSet> opaque_set = resource_manager->get_resource<RUStaticOpaqueSet>("scene.static_opaque_set");
RefRes<RUGraphicsPipeline> opaque_pipeline = resource_manager->get_resource<RUGraphicsPipeline>("forward_cook_torrance_pbr_static");

StaticSetCompilerVK::Config compiler_config;
compiler_config.color_attachment_formats.push_back(renderer->get_base_render_data()->swap_chain_format);
compiler_config.depth_attachment_format = VK_FORMAT_D32_SFLOAT;
Ref<CompiledStaticSetVK> static_set = StaticSetCompilerVK(renderer).compile(opaque_set, opaque_pipeline, compiler_config);

CompiledStaticSetVK::PassConfig pass_config(render_allocator);
pass_config.render_extent = renderer->get_base_render_data()->swap_chain_extent;
pass_config.color_attachment_ids.emplace_back(REND_RES_SWAP_CHAIN);
pass_config.depth_attachment_id = "scene_depth_view";
pass_config.clear_values.resize(2);
RendTaskDescVK task_desc(render_allocator,
    "run_static_set",
    static_set->create_render_task(renderer->get_base_render_data(), pass_config));
static_set->configure_task(task_desc);

RendGraphDescVK graph_desc(render_allocator);
graph_desc
   .add_resource(REND_RES_SWAP_CHAIN, renderer->acquire_swap_chain_image_view_container())
   .add_resource("scene_depth_view", depth_image_view)
   .add_task(std::move(task_desc));

renderer->register_render_graph("scene", std::move(graph_desc));
renderer->activate_render_graph("scene");
```

The application contributes all graph-visible attachments before registration. The renderer does not add or replace descriptor resources while
building the graph. The task retains its compiled Vulkan objects directly, and the `RendTaskDescVK` move into the graph remains one-shot.

### Configuring Explicit Passes

- `add_color_attachment()` adds color attachments in rendering order. With no explicit color attachment, the graphics pipeline and
  dynamic-rendering task use zero color attachments.
- `set_depth_attachment()` requires both a graph resource id and its Vulkan format. `set_render_extent()` overrides the swap-chain
  extent.
- `add_clear_value()` follows attachment order: all color values first, then depth. Missing color values default to opaque black;
  when depth is configured and only the color values are supplied, depth defaults to `0.0` and stencil to `0`.
- `set_color_attachment_load_op()` and `set_depth_attachment_load_op()` control how a follow-up task in the same logical pass
  reuses attachments produced by an earlier task.
- Task ids, resource usages, and dependencies belong to `RendTaskDescVK`, not the task configuration.

### Static Geometry Updates

`RendererVK::update_static_geometry_sets()` no longer rebuilds raster draw tasks or flatten `RUMesh3D` data. Raster passes stay
application-owned through compiled static sets and generic graphics tasks. The renderer-side update path exists only for derived shared state
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

### Replacing Pipeline Descriptor Bindings

Material and static-set bindings are populated during construction. An application may replace a reflected binding on the task's
compiled pipeline by field path:

```cpp
task->set_descriptor_binding("camera_info", camera_buffer);
task->set_descriptor_binding("light.lights.shadowmap", shadow_depth_view);
```

The resource type must match the field's `WBE_DESCRIPTOR` category. Buffers bind as complete `ROBufferVK` objects; image fields
take `ROImageViewVK`, and sampler fields take `ROSamplerVK`. The pipeline retains directly bound resources for its lifetime.

`ROCMultislotsVK` is a general container for arbitrary slot-indexed resources and is widely used for frame-local resources. Pass
the container instead of resolving one frame slot manually:

```cpp
task->set_descriptor_binding("camera_info", camera_buffers);
camera_buffers->publish_buffer_update(&camera_info, 0, sizeof(camera_info));
```

At record time, the pipeline resolves the container from `current_frame_slot` and pushes that resource's descriptor index.
Publish buffer contents and replace bindings on the rendering thread; callers on other threads must use `IRenderer::add_job()`.
Field lookup comes from shader reflection and throws when the requested path is absent or the resource type is incompatible.

To add an execution dependency, construct the descriptor explicitly before moving it into the graph:

```cpp
RendTaskDescVK task_desc(render_allocator, "run_gbuffer", std::move(task));
task_desc.after("run_shadow")
    .read("shadow_moments", RendResUsageVK::SAMPLED_IMAGE)
    .write("gbuffer_albedo", RendResUsageVK::COLOR_ATTACHMENT);
graph_desc.tasks.emplace_back(std::move(task_desc));
```

Compute and graphics pipeline-run tasks push their pipeline-owned descriptor-index blocks automatically after the graph binds the
global heaps. Shader resources that are not intrinsic task inputs are declared fluently on the task descriptor. These declarations
validate explicit dependency paths and narrow queue-aware synchronization scopes; they do not create execution edges:

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
