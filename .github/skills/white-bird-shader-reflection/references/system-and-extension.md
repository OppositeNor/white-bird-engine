# Shader Reflection System and Extension Guide

## Pipeline map

1. A shader entry in an ACP manifest supplies `src`, `dst`, and `metadata`.
2. `build_script/resource/acp/acp_compiler_shader.py` invokes `slangc` for SPIR-V 1.6 with descriptor-heap support. Buffer and image descriptor indices use separate regions and their descriptor type's device-reported stride.
3. It performs a second temporary compile with `WBE_REFLECTION_METADATA=1` so reflection-only declarations contribute metadata without entering the runtime shader, then normalizes Slang's JSON into the snake-case schema represented by `include/function/render/shader_metadata.hh`.
4. The resource loader stores the compiled words, metadata path, and discovered stage-entry names in `RUShader` (`include/function/render/render_resource_unit.hh` and `src/function/render_vk/render_vk_resource_unit_loader.cpp`).
5. `ShaderMetadataLoader::load` resolves `RUShader::metadata_path`, parses the JSON, and deserializes `ShaderMetadata`.
6. Backend-neutral resolvers consume metadata for entry points, annotations, material textures, and vertex inputs. Vulkan utilities translate `WBE_DESCRIPTOR` fields into a push-data layout, and pipeline compilers resolve caller-provided bindpoints to resource-owned descriptor-heap indices.

Metadata is a sidecar resource, not embedded in `RUShader`. A missing shader handle or metadata path is an error.

## Normalized schema currently preserved

The runtime model preserves:

- global parameters;
- entry points, stages, entry-point parameters, results, and bindings;
- declaration and type names;
- type `kind`, scalar type, array element count and element type;
- nested fields;
- semantics and semantic indices;
- binding kind, index, count, space, offset, size, and element stride;
- user attributes on global parameters, entry points, entry-point parameters, types, nested element types, and fields.

User-attribute arguments support only integer, floating-point, and string values. Boolean and other argument shapes fail during ACP normalization.

Constant buffers and parameter blocks expose fields from Slang's element layout. `bindless_space_index` is not part of the normalized runtime schema.

## Runtime queries and paths

`ShaderMetadataQuery` supports:

- exact stage and entry-point lookup;
- stage resolution with an omitted entry name only when unambiguous;
- unique global parameter lookup;
- unique direct-field lookup in a supplied field vector.

It does not implement a general dotted-path walker. For nested inspection, query each field vector deliberately or use the contract resolver responsible for that structure.

Descriptor-heap bindpoint declarations use field paths relative to the push-data block, such as `camera_info` or `lights[0].shadowmap`. The reflection walker recursively flattens nested structs and fixed-size struct arrays. Buffer bindings must name a complete `ROBufferVK`; subranges are not accepted. Missing bindings remain zero until supplied, while a supplied object of the wrong type throws.

## Vulkan descriptor-heap support

`src/function/render_vk/shader_metadata_vk_utils.cpp` recognizes these `WBE_DESCRIPTOR` strings:

- `uniform_buffer`;
- `storage_buffer`;
- `sampled_image`;
- `storage_image`;
- `sampler`.

Each annotation requires one non-empty string argument and must mark an aligned eight-byte `uint2` in the entry point's sole reflected `pushConstantBuffer`. Duplicate annotations, duplicate paths, overlapping ranges, malformed arrays, unsupported categories, and invalid layout values throw `std::runtime_error`. A descriptor-free entry point yields an empty layout. When graphics stages are merged, equal field paths must agree on category and byte offset, and distinct fields must not overlap.

`DescriptorHeapAllocatorVK` owns renderer-wide logical arenas for buffers, sampled images, storage images, and samplers. Buffers and images occupy separate regions of the one Vulkan resource heap. Buffer indices address the buffer region using `bufferDescriptorSize`; sampled and storage image indices address the image region using `imageDescriptorSize`. `ROBufferVK`, `ROImageViewVK`, and `ROSamplerVK` lazily allocate descriptor indices unless builders transfer preallocated ones, own those indices, and defer their release for frames-in-flight safety. `RenderGraphVK` binds the resource and sampler heaps on each task command buffer; `ROPipelineVK` retains bound resources and pushes the reflected index block with `vkCmdPushDataEXT`.

## Built-in annotation contracts

### `WBE_DESCRIPTOR`

`WBE_DESCRIPTOR("TYPE", "resource_id", "ACCESS")` takes an optional access argument: `read`, `write`, or `read_write`. Slang reflects an omitted argument as its empty default; storage buffers and storage images then default to `read_write`, and every other type is read-only and rejects `write`. Pipeline-run tasks report the reflected accesses to the render graph, so annotate read-only storage resources with `read`.

`collect_shader_descriptor_heap_layout()` finds the push-data parameter used by the selected entry point and recursively collects annotated `uint2` fields. Paths are relative to the push-data block; arrays expand to explicit paths such as `lights[0].shadowmap`. `collect_graphics_pipeline_descriptor_heap_layout()` merges vertex, optional geometry, and fragment contracts and combines compatible stage flags.

Production shader code accesses the global resource and sampler heaps through these indices. Declarations needed only to preserve higher-level contracts in metadata belong under `#if WBE_REFLECTION_METADATA`; ACP's second compile sees them, while the runtime SPIR-V does not.

Relevant files:

- `res/assets/shaders/utils/utils.slang`
- `include/function/render_vk/shader_metadata_vk_utils.hh`
- `src/function/render_vk/shader_metadata_vk_utils.cpp`
- `res/assets/shaders/descriptor_heap_reflection/descriptor_heap_reflection.slang`
- `tests/unit/function/shader_metadata_vk_utils_test.cpp`

### `WBE_VERTEX_INPUT`

`VertexInputContractResolver` searches entry-point parameter fields recursively. A missing string argument defaults the role to the field name. Annotated fields must be `varyingInput` values. Supported formats are scalar or vector `float32`, `int32`, and `uint32` with one through four components.

Relevant files:

- `res/assets/shaders/utils/utils.slang`
- `include/function/render/vertex_input_contract_resolver.hh`
- `src/function/render/vertex_input_contract_resolver.cpp`
- `tests/unit/function/shader_metadata_vk_utils_test.cpp`

### `WBE_TEXTURE`

`MaterialTextureContractResolver` reads direct children of each `MaterialParams` element. Each annotated index must be a four-byte uniform `int32` scalar. Roles must be unique by both name and hash. The role string is open-ended and comes from the shader; an omitted string defaults to the field name.

The fixed material contract is split between reflection-only metadata and a runtime table:

- `MaterialTextureContractMetadata.material_sampler` is a sampler;
- `MaterialTextureContractMetadata.textures` is an array of texture resources;
- `MaterialParamsBuffer.materials` is a runtime sized array with a valid element stride.

Across the merged pipeline metadata, stages must agree on the reflected layout, and the aggregate contract must expose material textures and material parameters together. Applications may configure only the two base variable names and the default-material resource key. Static-mesh compilation resolves each material texture to its global sampled-image heap index and stores that value in `MaterialParamsBuffer`; it does not allocate a per-pipeline descriptor array.

Relevant files:

- `res/assets/shaders/utils/utils.slang`
- `src/function/render/shader_role_field_resolver.cpp`
- `src/function/render/material_texture_contract_resolver.cpp`
- `tests/unit/function/shader_role_field_resolver_test.cpp`
- `tests/unit/function/material_texture_contract_resolver_test.cpp`

## How to expand reflection support

Trace the exact raw Slang JSON first; do not infer its layout from the Slang source alone.

1. Normalize the new raw property or shape in the narrowest helper in `build_script/resource/acp/acp_compiler_shader.py`: binding, type atom, field, full type, parameter, result, or entry point.
2. If runtime consumers need new data, add a serializable field to the matching structure in `include/function/render/shader_metadata.hh`. Let the normal build regenerate serialization artifacts; never edit `*.gen.*`.
3. Add or extend deserialization coverage in `tests/unit/function/shader_metadata_vk_test.cpp`. Use both a small normalized JSON test and, when Slang behavior matters, a compiled fixture under `res/assets/shaders` reached through `res/res_chunks`.
4. Put semantic validation in a backend-neutral contract resolver when possible. Use `ShaderMetadataQuery` for uniqueness and `ShaderRoleFieldResolver` for annotation parsing, default roles, duplicate roles, and hash collisions.
5. If the construct represents a Vulkan descriptor, extend `WBE_DESCRIPTOR` category or push-data field handling in `src/function/render_vk/shader_metadata_vk_utils.cpp`. Preserve alignment, overlap, path, stage-merge, and array-layout validation, then add focused utility tests.
6. If the change affects caller bindpoints, define nested and indexed path behavior explicitly before modifying `src/function/render_vk/pipeline_compiler_vk.cpp`. Preserve complete-resource binding and resource ownership unless the feature requires an API change.
7. Add contract-resolver tests for invalid type, layout, duplicate, missing-pair, and cross-stage mismatch cases relevant to the new feature.

Do not make generic engine propagation branch on concrete texture roles. Do not move backend-specific descriptor concepts into `function/render`. Do not catch engine exceptions; invalid reflection contracts throw `std::runtime_error` and terminate outside tests.

## Known limitations

- No single typed API returns an arbitrary shader value from a resource; the supported flow is load metadata, query its declarations, then let an appropriate consumer interpret them.
- Metadata is reloaded and deserialized from the sidecar when requested; `RUShader` does not own a parsed metadata object.
- Descriptor translation is currently Vulkan-specific.
- General direct-field lookup is one level at a time.
- Descriptor-heap buffer bindings cover complete render objects, not arbitrary subranges.
- Only fixed-size arrays of descriptor-bearing structs are expanded into indexed paths.
- The parameter-block array flattening rule is intentionally narrow.

## Useful fixtures and tests

- `res/assets/shaders/reflection_attributes`: attribute locations and int/float/string arguments.
- `res/assets/shaders/forward_cook_torrance_pbr`: nested material parameter layout and texture roles.
- `res/assets/shaders/draw_triangle`: vertex roles and automatically assigned locations.
- `res/assets/shaders/descriptor_heap_reflection`: explicit descriptor fields, nested structs, and fixed arrays.
- `tests/unit/function/shader_metadata_vk_test.cpp`: normalized schema and compiled fixtures.
- `tests/unit/function/shader_role_field_resolver_test.cpp`: role parsing, duplicates, and hash collisions.
- `tests/unit/function/material_texture_contract_resolver_test.cpp`: material contract validity and stage agreement.
- `tests/unit/function/shader_metadata_vk_utils_test.cpp`: Vulkan descriptor-heap layout and vertex-input translation.
