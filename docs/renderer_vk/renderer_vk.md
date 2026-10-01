Tick render stages:

- pre-execution tasks: A series of callback functions, each taking a command
  buffer as a parameter. The callbacks record commands into the command buffer,
  which is submitted to the queue before any render-graph work executes in the
  next frame.

- Run render tasks: The active `RenderGraphVK` (built from a
  `RendGraphDescVK` via `RenderGraphVK::build_from_descriptor`) is executed
  in topological order. Each `IRenderTaskVK` records into its own primary
  command buffer. The graph is a DAG built from resource dependency edges
  declared with `RendTaskDescVK::connect()`; edges between tasks on the same
  queue become pipeline barriers, and edges crossing queues use timeline
  semaphores. Compact `RendResUsageVK` declarations describe accesses but do not
  create edges; every conflicting access pair on a resource must be ordered by a
  chain of edges on that resource. After the
  last graphics submission the graph signals a terminal binary semaphore, which
  `RendererVK::present()` waits on before `vkQueuePresentKHR`.

## Material Texture Binding

Material texture semantics are shader metadata, not renderer-wide enums. Integer descriptor-index fields in the global
`MaterialParams` struct declare their texture roles with `WBE_TEXTURE`. An explicit string argument names the role; omitting the
argument uses the annotated field name. `rma` is the canonical role for the roughness, metallic, and ambient-occlusion channel
pack.

The static-mesh task reflects this contract independently for every graphics pipeline. It resolves each texture to a global
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

## Device Memory

`ROBufferVK` and `ROImageVK` bind to a `DeviceMemoryVK`, which owns one VMA allocation. Their ordinary `Builder` creates the
resource, allocates a `Ref<DeviceMemoryVK>` for its requirements, and binds it through `DeviceMemoryVK::get()`. An `AliasBuilder`
instead takes an existing `Ref<DeviceMemoryVK>` and an offset and creates the resource through `DeviceMemoryVK::create_buffer()` or
`create_image()`. Those check the resource's memory requirements (from its create info) against the memory type, alignment, and
remaining size before anything is created, then use VMA's `vmaCreateAliasingBuffer2()` / `vmaCreateAliasingImage2()` to create and
bind atomically. Every bound resource holds a reference to its memory, so the reference count of the `Ref` tracks its users. Aliasing resources share
their contents, so their use must be synchronized. Memory for several aliases can be allocated from explicit requirements with
`DeviceMemoryVK::Builder::set_requirements()`, or taken from an existing resource with `acquire_memory()`.

Every resource retains its memory until its own delayed destruction, and the allocation is freed when the last reference is
released. Swap-chain and libktx images do not use `DeviceMemoryVK`. The render graph does not alias resources.

## Descriptor Heap Binding

The Vulkan backend requires `VK_EXT_descriptor_heap`, shader untyped pointers, maintenance5, buffer device address, and memory that
is both host-visible and device-local for heap storage. `DescriptorHeapAllocatorVK` owns one resource heap divided into buffer,
sampled-image, and storage-image arenas plus a separate sampler heap. `ROBufferVK`, `ROImageViewVK`, and `ROSamplerVK` own indices
allocated lazily by their descriptor getters or transferred through their builders, and defer release for the configured frames
in flight.

Shaders represent each resource as an aligned `uint2` field in push data marked with `WBE_DESCRIPTOR("TYPE", "resource_id")`,
where `TYPE` is `uniform_buffer`, `storage_buffer`, `sampled_image`, `storage_image`, `sampler`, or `acceleration_structure`. An
optional third argument states the shader access: `"read"`, `"write"`, or `"read_write"`. Only storage buffers and storage images
may be written; they default to `"read_write"` and every other type is read. Pipeline-run tasks report these reflected accesses to
the render graph. The render graph binds both heaps before each task records, and `ROPipelineVK` pushes the reflected index block
before dispatch or draw. Pipeline binding paths are relative to that push-data block and may traverse nested structs and fixed-size
struct arrays.

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
changed through `RUSubmesh3D`, but mutation is not thread-safe. Recompile the affected mesh resources, and any acceleration
structures built from them, after changing geometry.

