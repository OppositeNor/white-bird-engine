/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "core/parser/parser_json.hh"
#include "function/function_utils.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_material_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/shader_metadata_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

struct IndexedFieldPath {
    std::string field_path;
    std::optional<uint32_t> array_index;
};

IndexedFieldPath parse_indexed_field_path(std::string_view p_field_path) {
    size_t open_index = p_field_path.find('[');
    size_t stray_close_index = p_field_path.find(']');
    if (open_index == std::string_view::npos) {
        if (stray_close_index != std::string_view::npos) {
            throw std::runtime_error(std::format("Failed to parse bindpoint '{}': closing bracket without opening bracket.", p_field_path));
        }
        return IndexedFieldPath{.field_path = std::string(p_field_path)};
    }
    size_t close_index = p_field_path.find(']', open_index + 1);
    if (close_index == std::string_view::npos) {
        throw std::runtime_error(std::format("Failed to parse bindpoint '{}': missing closing bracket.", p_field_path));
    }
    if (p_field_path.find('[', open_index + 1) != std::string_view::npos) {
        throw std::runtime_error(std::format("Failed to parse bindpoint '{}': only one array index is supported.", p_field_path));
    }
    if (close_index == open_index + 1) {
        throw std::runtime_error(std::format("Failed to parse bindpoint '{}': array index is empty.", p_field_path));
    }
    std::string_view index_text = p_field_path.substr(open_index + 1, close_index - open_index - 1);
    uint32_t array_index = 0;
    for (char character : index_text) {
        if (character < '0' || character > '9') {
            throw std::runtime_error(std::format("Failed to parse bindpoint '{}': array index is invalid.", p_field_path));
        }
        array_index = array_index * 10 + static_cast<uint32_t>(character - '0');
    }
    if (close_index + 1 < p_field_path.size() && p_field_path[close_index + 1] != '.') {
        throw std::runtime_error(
            std::format("Failed to parse bindpoint '{}': array index must be followed by a field separator.", p_field_path));
    }
    std::string result;
    result.reserve(p_field_path.size());
    result.append(p_field_path.substr(0, open_index));
    result.append(p_field_path.substr(close_index + 1));
    return IndexedFieldPath{.field_path = std::move(result), .array_index = array_index};
}

uint32_t image_upload_channel_count(const RUImage& p_image) {
    // Material textures are uploaded as RGBA because Vulkan format selection and descriptor writes expect four channels here.
    if (p_image.channel_count == 3 || p_image.channel_count == 4) {
        return 4;
    }
    throw std::runtime_error(std::format("Failed to compile material texture: unsupported image channel count {}.", p_image.channel_count));
}

VkFormat image_format_from_resource(const RUImage& p_image) {
    // Reuse channel validation before choosing the Vulkan format so unsupported images fail at the format boundary.
    image_upload_channel_count(p_image);
    if (p_image.config.test(RUImage::IS_HDR)) {
        // HDR image data is stored as float channels and uploaded through the matching float RGBA format.
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    return p_image.color_space == ColorSpace::SRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
}

std::vector<uint8_t> build_image_upload_data(const RUImage& p_image) {
    // Convert serialized image data into the byte layout consumed by the staging upload buffer.
    if (p_image.image_data == nullptr) {
        throw std::runtime_error("Failed to compile material texture: image data is missing.");
    }

    const uint32_t source_channels = p_image.channel_count;
    const uint32_t target_channels = image_upload_channel_count(p_image);
    const size_t pixel_count = static_cast<size_t>(p_image.width) * static_cast<size_t>(p_image.height);
    const size_t channel_size = p_image.config.test(RUImage::IS_HDR) ? sizeof(float) : sizeof(uint8_t);
    const auto* source_data = reinterpret_cast<const uint8_t*>(p_image.image_data.get());
    std::vector<uint8_t> upload_data(pixel_count * target_channels * channel_size);

    if (source_channels == target_channels) {
        // Already-RGBA data can be copied directly without per-pixel expansion.
        std::memcpy(upload_data.data(), source_data, upload_data.size());
        return upload_data;
    }

    if (p_image.config.test(RUImage::IS_HDR)) {
        // RGB float textures are expanded to RGBA by appending an opaque alpha float to every pixel.
        const float alpha = 1.0F;
        const size_t source_stride = static_cast<size_t>(source_channels) * sizeof(float);
        const size_t target_stride = static_cast<size_t>(target_channels) * sizeof(float);
        for (size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
            // Copy the source float channels first, then append alpha at the end of the target pixel.
            const size_t source_offset = pixel_index * source_stride;
            const size_t target_offset = pixel_index * target_stride;
            std::memcpy(upload_data.data() + target_offset, source_data + source_offset, source_stride);
            std::memcpy(upload_data.data() + target_offset + source_stride, &alpha, sizeof(alpha));
        }
        return upload_data;
    }

    for (size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
        // RGB byte textures are expanded to RGBA by copying color channels and setting alpha to fully opaque.
        const size_t source_offset = pixel_index * source_channels;
        const size_t target_offset = pixel_index * target_channels;
        upload_data[target_offset + 0] = source_data[source_offset + 0];
        upload_data[target_offset + 1] = source_data[source_offset + 1];
        upload_data[target_offset + 2] = source_data[source_offset + 2];
        upload_data[target_offset + 3] = UINT8_MAX;
    }
    return upload_data;
}

std::string text_resource_value(RefRes<RUText> p_text) {
    // RUText stores C-string data in the resource chunk; expose it as a normal string for binding lookups.
    if (p_text == nullptr || p_text->text_data == nullptr) {
        throw std::runtime_error("Failed to compile material texture: texture key is missing.");
    }
    return std::string(p_text->text_data.get());
}

ShaderMetadata load_shader_metadata(RefRes<RUShader> p_shader) {
    // Shader metadata is loaded beside the shader resource because descriptor layouts are derived from reflection data.
    if (p_shader == nullptr || p_shader->metadata_path == nullptr) {
        throw std::runtime_error("Failed to load shader metadata: metadata path is missing.");
    }
    ParserJSON metadata_parser;
    metadata_parser.parse(get_resource_path(p_shader->metadata_path.get()));
    ShaderMetadata metadata;
    metadata.deserialize(metadata_parser.get_data());
    return metadata;
}

bool descriptor_name_matches_bindpoint_scope(std::string_view p_descriptor_name, std::string_view p_bindpoint_scope) {
    if (p_descriptor_name == p_bindpoint_scope) {
        return true;
    }
    if (p_descriptor_name.size() <= p_bindpoint_scope.size() + 1 || !p_descriptor_name.starts_with(p_bindpoint_scope)) {
        return false;
    }
    return p_descriptor_name[p_bindpoint_scope.size()] == '.';
}

const RenderBindpointMaterialDeclaration* find_material_bindpoint(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_descriptor_name) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.material_declerations) {
        if (!bindpoint_declaration.material_key.empty() &&
            descriptor_name_matches_bindpoint_scope(p_descriptor_name, bindpoint_declaration.field_path)) {
            return &bindpoint_declaration;
        }
    }
    return nullptr;
}

