# Shader Reflection System and Extension Guide

## Pipeline map

1. A shader entry in an ACP manifest supplies `src`, `dst`, and `metadata`.
2. `build_script/resource/acp/acp_compiler_shader.py` invokes `slangc` with `-target spirv` and `-reflection-json`.
3. The same compiler normalizes Slang's JSON into the snake-case schema represented by `include/function/render/shader_metadata.hh`.
4. The resource loader stores the compiled words, metadata path, and discovered stage-entry names in `RUShader` (`include/function/render/render_resource_unit.hh` and `src/function/render_vk/render_vk_resource_unit_loader.cpp`).
5. `ShaderMetadataLoader::load` resolves `RUShader::metadata_path`, parses the JSON, and deserializes `ShaderMetadata`.
6. Backend-neutral resolvers consume metadata for entry points, annotations, material textures, and vertex inputs. Vulkan utilities translate supported resources into descriptor bindings, and the graphics pipeline compiler packs caller-provided bindpoints at reflected offsets.

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
- bindless space index;
- user attributes on global parameters, entry points, entry-point parameters, types, nested element types, and fields.

User-attribute arguments support only integer, floating-point, and string values. Boolean and other argument shapes fail during ACP normalization.

Constant buffers and parameter blocks expose fields from Slang's element layout. The normalizer also has one specialized transformation: parameter-block arrays of structs whose fields are descriptor-table slots are flattened into names such as `material_textures.textures`, with binding indices and counts adjusted for the array.

## Runtime queries and paths

`ShaderMetadataQuery` supports:

- exact stage and entry-point lookup;
- stage resolution with an omitted entry name only when unambiguous;
- unique global parameter lookup;
- unique direct-field lookup in a supplied field vector.

It does not implement a general dotted-path walker. For nested inspection, query each field vector deliberately or use the contract resolver responsible for that structure.

Pipeline bindpoint declarations accept reflected names such as `camera_info`, `camera_info.view`, or an indexed path. Current indexed parsing supports one numeric `[index]`; if text follows the bracket, it must start with `.`. Uniform data may bind the entire reflected descriptor or direct fields. Field-by-field packing copies bytes at reflected offsets into zero-initialized storage; it is not a general typed serializer for arbitrary scalar/vector values.

## Vulkan descriptor support

`src/function/render_vk/shader_metadata_vk_utils.cpp` currently maps:

- `constantBuffer` to a uniform-buffer descriptor;
- `resource` whose reflected scalar type starts with `texture` to a sampled-image descriptor;
- `samplerState` to a sampler descriptor;
- equivalent direct or array-element field forms inside parameter blocks.

Unsupported descriptor kinds or shapes throw `std::runtime_error`. When merging shader stages, identical set/binding pairs must have compatible descriptor types and counts; compatible stage flags are combined.

## Built-in annotation contracts

### `WBE_VERTEX_INPUT`

`VertexInputContractResolver` searches entry-point parameter fields recursively. A missing string argument defaults the role to the field name. Annotated fields must be `varyingInput` values. Supported formats are scalar or vector `float32`, `int32`, and `uint32` with one through four components.

Relevant files:

- `res/assets/shaders/defs.slang`
- `include/function/render/vertex_input_contract_resolver.hh`
- `src/function/render/vertex_input_contract_resolver.cpp`
- `tests/unit/function/shader_metadata_vk_utils_test.cpp`

### `WBE_TEXTURE`

`MaterialTextureContractResolver` reads direct children of each `MaterialParams` element. Each annotated index must be a four-byte uniform `int32` scalar. Roles must be unique by both name and hash. The role string is open-ended and comes from the shader; an omitted string defaults to the field name.

The fixed global contract is:

- `MaterialTextures.material_sampler` is a sampler;
- `MaterialTextures.textures` is an array of texture resources;
- `MaterialParamsBuffer.materials` is a sized array with a valid element stride.

Across the merged pipeline metadata, stages must agree on the reflected layout, and the aggregate contract must expose material textures and material parameters together. Applications may configure only the two base variable names and the default-material resource key.

Relevant files:

- `res/assets/shaders/defs.slang`
- `src/function/render/shader_role_field_resolver.cpp`
- `src/function/render/material_texture_contract_resolver.cpp`
- `tests/unit/function/shader_role_field_resolver_test.cpp`
- `tests/unit/function/material_texture_contract_resolver_test.cpp`

## How to expand reflection support

Trace the exact raw Slang JSON first; do not infer its layout from the Slang source alone.

1. Normalize the new raw property or shape in the narrowest helper in `build_script/resource/acp/acp_compiler_shader.py`: binding, type atom, field, full type, parameter, result, or entry point.
2. If runtime consumers need new data, add a serializable field to the matching structure in `include/function/render/shader_metadata.hh`. Let the normal build regenerate serialization artifacts; never edit `*.gen.*`.
3. Add or extend deserialization coverage in `tests/unit/function/shader_metadata_vk_test.cpp`. Use both a small normalized JSON test and, when Slang behavior matters, a compiled fixture under `test_env_res/assets/shaders` reached through `test_env_res/res_chunks`.
4. Put semantic validation in a backend-neutral contract resolver when possible. Use `ShaderMetadataQuery` for uniqueness and `ShaderRoleFieldResolver` for annotation parsing, default roles, duplicate roles, and hash collisions.
5. If the construct represents a Vulkan descriptor, extend the parameter/field descriptor mapping in `src/function/render_vk/shader_metadata_vk_utils.cpp`. Preserve set, binding, count, stage, and array-layout validation, then add focused utility tests.
6. If the change affects caller bindpoints, define nested and indexed path behavior explicitly before modifying `src/function/render_vk/graphics_pipeline_compiler_vk.cpp`. Preserve existing whole-descriptor and field-packing behavior unless the feature requires an API change.
7. Add contract-resolver tests for invalid type, layout, duplicate, missing-pair, and cross-stage mismatch cases relevant to the new feature.

Do not make generic engine propagation branch on concrete texture roles. Do not move backend-specific descriptor concepts into `function/render`. Do not catch engine exceptions; invalid reflection contracts throw `std::runtime_error` and terminate outside tests.

## Known limitations

- No single typed API returns an arbitrary shader value from a resource; the supported flow is load metadata, query its declarations, then let an appropriate consumer interpret them.
- Metadata is reloaded and deserialized from the sidecar when requested; `RUShader` does not own a parsed metadata object.
- Descriptor translation is currently Vulkan-specific.
- General direct-field lookup is one level at a time.
- General uniform bindpoint packing is byte-oriented and limited to reflected direct fields.
- Bindpoint paths accept at most one array index.
- The parameter-block array flattening rule is intentionally narrow.

## Useful fixtures and tests

- `test_env_res/assets/shaders/reflection_attributes`: attribute locations and int/float/string arguments.
- `test_env_res/assets/shaders/forward_cook_torrance_pbr`: nested material parameter layout and texture roles.
- `test_env_res/assets/shaders/draw_triangle`: vertex roles and automatically assigned locations.
- `tests/unit/function/shader_metadata_vk_test.cpp`: normalized schema and compiled fixtures.
- `tests/unit/function/shader_role_field_resolver_test.cpp`: role parsing, duplicates, and hash collisions.
- `tests/unit/function/material_texture_contract_resolver_test.cpp`: material contract validity and stage agreement.
- `tests/unit/function/shader_metadata_vk_utils_test.cpp`: Vulkan descriptor and vertex-input translation.