Static geometry resources use `RUStaticMesh`, with distinct `RUStaticOpaqueMesh` and `RUStaticMaskedMesh` resource types.
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

## Mesh Resources

Mesh compilers derive from `IMeshCompilerVK`, which owns the shared steps as protected helpers: geometry buffer upload through
`MeshBufferCompilerVK`, mesh-node layout validation, draw planning through `MeshDrawBatchPlannerVK`, and material compilation
through `MeshMaterialResourceBuilderVK`.

- `StaticMeshCompilerVK` compiles the static mesh categories into `StaticMeshResourcesVK` (below).
- `DynamicMeshCompilerVK` compiles one `RUMesh3D` with a fixed instance capacity into `DynamicMeshResourcesVK`. Its geometry,
  submesh, instance-link, and material resources are immutable; node transforms and indirect commands live in one buffer per frame
  in flight and change through `DynamicMeshResourcesVK::publish_visible_transforms()`. Import it with
  `RendGraphDescVK::add_resource(key, *dynamic_mesh)`, which registers each member under `<key>.<member>` using the
  `DynamicMeshResourcesVK` member names.

### Static Mesh Resources

`StaticMeshCompilerVK::compile()` compiles opaque, masked, and opaque double-sided geometry into one pipeline-independent
`StaticMeshResourcesVK`. It owns shared vertex and index buffers, separate opaque, opaque double-sided, and masked indirect command
buffers with their draw batches, submesh/node/instance tables, material parameters, sampler, and textures. Rendering is performed
by the generic `RTRunGraphicsPipelineIndirectVK`; callers provide the graphics pipeline of each draw batch.

### Loading Static Geometry

Static geometry resources are loaded through the normal chunk pipeline. `ResourceUnitLoaderBinary` loads the shared `RUBinary`
payload, and `register_renderer_vk_resource_unit_loaders()` registers `ResourceUnitLoaderStaticOpaqueMesh` and
`ResourceUnitLoaderStaticMaskedMesh` so `static_opaque_mesh` and `static_masked_mesh` schemas resolve into `RUStaticOpaqueMesh` and
`RUStaticMaskedMesh` resources.

Each static mesh stores:

- A list of submeshes.
- For each submesh: a material, a `RUVertexView`, a `RUIndexView`, and a `[first_instance, instance_count)` slice.
- One shared instance array containing `glm::mat4` transforms.

All drawable submeshes in one compiled static mesh must reference the same vertex `RUBinary` resource and the same index
`RUBinary` resource. Vertex and index binaries may be distinct from each other. The task validates that each vertex view and
index view stays within its binary bounds before it creates any Vulkan buffers.

### Vertex Assembly And Upload

`StaticMeshCompilerVK` does not flatten mesh data through `RenderDatabase`. Instead, compilation follows this path in
`src/function/render_vk/static_mesh_compiler_vk.cpp`:

1. `MeshBufferCompilerVK` verifies submesh storage, vertex stride, vertex byte ranges, index ranges, and instance ranges. It
  also enforces that every drawable submesh shares one vertex binary resource and one index binary resource.
2. `MeshBufferCompilerVK` uploads the vertex binary into an `ROBufferVK` with the effective vertex usage flags, and uploads
  the index binary into a separate `ROBufferVK` with the effective index usage flags (`build_binary_buffer()` in
  `mesh_buffer_compiler_vk.cpp`).
3. Each upload is cached in `RenderDatabase::register_compiled_render_resource()` under the binary resource identity plus Vulkan
  usage flags, so later tasks and renderer plugins can reuse compatible buffers without re-uploading identical geometry bytes.
4. `build_static_instance_buffer_data()` assembles three CPU-side tables from each set declaration: `GeometrySubmeshGPU`,
  `GeometryNodeGPU`, and `GeometryInstanceGPU`. The tables of all categories are concatenated.