bool has_texture_bindpoint(const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_texture_key) {
    // Raw texture bindings are matched by shader texture parameter name, independent of material texture slots.
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
        if (parse_indexed_field_path(bindpoint_declaration.field_path).field_path == p_texture_key) {
            return true;
        }
    }
    return false;
}

struct UniformDataView {
    const void* data = nullptr;
    size_t size = 0;
};

UniformDataView get_uniform_data(const RenderBindpointDeclarations& p_bindpoint_declarations,
    const RenderBindpointBufferDeclaration& p_bindpoint_declaration) {
    std::span<const uint8_t> data = p_bindpoint_declarations.get_buffer_data(p_bindpoint_declaration);
    return UniformDataView{.data = data.data(), .size = data.size()};
}

void validate_bindpoint_declarations(const RenderBindpointDeclarations& p_bindpoint_declarations) {
    // Uniforms are optional, but every provided binding must be complete before descriptor writes begin.
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        if (bindpoint_declaration.field_path.empty()) {
            throw std::runtime_error("Failed to compile graphics pipeline: uniform binding field path is empty.");
        }
        parse_indexed_field_path(bindpoint_declaration.field_path);
        if (bindpoint_declaration.offset + bindpoint_declaration.size > p_bindpoint_declarations.buffer_data.size()) {
            throw std::runtime_error(std::format("Failed to compile graphics pipeline: uniform binding '{}' exceeds the packed buffer range.",
                bindpoint_declaration.field_path));
        }
    }
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
        if (bindpoint_declaration.field_path.empty()) {
            throw std::runtime_error("Failed to compile graphics pipeline: image binding field path is empty.");
        }
        parse_indexed_field_path(bindpoint_declaration.field_path);
    }
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.sampler_declerations) {
        if (bindpoint_declaration.field_path.empty()) {
            throw std::runtime_error("Failed to compile graphics pipeline: sampler binding field path is empty.");
        }
        parse_indexed_field_path(bindpoint_declaration.field_path);
    }
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.material_declerations) {
        if (bindpoint_declaration.field_path.empty()) {
            throw std::runtime_error("Failed to compile graphics pipeline: material binding field path is empty.");
        }
        parse_indexed_field_path(bindpoint_declaration.field_path);
    }
}

const RenderBindpointBufferDeclaration* find_uniform_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path) {
    // The binding list is small and call-site ordered, so a linear scan keeps ownership and lifetime simple.
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        if (bindpoint_declaration.field_path == p_field_path) {
            return &bindpoint_declaration;
        }
    }
    return nullptr;
}

const RenderBindpointBufferDeclaration* find_indexed_uniform_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path, uint32_t p_array_index) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
        if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == p_array_index) {
            return &bindpoint_declaration;
        }
    }
    return nullptr;
}

bool has_unindexed_uniform_binding(const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
        if (indexed_path.field_path == p_field_path && !indexed_path.array_index.has_value()) {
            return true;
        }
    }
    return false;
}

void validate_indexed_uniform_bindings(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path, uint32_t p_descriptor_count) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
        if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index >= p_descriptor_count) {
            throw std::runtime_error(std::format(
                "Failed to bind uniform array '{}': index {} is out of bounds.", p_field_path, *indexed_path.array_index));
        }
    }
}

