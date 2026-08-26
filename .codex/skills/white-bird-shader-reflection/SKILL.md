---
name: white-bird-shader-reflection
description: Use and extend White Bird Engine's Slang shader-reflection pipeline. Apply when loading reflected shader parameters from RUShader resources, querying metadata, consuming user attributes or layout data, diagnosing reflection contracts, or adding reflected types and Vulkan descriptor support.
---

# White Bird Shader Reflection

Use the backend-neutral reflection model for inspection and contracts. Keep Vulkan translation in `function/render_vk` and keep resource acquisition, metadata loading, and querying separate.

## Acquire metadata from a shader resource

Given a resource key, acquire the shader resource and load its normalized reflection sidecar:

```cpp
auto shader = ResourceManager::get_singleton()->get_resource<RUShader>("shader_resource_key");
ShaderMetadata metadata = ShaderMetadataLoader().load(shader);

const ShaderMetadataParameter* parameter = ShaderMetadataQuery().find_parameter(metadata, "camera_info");
if (parameter == nullptr) {
    throw std::runtime_error("Required shader parameter 'camera_info' is missing.");
}

const ShaderMetadataTypeField* view = ShaderMetadataQuery().find_field(parameter->type.fields, "view");
if (view == nullptr) {
    throw std::runtime_error("Required shader field 'camera_info.view' is missing.");
}
```

Include `resource/resource_manager.hh`, `function/render/shader_metadata_loader.hh`, and `function/render/shader_metadata_query.hh` as needed. If the caller already owns `RefRes<RUShader>`, pass it directly to `ShaderMetadataLoader::load`.

Treat pointers and references returned by `ShaderMetadataQuery` as views into `ShaderMetadata`: keep the metadata alive and unmodified. `find_parameter` and `find_field` return `nullptr` for absence but throw for empty or duplicate names. Use `resolve_entry_point` when an entry name may be omitted; omission is valid only when exactly one reflected entry exists for the stage.

## Choose the consumer

- For name-based inspection, use `ShaderMetadataQuery`.
- For annotation roles, use `ShaderRoleFieldResolver`; do not hand-roll attribute parsing or role hashing.
- For vertex inputs, use `VertexInputContractResolver` and `WBE_VERTEX_INPUT`.
- For material texture indices, use `MaterialTextureContractResolver` and `WBE_TEXTURE`.
- For Vulkan descriptor layouts, use the translation utilities in `function/render_vk/shader_metadata_vk_utils.*`.
- For pipeline data binding, use reflected bindpoint paths through the existing pipeline compiler rather than duplicating packing logic.

Do not introduce a closed texture-role enum or branch on roles such as `albedo`, `normal`, or `rma`. Roles are shader-defined strings. An attribute with no explicit string argument uses the annotated field name as its role.

## Before changing support

Read [references/system-and-extension.md](references/system-and-extension.md) when the task asks what is supported, touches the ACP normalizer or serialized metadata schema, adds an annotation contract, changes field paths, or adds a Vulkan descriptor/type mapping. Trace one real Slang declaration through raw reflection, normalization, deserialization, and its final consumer before editing.

Make the smallest layer-correct change. Add tests at every boundary that changes, and never edit generated serialization files or compiled reflection fixtures by hand; regenerate them through the normal build/resource pipeline.

## Validate

Use the repository validation wrapper and a focused filter first:

```sh
python validation.py -t debug -- --gtest_filter='WBEShaderMetadataVKTest*'
```

Select additional resolver or Vulkan utility tests named in the reference when those consumers change. The wrapper runs the full unit test binary after the focused test passes. Do not run lint automatically.