5. `IMeshCompilerVK::create_storage_buffer()` uploads those tables as storage buffers for submesh metadata, node transforms, and per-instance draw
  indirection.

The important distinction is that vertex and index bytes stay in their original binary layouts. The task uploads those binaries
as-is into separate buffers, then uploads side-band metadata buffers that tell the shader which submesh and instance transform to
use for each indirect draw.

### Feeding The Graphics Pipeline

Mesh shaders declare the metadata tables as reflected storage buffers in `MeshDescriptorIndices` (`utils/utils.slang`),
annotated as read-only, under shader resource ids shared by every mesh kind:

- `mesh.submeshes` (`REND_RES_MESH_SUBMESHES`)
- `mesh.nodes` (`REND_RES_MESH_NODES`)
- `mesh.instances` (`REND_RES_MESH_INSTANCES`)

The node layout is validated against the reflection-only `ParameterBlock<MeshContractMetadata> mesh`.

Material resources follow the pipeline's reflected material texture contract. `get_default_image()` resolves the configured
default material, and `MeshMaterialResourceBuilderVK` builds the material-parameter table and sampler from it. The material
image views are returned as a CPU-side `RenderResourceListVK` through `StaticMeshResourcesVK::acquire_textures()`.

The application imports a compiled static mesh once with `RendGraphDescVK::add_resource(key, *static_mesh)`. Like every import, this
goes through `IBindableRenderResourceVK::bind()`; the composite registers each member under `<key>.<member>`, using the
`StaticMeshResourcesVK` member names (`VERTICES`, `NODES`, `MATERIAL_PARAMS`, ...). Graphics tasks bind the vertex, index, and
indirect-command buffers by those identifiers and
report the reflected metadata and material accesses themselves. Because the shader ids are shared, each task maps them to the drawn
mesh's resources with `RendTaskDescVK::alias()`, for example `REND_RES_MESH_NODES` to `sponza.nodes`. This includes the material
parameter table and sampler. Only ids the pipeline reflects may be aliased, since every alias must be used by the task. A `RenderResourceListVK` does not create GPU resources; graph construction retains the list under its
aggregate key and registers its entries as `<resource_list_key>[<index>]`. An access or edge on the aggregate expands to all indexed
entries.

`MeshDrawBatchPlannerVK::build()` groups drawable submeshes for the task's selected pipeline by effective reflected vertex
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
`RTRunGraphicsPipelineIndirectVK::PipelineBatch` selects a contiguous command range and records one `vkCmdDrawIndexedIndirect`; a
future compute task can write the same graph resource as a storage buffer when a chain of edges on that resource orders it before
the graphics task.

Before constructing the task:

1. Load the static meshes, materials, images, shaders, and graphics pipelines through `ResourceManager`, and compile the graphics
  pipeline for the task's attachment formats with `GraphicsPipelineCompilerVK`.
2. Compile the static mesh with `StaticMeshCompilerVK::compile()` and import it with `RendGraphDescVK::add_resource(key, *static_mesh)`.
3. Declare every render target with `RendGraphDescVK::add_image()` and `add_image_view()`; the graph allocates them when it is
  compiled. Import the renderer's swap-chain image-view container under `REND_RES_SWAP_CHAIN` with `add_resource()` when the graph
  renders to the swap chain.
4. Create one `RTRunGraphicsPipelineIndirectVK` per category-plus-pipeline combination, alias its render targets and mesh
  resources, and order it
  against other tasks with resource edges declared by `RendTaskDescVK::connect()`.