std::vector<uint8_t> build_uniform_array_data(const ShaderDescriptorBindingVK& p_descriptor_binding,
    const RenderBindpointDeclarations& p_bindpoint_declarations,
    uint32_t p_descriptor_count) {
    if (has_unindexed_uniform_binding(p_bindpoint_declarations, p_descriptor_binding.parameter_name)) {
        throw std::runtime_error(std::format("Failed to bind uniform array '{}': array index must be specified with [].",
            p_descriptor_binding.parameter_name));
    }
    validate_indexed_uniform_bindings(p_bindpoint_declarations, p_descriptor_binding.parameter_name, p_descriptor_count);
    std::vector<uint8_t> uniform_data(p_descriptor_binding.buffer_size * p_descriptor_count, 0);
    for (uint32_t i = 0; i < p_descriptor_count; ++i) {
        const auto* bindpoint_declaration =
            find_indexed_uniform_binding(p_bindpoint_declarations, p_descriptor_binding.parameter_name, i);
        if (bindpoint_declaration == nullptr) {
            continue;
        }
        UniformDataView data_view = get_uniform_data(p_bindpoint_declarations, *bindpoint_declaration);
        if (data_view.size > p_descriptor_binding.buffer_size) {
            throw std::runtime_error(std::format("Failed to bind uniform buffer '{}[{}]': program buffer size {} exceeds shader buffer size {}.",
                p_descriptor_binding.parameter_name,
                i,
                data_view.size,
                p_descriptor_binding.buffer_size));
        }
        std::memcpy(uniform_data.data() + p_descriptor_binding.buffer_size * i, data_view.data, data_view.size);
    }
    return uniform_data;
}

std::vector<uint8_t> build_uniform_data(
    const ShaderDescriptorBindingVK& p_descriptor_binding, const RenderBindpointDeclarations& p_bindpoint_declarations) {
    // Pack caller-provided field values into a zero-initialized buffer matching the shader-reflected layout.
    if (p_descriptor_binding.buffer_size == 0) {
        throw std::runtime_error(std::format("Failed to compile uniform descriptor '{}': reflected buffer size is missing.",
            p_descriptor_binding.parameter_name));
    }
    uint32_t descriptor_count = std::max(p_descriptor_binding.layout_binding.descriptorCount, 1U);
    if (descriptor_count > 1) {
        return build_uniform_array_data(p_descriptor_binding, p_bindpoint_declarations, descriptor_count);
    }
    std::vector<uint8_t> uniform_data(p_descriptor_binding.buffer_size, 0);
    const auto* direct_bindpoint_declaration = find_uniform_binding(p_bindpoint_declarations, p_descriptor_binding.parameter_name);
    if (direct_bindpoint_declaration != nullptr) {
        // A declaration keyed by the descriptor parameter name replaces field-by-field packing for complex UBO layouts.
        UniformDataView data_view = get_uniform_data(p_bindpoint_declarations, *direct_bindpoint_declaration);
        if (data_view.size > uniform_data.size()) {
            throw std::runtime_error(std::format("Failed to bind uniform buffer '{}': program buffer size {} exceeds shader buffer size {}.",
                p_descriptor_binding.parameter_name,
                data_view.size,
                uniform_data.size()));
        }
        std::memcpy(uniform_data.data(), data_view.data, data_view.size);
        return uniform_data;
    }
    for (const auto& field : p_descriptor_binding.fields) {
        // Fields omitted by the caller retain their zero-initialized default value.
        const auto* bindpoint_declaration = find_uniform_binding(p_bindpoint_declarations, field.field_path);
        if (bindpoint_declaration == nullptr) {
            continue;
        }
        UniformDataView data_view = get_uniform_data(p_bindpoint_declarations, *bindpoint_declaration);
        if (data_view.size != field.size) {
            throw std::runtime_error(std::format("Failed to bind uniform field '{}': program field size {} does not match shader field size {}.",
                field.field_path,
                data_view.size,
                field.size));
        }
        if (field.offset + field.size > uniform_data.size()) {
            throw std::runtime_error(std::format(
                "Failed to bind uniform field '{}': reflected field range exceeds uniform buffer size.", field.field_path));
        }
        // Copy into the reflected offset so padding and field ordering match the shader uniform block.
        std::memcpy(uniform_data.data() + field.offset, data_view.data, field.size);
    }
    return uniform_data;
}

bool has_texture_descriptor_bindings(const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings, uint32_t p_set_index) {
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        if (descriptor_binding.set_index != p_set_index) {
            continue;
        }
        VkDescriptorType descriptor_type = descriptor_binding.layout_binding.descriptorType;
        if (descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER ||
            descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            return true;
        }
    }
    return false;
}

bool requires_texture_image(const ShaderDescriptorBindingVK& p_descriptor_binding) {
    VkDescriptorType descriptor_type = p_descriptor_binding.layout_binding.descriptorType;
    return descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
}

void validate_texture_bindpoint_sources(std::string_view p_resource_key,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    const RenderBindpointDeclarations& p_bindpoint_declarations,
    bool p_has_default_image) {
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        if (!requires_texture_image(descriptor_binding)) {
            continue;
        }
        if (find_material_bindpoint(p_bindpoint_declarations, descriptor_binding.parameter_name) != nullptr) {
            continue;
        }
        if (p_has_default_image) {
            continue;
        }
        if (!has_texture_bindpoint(p_bindpoint_declarations, descriptor_binding.parameter_name)) {
            throw std::runtime_error(std::format("Failed to compile graphics pipeline '{}': no texture bindpoint declaration was provided for '{}'.",
                p_resource_key,
                descriptor_binding.parameter_name));
        }
    }
}

std::string get_stage_entry(RefRes<char> p_stage_entry) {
    // Empty resource data means the pipeline should resolve the stage entry from reflected shader metadata.
    if (p_stage_entry == nullptr || p_stage_entry.get()[0] == '\0') {
        return {};
    }
    return std::string(p_stage_entry.get());
}

std::string get_shader_resource_stage_entry(RefRes<RUShader> p_shader, std::string_view p_stage) {
    if (p_shader == nullptr) {
        return {};
    }
    RefRes<char> stage_entry;
    if (p_stage == "vertex") {
        stage_entry = p_shader->vertex_entry;
    } else if (p_stage == "fragment") {
        stage_entry = p_shader->fragment_entry;
    } else if (p_stage == "geometry") {
        stage_entry = p_shader->geometry_entry;
    } else if (p_stage == "tesscontrol" || p_stage == "tesseval") {
        stage_entry = p_shader->tessellation_entry;
    } else if (p_stage == "compute") {
        stage_entry = p_shader->compute_entry;
    }
    return get_stage_entry(stage_entry);
}

std::string resolve_pipeline_stage_entry(
    RefRes<RUShader> p_shader, const ShaderMetadata& p_metadata, std::string_view p_stage, RefRes<char> p_stage_entry) {
    std::string resolved_stage_entry = get_stage_entry(p_stage_entry);
    if (resolved_stage_entry.empty()) {
        resolved_stage_entry = get_shader_resource_stage_entry(p_shader, p_stage);
    }
    return std::string(resolve_shader_stage_entry_point(p_metadata, p_stage, resolved_stage_entry).name);
}

template <typename RenderObjectType>
Ref<RenderObjectType> get_registered_render_object(
    Ref<RenderDatabase> p_render_database, std::string_view p_resource_key, HashCode p_render_object_type_id) {
    // Render objects are cached by resource key and render-object type so each subresource can be reused independently.
    auto render_object = p_render_database->get_compiled_render_resource(p_resource_key, p_render_object_type_id);
    if (render_object == MEM_NULL) {
        return MEM_NULL;
    }
    return render_object.dynamic_cast_ref<RenderObjectType>();
}

GraphicsPipelineCompilerVK::DescriptorSetBindings build_descriptor_set_bindings(
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings) {
    GraphicsPipelineCompilerVK::DescriptorSetBindings result;
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        result.emplace(descriptor_binding.parameter_name,
            GraphicsPipelineCompilerVK::DescriptorSetBinding{descriptor_binding.set_index, descriptor_binding.layout_binding.binding});
    }
    return result;
}

} // namespace

GraphicsPipelineCompilerVK::GraphicsPipelineCompilerVK(const GraphicsPipelineCompilerVKContext& p_context)
    : context(p_context) {
    // Reject incomplete context up front because every compile phase depends on these shared render services.
    if (context.allocator == nullptr || context.render_data == nullptr || context.render_database == MEM_NULL ||
        context.renderer == nullptr || context.color_attachment_formats.empty()) {
        throw std::runtime_error("Failed to create graphics pipeline compiler: invalid context.");
    }
}

GraphicsPipelineCompilerVK::CompileResult GraphicsPipelineCompilerVK::compile(std::string_view p_resource_key,
    RefRes<RUGraphicsPipeline> p_pipeline_resource,
    const RenderBindpointDeclarations& p_bindpoint_declarations) {
    auto compiled = compile_pipeline_objects(p_resource_key, p_pipeline_resource, p_bindpoint_declarations);
    return CompileResult{.graphics_pipeline = compiled.graphics_pipeline,
        .descriptor_sets = std::move(compiled.descriptor_sets),
        .descriptor_set_bindings = std::move(compiled.descriptor_set_bindings)};
}