```cpp
RendererVK* renderer = RendererVK::get_singleton();
const BaseRenderDataVK* render_data = renderer->get_base_render_data();
RenderAllocator* render_allocator = render_data->allocator;

// opaque_mesh and the compiled graphics pipeline come from step 1; the vertex metadata, entry, and material contract come from
// the pipeline's shader metadata.
Ref<StaticMeshResourcesVK> static_mesh = StaticMeshCompilerVK(renderer).compile(opaque_mesh,
    MEM_NULL,
    opaque_vertex_metadata,
    opaque_vertex_entry,
    ShaderMetadata{},
    {},
    material_contract,
    StaticMeshCompilerVK::Config{});

RTRunGraphicsPipelineIndirectVK::Builder task_builder(render_allocator);
task_builder.set_render_extent(render_data->swap_chain_extent)
    .set_vertex_buffer("sponza.vertices")
    .set_index_buffer("sponza.indices")
    .set_indirect_buffer("sponza.opaque_draw_batches");
for (size_t i = 0; i < static_mesh->get_opaque_draw_batch_count(); ++i) {
    const MeshDrawBatchVK& batch = static_mesh->get_opaque_draw_batch(i);
    task_builder.add_pipeline_batch(graphics_pipeline, batch.first_draw, batch.draw_count, batch.vertex_input, batch.vertex_buffer_offset);
}
RendTaskDescVK task_desc(render_allocator, "run_static_mesh", task_builder.create_unique(render_data));
task_desc.alias(RenderTargetsVK::color_id(0), REND_RES_SWAP_CHAIN).alias(RenderTargetsVK::DEPTH_ID, "scene_depth_view");
// Map the shader-side mesh ids this pipeline reflects to the static mesh's resources.
task_desc.alias(REND_RES_MESH_SUBMESHES, "sponza.submeshes")
    .alias(REND_RES_MESH_NODES, "sponza.nodes")
    .alias(REND_RES_MESH_INSTANCES, "sponza.instances")
    .alias(REND_RES_MESH_MATERIAL_PARAMS, "sponza.material_params")
    .alias(REND_RES_MESH_MATERIAL_SAMPLER, "sponza.material_sampler");

const VkExtent2D extent = render_data->swap_chain_extent;
RendGraphDescVK graph_desc(render_allocator);
graph_desc.add_resource("sponza", *static_mesh)
    .add_resource(REND_RES_SWAP_CHAIN, renderer->acquire_swap_chain_image_view_container())
    .add_image("scene_depth_image",
        {.usage = RendResPrimaryUsageVK::IMAGE_DEPTH_TARGET,
            .format = VK_FORMAT_D32_SFLOAT,
            .extent = {.width = extent.width, .height = extent.height, .depth = 1}})
    .add_image_view("scene_depth_view", "scene_depth_image")
    .add_task(std::move(task_desc));

renderer->register_render_graph("scene", std::move(graph_desc));
renderer->activate_render_graph("scene");
```

The application contributes all graph-visible resources before registration. The renderer does not add or replace descriptor
resources while building the graph. The task retains its graphics pipelines directly, and the `RendTaskDescVK` move into the graph
remains one-shot. The reflected camera and light buffers of the pipeline must also be declared on the graph.

### Render Targets

Graphics tasks render into the task-local resources `render_target_colors[<index>]` and `render_target_depth`
(`RenderTargetsVK::color_id()` and `RenderTargetsVK::DEPTH_ID`). The number of color targets and the use of a depth target follow the
graphics pipeline's attachment formats; every pipeline in one task must share that layout. Map the targets to graph resources on the
task descriptor:

```cpp
task_desc.alias(RenderTargetsVK::color_id(0), "gbuffer_albedo")
    .alias(RenderTargetsVK::color_id(1), "gbuffer_normal")
    .alias(RenderTargetsVK::DEPTH_ID, "gbuffer_depth");
```

- `set_clear_values()` follows target order: all color values first, then depth. Empty clears every target to zero.
- `set_color_load_op()` and `set_depth_load_op()` control how a follow-up task reuses targets produced by an earlier task; a loaded
  target is also read.
- Task ids, resource aliases, and dependencies belong to `RendTaskDescVK`, not the task configuration.

### Static Mesh Acceleration Structures

Ray-tracing acceleration structures are explicit resources; the renderer keeps no published scene geometry.