GraphicsPipelineCompilerVK::CompiledPipelineObjects GraphicsPipelineCompilerVK::compile_pipeline_objects(
    std::string_view p_resource_key, RefRes<RUGraphicsPipeline> p_pipeline_resource, const RenderBindpointDeclarations& p_bindpoint_declarations) {
    // Compile order follows Vulkan dependencies: shaders and metadata first, then layouts, then pipeline and descriptor sets.
    validate_bindpoint_declarations(p_bindpoint_declarations);
    if (p_pipeline_resource->vertex_stage == nullptr || p_pipeline_resource->fragment_stage == nullptr) {
        throw std::runtime_error(
            std::format("Failed to compile graphics pipeline '{}': vertex and fragment stages are required.", p_resource_key));
    }
    auto vertex_shader = compile_shader(p_pipeline_resource->vertex_stage);
    auto fragment_shader = compile_shader(p_pipeline_resource->fragment_stage);
    auto vertex_shader_metadata = load_shader_metadata(p_pipeline_resource->vertex_stage);
    auto fragment_shader_metadata = load_shader_metadata(p_pipeline_resource->fragment_stage);
    std::string vertex_stage_entry = resolve_pipeline_stage_entry(
        p_pipeline_resource->vertex_stage, vertex_shader_metadata, "vertex", p_pipeline_resource->vertex_stage_entry);
    std::string fragment_stage_entry = resolve_pipeline_stage_entry(
        p_pipeline_resource->fragment_stage, fragment_shader_metadata, "fragment", p_pipeline_resource->fragment_stage_entry);
    auto descriptor_bindings = collect_graphics_pipeline_descriptor_bindings(
        vertex_shader_metadata, vertex_stage_entry, fragment_shader_metadata, fragment_stage_entry);
    auto descriptor_set_layouts = compile_descriptor_set_layouts(p_resource_key, descriptor_bindings);
    auto pipeline_layout = compile_pipeline_layout(p_resource_key, descriptor_set_layouts);
    CompiledPipelineObjects result;
    result.graphics_pipeline = compile_graphics_pipeline(
        p_resource_key, *p_pipeline_resource, vertex_shader, fragment_shader, vertex_stage_entry, fragment_stage_entry, pipeline_layout);
    result.descriptor_sets =
        compile_descriptor_sets(p_resource_key, descriptor_set_layouts, descriptor_bindings, p_bindpoint_declarations);
    result.descriptor_set_bindings = build_descriptor_set_bindings(descriptor_bindings);
    return result;
}

Ref<ROShaderVK> GraphicsPipelineCompilerVK::compile_shader(RefRes<RUShader> p_shader) {
    // A propagated resource key is required because multiple graphics pipelines can share the same shader resource.
    if (p_shader == nullptr || p_shader->resource_key == nullptr) {
        throw std::runtime_error("Failed to compile shader: shader resource key was not propagated.");
    }
    std::string shader_key = p_shader->resource_key;
    auto registered_shader = get_registered_render_object<ROShaderVK>(context.render_database, shader_key, TypeUUID<ROShaderVK>::UUID);
    if (registered_shader != MEM_NULL) {
        // Reuse an existing shader object instead of creating duplicate Vulkan shader modules for the same resource.
        return registered_shader;
    }
    auto shader = ROShaderVK::Builder().set_code(p_shader).create_ref(context.render_data);
    context.render_database->register_compiled_render_resource(shader_key, TypeUUID<ROShaderVK>::UUID, RefWeak<IRenderObject>(shader));
    return shader;
}

std::vector<Ref<RODescriptorSetLayoutVK>> GraphicsPipelineCompilerVK::compile_descriptor_set_layouts(
    std::string_view p_resource_key, const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings) {
    // Descriptor set layouts are compiler-generated subresources derived from shader metadata.
    std::vector<Ref<RODescriptorSetLayoutVK>> result;
    auto descriptor_set_layout_bindings = build_graphics_pipeline_descriptor_set_layout_bindings(p_descriptor_bindings);
    set_variable_descriptor_count_upper_bounds(
        descriptor_set_layout_bindings, context.render_data->max_variable_sampled_image_descriptor_count);
    result.reserve(descriptor_set_layout_bindings.size());
    for (const auto& descriptor_set_layout_binding : descriptor_set_layout_bindings) {
        // Descriptor set layouts are compiler-generated subresources of the
        // graphics pipeline, so they use derived resource keys.
        std::string descriptor_set_layout_key =
            std::format("{}#descriptor_set_layout_{}", p_resource_key, descriptor_set_layout_binding.set_index);
        auto descriptor_set_layout = get_registered_render_object<RODescriptorSetLayoutVK>(
            context.render_database, descriptor_set_layout_key, TypeUUID<RODescriptorSetLayoutVK>::UUID);
        if (descriptor_set_layout == MEM_NULL) {
            // Missing cache entry: build the layout from every reflected binding in this set.
            RODescriptorSetLayoutVK::Builder builder;
            for (size_t i = 0; i < descriptor_set_layout_binding.bindings.size(); ++i) {
                VkDescriptorBindingFlags binding_flags = descriptor_set_layout_binding.binding_flags[i];
                builder.add_binding(descriptor_set_layout_binding.bindings[i], binding_flags);
            }
            descriptor_set_layout = builder.create_ref(context.render_data);
            context.render_database->register_compiled_render_resource(
                descriptor_set_layout_key, TypeUUID<RODescriptorSetLayoutVK>::UUID, RefWeak<IRenderObject>(descriptor_set_layout));
        }
        result.push_back(descriptor_set_layout);
    }
    return result;
}

Ref<ROPipelineLayoutVK> GraphicsPipelineCompilerVK::compile_pipeline_layout(
    std::string_view p_resource_key, const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts) {
    // Pipeline layouts are keyed as pipeline subresources because they are generated from this pipeline's descriptor sets.
    std::string pipeline_layout_key = std::format("{}#pipeline_layout", p_resource_key);
    auto pipeline_layout = get_registered_render_object<ROPipelineLayoutVK>(
        context.render_database, pipeline_layout_key, TypeUUID<ROPipelineLayoutVK>::UUID);
    if (pipeline_layout != MEM_NULL) {
        // Reuse avoids rebuilding identical Vulkan pipeline layouts during repeated compile calls.
        return pipeline_layout;
    }
    ROPipelineLayoutVK::Builder builder;
    for (const auto& descriptor_set_layout : p_descriptor_set_layouts) {
        // Preserve descriptor set order because Vulkan pipeline layouts are indexed by set number.
        builder.add_descriptor_set_layout(descriptor_set_layout);
    }
    pipeline_layout = builder.create_ref(context.render_data);
    context.render_database->register_compiled_render_resource(
        pipeline_layout_key, TypeUUID<ROPipelineLayoutVK>::UUID, RefWeak<IRenderObject>(pipeline_layout));
    return pipeline_layout;
}

Ref<ROGraphicsPipelineVK> GraphicsPipelineCompilerVK::compile_graphics_pipeline(std::string_view p_resource_key,
    const RUGraphicsPipeline& p_pipeline_resource,
    Ref<ROShaderVK> p_vertex_shader,
    Ref<ROShaderVK> p_fragment_shader,
    std::string_view p_vertex_stage_entry,
    std::string_view p_fragment_stage_entry,
    Ref<ROPipelineLayoutVK> p_pipeline_layout) {
    auto graphics_pipeline = get_registered_render_object<ROGraphicsPipelineVK>(
        context.render_database, p_resource_key, TypeUUID<ROGraphicsPipelineVK>::UUID);
    if (graphics_pipeline != MEM_NULL) {
        // The final pipeline is keyed by the graphics pipeline resource itself.
        return graphics_pipeline;
    }
    // Shader config connects compiled shader modules to their manifest-defined entry points.
    ROGraphicsPipelineVK::Config::ShaderConfig shader_config;
    shader_config.set_shaders(p_vertex_shader, p_fragment_shader, p_vertex_stage_entry, p_fragment_stage_entry);
    auto pipeline_builder =
        ROGraphicsPipelineVK::Builder().default_render_opaque_configuration(context.render_data, shader_config, p_pipeline_layout);
    pipeline_builder.config.attachment_config.set_color_formats(context.color_attachment_formats)
        .set_depth_format(context.depth_attachment_format)
        .set_stencil_format(context.stencil_attachment_format);
    // default_render_opaque_configuration adds one color attachment; add
    // additional ones to match the dynamic-rendering color attachment count.
    for (uint32_t additional_index = 1; additional_index < context.color_attachment_formats.size(); ++additional_index) {
        // Keep the blend attachment array aligned with the dynamic-rendering color attachment array.
        pipeline_builder.config.color_blend.add_default_color_attachment();
    }
    // The resource unit owns depth behavior so asset manifests can choose between opaque and depth-read-only passes.
    pipeline_builder.config.depth_test.set_depth_test(p_pipeline_resource.depth_test ? VK_TRUE : VK_FALSE);
    pipeline_builder.config.depth_test.set_depth_write(p_pipeline_resource.depth_write ? VK_TRUE : VK_FALSE);
    graphics_pipeline = pipeline_builder.create_ref(context.render_data);
    context.render_database->register_compiled_render_resource(
        p_resource_key, TypeUUID<ROGraphicsPipelineVK>::UUID, RefWeak<IRenderObject>(graphics_pipeline));
    return graphics_pipeline;
}

std::vector<Ref<RODescriptorSetVK>> GraphicsPipelineCompilerVK::compile_descriptor_sets(std::string_view p_resource_key,
    const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    const RenderBindpointDeclarations& p_bindpoint_declarations) {
    // Pipelines with no descriptor sets need no descriptor allocation or material lookup.
    if (p_descriptor_set_layouts.empty()) {
        return {};
    }
    validate_texture_bindpoint_sources(
        p_resource_key, p_descriptor_bindings, p_bindpoint_declarations, context.default_image_resource != nullptr);

    std::vector<Ref<RODescriptorSetVK>> descriptor_sets;
    descriptor_sets.reserve(p_descriptor_set_layouts.size());
    for (uint32_t set_index = 0; set_index < p_descriptor_set_layouts.size(); ++set_index) {
        // Build an empty descriptor set when the set only contains caller-provided uniforms; otherwise bind texture descriptors too.
        Ref<RODescriptorSetVK> descriptor_set =
            !has_texture_descriptor_bindings(p_descriptor_bindings, set_index)
                ? RODescriptorSetVK::Builder()
                      .add_descriptor_set_layout(p_descriptor_set_layouts[set_index])
                      .create_ref(context.render_data)
                : compile_texture_descriptor_set(
                      p_resource_key, p_descriptor_set_layouts[set_index], p_descriptor_bindings, set_index, p_bindpoint_declarations);
        // Uniform descriptors are written after material bindings so both sources can populate the same descriptor set.
        write_uniform_descriptors(descriptor_set, p_descriptor_bindings, set_index, p_bindpoint_declarations);
        descriptor_sets.push_back(descriptor_set);
    }
    return descriptor_sets;
}