1. Compile the static mesh with `StaticMeshAccelerationStructureVK::GEOMETRY_BUFFER_USAGE` added to
  `StaticMeshCompilerVK::Config::vertex_buffer_usage` and `index_buffer_usage`. The compiled `StaticMeshResourcesVK` keeps its
  source `RUStatic*Mesh` units.
2. Create `StaticMeshAccelerationStructureCompilerVK` from the base render data and `RayTracingPluginVK::get_base_ray_tracing_data()`,
  then call `compile()` with the static mesh. The result holds one BLAS per drawable opaque or opaque double-sided submesh, one
  TLAS instance per instance transform, the instance buffer, scratch buffers, and the build inputs.
3. Import it with `add_resource(key, *acceleration_structure)`, which registers the TLAS as `<key>.top_level`, and record its build
  with `StaticMeshAccelerationStructureVK::record_build()`, for example in an `RTBuildAccelerationStructuresVK` task writing
  `<key>.top_level` and wrapped in `RTTriggeredVK` to run once. Consumers order after that task with `connect()`.

### Binding Pipeline Descriptors

Pipeline-run tasks bind every reflected `WBE_DESCRIPTOR` field once, when the graph is built: the field's resource id, resolved
through the task's `RendTaskDescVK::alias()` map, names a graph resource. There is no per-field binding API. The graph resource's
type must match the field's category: buffers bind as `ROBufferVK`, image fields as `ROImageViewVK`, sampler fields as
`ROSamplerVK`, and acceleration-structure fields as `ROAccelerationStructureVK`. A missing resource or an incompatible type makes
graph construction fail.

Frame-local resources are graph resources held in an `ROCMultislotsVK`, either declared per frame on the graph or imported as a
container. Update their contents instead of rebinding them:

```cpp
graph_desc.add_buffer("camera_info", {.usage = RendResPrimaryUsageVK::BUFFER_UNIFORM, .domain = DeviceDomain::HOST_DEVICE, .per_frame = true});
Ref<RenderGraphVK> graph = renderer->register_render_graph("scene", std::move(graph_desc));
Ref<ROCMultislotsVK> camera_buffers = graph->acquire_resource("camera_info").dynamic_cast_ref<ROCMultislotsVK>();
camera_buffers->publish_buffer_update(&camera_info, 0, sizeof(camera_info));
```

At record time, the task resolves each container from `current_frame_slot` and pushes that resource's descriptor index. Publish
buffer contents on the rendering thread; callers on other threads must use `IRenderer::add_job()`.

To add a dependency, connect the earlier task to the later one through the resource they share. The graph derives the edge's
stages and accesses from the usages both tasks report for that resource, so both tasks must access it:

```cpp
RendTaskDescVK task_desc(render_allocator, "run_shadow", std::move(task));
task_desc.connect("run_gbuffer", "shadow_moments");
graph_desc.tasks.emplace_back(std::move(task_desc));
```

Compute and graphics pipeline-run tasks push their pipeline-owned descriptor-index blocks automatically after the graph binds the
global heaps. Every task reports all of its accesses through `IRenderTaskVK::append_accesses()`: reflected shader resources, render
targets, and fixed-function buffers. The accesses are validated against the resource edges; they do not create edges. When a task's
identifiers differ from the graph's, alias them on the task descriptor; the graph resolves every identifier the task binds or
reports through that map, and rejects aliases the task never uses:

```cpp
RendTaskDescVK blur(render_allocator);
blur.task_id = "blur_bloom_0";
blur.alias("kawase_blur_source", "bloom_ping")
    .alias("kawase_blur_destination", "bloom_pong")
    .alias("kawase_blur_sampler", "bloom_sampler");
blur.create = [render_allocator, render_data, blur_pipeline]() -> Unique<IRenderTaskVK> {
    return make_unique<RTRunComputePipelineVK>(render_allocator, render_data, blur_pipeline, VkExtent3D{.width = 160, .height = 90, .depth = 1});
};
```