Ref<RODescriptorSetVK> GraphicsPipelineCompilerVK::compile_texture_descriptor_set(std::string_view p_resource_key,
    Ref<RODescriptorSetLayoutVK> p_descriptor_set_layout,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    uint32_t p_set_index,
    const RenderBindpointDeclarations& p_bindpoint_declarations) {
    auto default_sampler = ROSamplerVK::Builder().create_ref(context.render_data);
    ROMaterialVK::Builder builder;
    builder.set_descriptor_set_layout(p_descriptor_set_layout)
        .set_set_index(p_set_index)
        .set_descriptor_bindings(p_descriptor_bindings)
        .set_default_sampler(default_sampler);
    if (context.default_image_resource != nullptr) {
        builder.set_default_texture(compile_image_view(std::format("{}#default_texture", p_resource_key), context.default_image_resource),
            VK_IMAGE_LAYOUT_GENERAL);
    }
    add_direct_texture_bindpoints(p_resource_key, builder, p_bindpoint_declarations);
    add_direct_sampler_bindpoints(p_resource_key, builder, p_bindpoint_declarations);
    add_material_texture_bindpoints(p_resource_key, builder, p_descriptor_bindings, p_set_index, p_bindpoint_declarations);
    return builder.create_ref(context.render_data)->get_descriptor_set();
}

void GraphicsPipelineCompilerVK::add_direct_texture_bindpoints(
    std::string_view p_resource_key, ROMaterialVK::Builder& p_builder, const RenderBindpointDeclarations& p_bindpoint_declarations) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
        // Add raw texture bindpoints before material slots so explicit per-compile inputs win on duplicate texture keys.
        IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
        ROMaterialVK::TextureBinding texture_binding{
            .texture_key = indexed_path.field_path,
            .array_index = indexed_path.array_index.value_or(0),
            .has_array_index = indexed_path.array_index.has_value(),
            .image_layout = VK_IMAGE_LAYOUT_GENERAL,
        };
        if (bindpoint_declaration.image_resource != nullptr) {
            size_t image_resource_hash = std::hash<RefRes<RUImage>>{}(bindpoint_declaration.image_resource);
            std::string image_view_resource_key =
                std::format("{}#texture_{}#resource_{}", p_resource_key, bindpoint_declaration.field_path, image_resource_hash);
            texture_binding.image_view = compile_image_view(image_view_resource_key, bindpoint_declaration.image_resource);
        } else {
            auto image_view = bindpoint_declaration.image_render_object.dynamic_cast_ref<ROImageViewVK>();
            if (image_view == MEM_NULL) {
                throw std::runtime_error(std::format("Failed to compile graphics pipeline '{}': image view texture '{}' is missing.",
                    p_resource_key,
                    bindpoint_declaration.field_path));
            }
            texture_binding.image_view = image_view;
        }
        p_builder.add_texture_binding(texture_binding);
    }
}

void GraphicsPipelineCompilerVK::add_direct_sampler_bindpoints(
    std::string_view p_resource_key, ROMaterialVK::Builder& p_builder, const RenderBindpointDeclarations& p_bindpoint_declarations) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.sampler_declerations) {
        IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
        auto sampler = bindpoint_declaration.sampler_render_object.dynamic_cast_ref<ROSamplerVK>();
        if (sampler == MEM_NULL) {
            throw std::runtime_error(std::format(
                "Failed to compile graphics pipeline '{}': sampler '{}' is missing.", p_resource_key, bindpoint_declaration.field_path));
        }
        p_builder.add_sampler_binding(ROMaterialVK::SamplerBinding{.sampler_key = indexed_path.field_path,
            .array_index = indexed_path.array_index.value_or(0),
            .has_array_index = indexed_path.array_index.has_value(),
            .sampler = sampler});
    }
}

void GraphicsPipelineCompilerVK::add_material_texture_bindpoints(std::string_view p_resource_key,
    ROMaterialVK::Builder& p_builder,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    uint32_t p_set_index,
    const RenderBindpointDeclarations& p_bindpoint_declarations) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.material_declerations) {
        const auto* material_unit = context.render_database->find_opaque_material(bindpoint_declaration.material_key);
        if (material_unit == nullptr || material_unit->material == nullptr) {
            throw std::runtime_error(std::format("Failed to compile graphics pipeline '{}': material '{}' was not found in the render database.",
                p_resource_key,
                bindpoint_declaration.material_key));
        }
        auto material = material_unit->material;
        bool material_used_by_set = false;
        for (const auto& descriptor_binding : p_descriptor_bindings) {
            if (descriptor_binding.set_index == p_set_index && requires_texture_image(descriptor_binding) &&
                descriptor_name_matches_bindpoint_scope(descriptor_binding.parameter_name, bindpoint_declaration.field_path)) {
                material_used_by_set = true;
                break;
            }
        }
        if (!material_used_by_set) {
            continue;
        }
        std::span<RUMaterial::TextureSlot> texture_slots(material->textures.get(), material->texture_count);
        for (const auto& texture_slot : texture_slots) {
            std::string texture_key = text_resource_value(texture_slot.texture_key);
            if (texture_slot.texture == nullptr) {
                throw std::runtime_error(
                    std::format("Failed to compile material '{}': texture '{}' is missing.", material->resource_key, texture_key));
            }
            std::string scoped_texture_key = std::format("{}.{}", bindpoint_declaration.field_path, texture_key);
            p_builder.add_texture_binding(ROMaterialVK::TextureBinding{.texture_key = scoped_texture_key,
                .image_view = compile_image_view(
                    std::format("{}#texture_{}", material->resource_key, scoped_texture_key), texture_slot.texture),
                .image_layout = VK_IMAGE_LAYOUT_GENERAL});
        }
    }
}

Ref<ROImageViewVK> GraphicsPipelineCompilerVK::compile_image_view(std::string_view p_resource_key, RefRes<RUImage> p_image_resource) {
    // Image views use a derived key because several shader bindings may point at the same image resource.
    std::string formatted_resource_key = std::format("{}{}", p_resource_key, "#image_view");
    auto image_view = get_registered_render_object<ROImageViewVK>(
        context.render_database, formatted_resource_key, TypeUUID<ROImageViewVK>::UUID);
    if (image_view != MEM_NULL) {
        // Reuse the view together with its already-compiled backing image.
        return image_view;
    }
    VkFormat image_format = image_format_from_resource(*p_image_resource);
    auto image = compile_image(formatted_resource_key, p_image_resource, image_format);
    image_view =
        ROImageViewVK::Builder()
            .set_image(image)
            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
            .set_format(image_format)
            .set_subresource_range(VkImageSubresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1})
            .create_ref(context.render_data);
    context.render_database->register_compiled_render_resource(
        formatted_resource_key, TypeUUID<ROImageViewVK>::UUID, RefWeak<IRenderObject>(image_view));
    return image_view;
}

Ref<ROImageVK> GraphicsPipelineCompilerVK::compile_image(
    std::string_view p_resource_key, RefRes<RUImage> p_image_resource, VkFormat p_image_format) {
    // Texture images are cached independently from image views so later bindings can reuse uploaded GPU data.
    auto image = get_registered_render_object<ROImageVK>(context.render_database, p_resource_key, TypeUUID<ROImageVK>::UUID);
    if (image != MEM_NULL) {
        // Cached images already have their upload task scheduled or completed by the original compile.
        return image;
    }
    image = ROImageVK::Builder()
                .set_image_type(VK_IMAGE_TYPE_2D)
                .set_format(p_image_format)
                .set_extent(VkExtent3D{.width = p_image_resource->width, .height = p_image_resource->height, .depth = 1})
                .set_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                .create_ref(context.render_data);
    context.render_database->register_compiled_render_resource(p_resource_key, TypeUUID<ROImageVK>::UUID, RefWeak<IRenderObject>(image));

    auto upload_data = build_image_upload_data(*p_image_resource);
    auto upload_buffer = ROBufferVK::Builder()
                             .set_domain(DeviceDomain::HOST)
                             .set_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                             .set_size(upload_data.size())
                             .create_ref(context.render_data);
    upload_buffer->update_buffer(upload_data.data(), upload_data.size());
    context.renderer->add_pre_execute_task([image, upload_buffer, width = p_image_resource->width, height = p_image_resource->height](
                                               VkCommandBuffer p_command_buffer) mutable {
        // Defer the image copy until a command buffer is available from the renderer's pre-execute phase.
        VkBufferImageCopy region_info{};
        region_info.imageSubresource =
            VkImageSubresourceLayers{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1};
        region_info.imageExtent = VkExtent3D{.width = width, .height = height, .depth = 1};
        image->set_image_data_immediate(p_command_buffer, upload_buffer, region_info);
    });
    return image;
}

void GraphicsPipelineCompilerVK::write_uniform_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    uint32_t p_set_index,
    const RenderBindpointDeclarations& p_bindpoint_declarations) const {
    // Scan all reflected bindings and write only uniform buffers belonging to this descriptor set index.
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        if (descriptor_binding.set_index != p_set_index ||
            descriptor_binding.layout_binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            // Non-uniform descriptors are handled by material compilation; other sets are written in their own iteration.
            continue;
        }
        // Allocate one host-visible buffer per reflected uniform descriptor and write it into the descriptor set.
        auto uniform_data = build_uniform_data(descriptor_binding, p_bindpoint_declarations);
        auto uniform_buffer = ROBufferVK::Builder()
                                  .set_domain(DeviceDomain::HOST_DEVICE)
                                  .set_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                  .set_size(uniform_data.size())
                                  .create_ref(context.render_data);
        uniform_buffer->update_buffer(uniform_data.data(), uniform_data.size());
        RODescriptorSetVK::Writer writer;
        writer.set_binding(descriptor_binding.layout_binding.binding);
        uint32_t descriptor_count = std::max(descriptor_binding.layout_binding.descriptorCount, 1U);
        for (uint32_t i = 0; i < descriptor_count; ++i) {
            writer.add_uniform_buffer_info(RODescriptorSetVK::Writer::BufferInfo{
                .buffer = uniform_buffer,
                .offset = descriptor_binding.buffer_size * i,
                .range = descriptor_binding.buffer_size,
            });
        }
        p_descriptor_set->write_descriptor_set(writer);
    }
}

} // namespace WhiteBirdEngine
