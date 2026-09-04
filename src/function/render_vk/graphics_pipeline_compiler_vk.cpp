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
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render/shader_metadata_query.hh"
#include "function/render_vk/ktx_vulkan_adapter.hh"
#include "function/render_vk/pipeline_compiler_vk.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_material_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
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

std::string texture_resource_cache_key(RefRes<RUTexture> p_texture_resource) {
    if (p_texture_resource == nullptr) {
        throw std::runtime_error("Failed to build texture cache key: texture resource is missing.");
    }
    return std::format("texture_resource_{}", reinterpret_cast<uintptr_t>(p_texture_resource.get()));
}

bool has_texture_bindpoint(const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path) {
    // Raw texture bindings are matched by shader texture parameter name, independent of material texture slots.
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
        if (parse_indexed_field_path(bindpoint_declaration.field_path).field_path == p_field_path) {
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
        if (bindpoint_declaration.buffer_render_object != MEM_NULL) {
            continue;
        }
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
}

const RenderBindpointBufferDeclaration* find_uniform_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path) {
    // The binding list is small and call-site ordered, so a linear scan keeps ownership and lifetime simple.
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        if (bindpoint_declaration.field_path == p_field_path && bindpoint_declaration.buffer_render_object == MEM_NULL) {
            return &bindpoint_declaration;
        }
    }
    return nullptr;
}

const RenderBindpointBufferDeclaration* find_indexed_uniform_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path, uint32_t p_array_index) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        if (bindpoint_declaration.buffer_render_object != MEM_NULL) {
            continue;
        }
        IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
        if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == p_array_index) {
            return &bindpoint_declaration;
        }
    }
    return nullptr;
}

bool has_unindexed_uniform_binding(const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        if (bindpoint_declaration.buffer_render_object != MEM_NULL) {
            continue;
        }
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
        // A declaration keyed by the descriptor parameter name replaces field-by-field packing for complex uniform-buffer layouts.
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
    return std::string(ShaderMetadataQuery().resolve_entry_point(p_metadata, p_stage, resolved_stage_entry).name);
}

} // namespace

GraphicsPipelineCompilerVK::GraphicsPipelineCompilerVK(const GraphicsPipelineCompilerVKContext& p_context)
    : PipelineCompilerVK(
          PipelineCompilerVKContext{
              .allocator = p_context.allocator,
              .render_data = p_context.render_data,
              .render_database = p_context.render_database,
          },
          "graphics pipeline"),
      context(p_context) {
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
    Ref<ROShaderVK> geometry_shader = MEM_NULL;
    auto fragment_shader = compile_shader(p_pipeline_resource->fragment_stage);
    auto vertex_shader_metadata = ShaderMetadataLoader().load(p_pipeline_resource->vertex_stage);
    std::optional<ShaderMetadata> geometry_shader_metadata;
    auto fragment_shader_metadata = ShaderMetadataLoader().load(p_pipeline_resource->fragment_stage);
    std::string vertex_stage_entry = resolve_pipeline_stage_entry(
        p_pipeline_resource->vertex_stage, vertex_shader_metadata, "vertex", p_pipeline_resource->vertex_stage_entry);
    std::string geometry_stage_entry;
    if (p_pipeline_resource->geometry_stage != nullptr) {
        geometry_shader = compile_shader(p_pipeline_resource->geometry_stage);
        geometry_shader_metadata = ShaderMetadataLoader().load(p_pipeline_resource->geometry_stage);
        geometry_stage_entry = resolve_pipeline_stage_entry(
            p_pipeline_resource->geometry_stage, *geometry_shader_metadata, "geometry", p_pipeline_resource->geometry_stage_entry);
    }
    std::string fragment_stage_entry = resolve_pipeline_stage_entry(
        p_pipeline_resource->fragment_stage, fragment_shader_metadata, "fragment", p_pipeline_resource->fragment_stage_entry);
    auto descriptor_bindings = collect_graphics_pipeline_descriptor_bindings(vertex_shader_metadata,
        vertex_stage_entry,
        geometry_shader_metadata.has_value() ? &(*geometry_shader_metadata) : nullptr,
        geometry_stage_entry,
        fragment_shader_metadata,
        fragment_stage_entry);
    auto descriptor_set_layouts = compile_descriptor_set_layouts(p_resource_key, descriptor_bindings);
    auto pipeline_layout = compile_pipeline_layout(p_resource_key, descriptor_set_layouts);
    CompiledPipelineObjects result;
    result.graphics_pipeline = compile_graphics_pipeline(
        p_resource_key, *p_pipeline_resource, vertex_shader, geometry_shader, fragment_shader, vertex_stage_entry, geometry_stage_entry, fragment_stage_entry, pipeline_layout);
    result.graphics_pipeline->set_descriptor_sets(
        compile_descriptor_sets(p_resource_key, descriptor_set_layouts, descriptor_bindings, p_bindpoint_declarations));
    result.descriptor_set_bindings = build_descriptor_set_bindings(descriptor_bindings);
    return result;
}

Ref<ROGraphicsPipelineVK> GraphicsPipelineCompilerVK::compile_graphics_pipeline(std::string_view p_resource_key, // NOLINT
    const RUGraphicsPipeline& p_pipeline_resource,
    Ref<ROShaderVK> p_vertex_shader,
    Ref<ROShaderVK> p_geometry_shader,
    Ref<ROShaderVK> p_fragment_shader,
    std::string_view p_vertex_stage_entry,
    std::string_view p_geometry_stage_entry,
    std::string_view p_fragment_stage_entry,
    Ref<ROPipelineLayoutVK> p_pipeline_layout) {
    // Descriptor sets are pipeline-owned, so each compile result needs an independent runtime pipeline object. Shaders,
    // descriptor-set layouts, and pipeline layouts remain cached by the shared compiler helpers.
    // Shader config connects compiled shader modules to their manifest-defined entry points.
    ROGraphicsPipelineVK::Config::ShaderConfig shader_config;
    shader_config.set_shaders(p_vertex_shader, p_fragment_shader, p_vertex_stage_entry, p_fragment_stage_entry);
    if (p_geometry_shader != MEM_NULL) {
        shader_config.set_geometry_shader(p_geometry_shader, p_geometry_stage_entry);
    }
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
    pipeline_builder.config.rasterization.set_cull_mode(p_pipeline_resource.cull_backfaces ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
    Ref<ROGraphicsPipelineVK> graphics_pipeline = pipeline_builder.create_ref(context.render_data);
    context.render_database->register_compiled_render_resource(p_resource_key, graphics_pipeline);
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
        p_resource_key, p_descriptor_bindings, p_bindpoint_declarations, context.default_texture_resource != nullptr);
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
        write_uniform_descriptors(descriptor_set, p_descriptor_bindings, set_index, p_bindpoint_declarations, build_uniform_data);
        write_storage_buffer_descriptors(descriptor_set, p_descriptor_bindings, set_index, p_bindpoint_declarations);
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
    if (context.default_texture_resource != nullptr) {
        builder.set_default_texture(
            compile_texture_view(texture_resource_cache_key(context.default_texture_resource), context.default_texture_resource),
            VK_IMAGE_LAYOUT_GENERAL);
    }
    add_direct_texture_bindpoints(p_resource_key, builder, p_bindpoint_declarations);
    add_direct_sampler_bindpoints(p_resource_key, builder, p_bindpoint_declarations);
    return builder.create_ref(context.render_data)->get_descriptor_set();
}

void GraphicsPipelineCompilerVK::add_direct_texture_bindpoints(
    std::string_view p_resource_key, ROMaterialVK::Builder& p_builder, const RenderBindpointDeclarations& p_bindpoint_declarations) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
        // Add raw texture bindpoints before material slots so explicit per-compile inputs win on duplicate texture keys.
        IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
        ROMaterialVK::TextureBinding texture_binding{
            .field_path = indexed_path.field_path,
            .array_index = indexed_path.array_index.value_or(0),
            .has_array_index = indexed_path.array_index.has_value(),
            .image_layout = VK_IMAGE_LAYOUT_GENERAL,
        };
        if (bindpoint_declaration.image_resource != nullptr) {
            texture_binding.image_view = compile_texture_view(
                texture_resource_cache_key(bindpoint_declaration.image_resource), bindpoint_declaration.image_resource);
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

Ref<ROImageViewVK> GraphicsPipelineCompilerVK::compile_texture_view(std::string_view p_resource_key, RefRes<RUTexture> p_texture_resource) {
    // Texture views use a derived key because several shader bindings may point at the same texture resource.
    std::string formatted_resource_key = std::format("{}{}", p_resource_key, "#image_view");
    auto image_view = get_registered_render_object<ROImageViewVK>(
        context.render_database, formatted_resource_key, TypeUUID<ROImageViewVK>::UUID);
    if (image_view != MEM_NULL) {
        // Reuse the view together with its already-compiled backing image.
        return image_view;
    }
    auto image = compile_texture(formatted_resource_key, p_texture_resource);
    const ROImageVK::ExposedFields& image_fields = image->fields();
    image_view = ROImageViewVK::Builder()
                     .set_image(image)
                     .set_view_type(image_fields.view_type)
                     .set_format(image_fields.format)
                     .set_subresource_range(VkImageSubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                         .baseMipLevel = 0,
                         .levelCount = image_fields.mipmap_levels,
                         .baseArrayLayer = 0,
                         .layerCount = image_fields.layers_count})
                     .create_ref(context.render_data);
    context.render_database->register_compiled_render_resource(formatted_resource_key, image_view);
    return image_view;
}

Ref<ROImageVK> GraphicsPipelineCompilerVK::compile_texture(std::string_view p_resource_key, RefRes<RUTexture> p_texture_resource) {
    // Texture images are cached independently from image views so later bindings can reuse uploaded GPU data.
    auto image = get_registered_render_object<ROImageVK>(context.render_database, p_resource_key, TypeUUID<ROImageVK>::UUID);
    if (image != MEM_NULL) {
        // Cached textures were uploaded synchronously by the original compile.
        return image;
    }
    if (context.render_data->ktx_vulkan_device_info == nullptr) {
        throw std::runtime_error("Failed to compile texture: libktx Vulkan device info is unavailable.");
    }
    ktxVulkanTexture uploaded_texture = context.render_data->ktx_vulkan_device_info->upload(p_texture_resource->texture.get());
    image = make_ref<ROImageVK>(context.render_data->allocator, context.render_data, uploaded_texture);
    context.render_database->register_compiled_render_resource(p_resource_key, image);
    return image;
}

} // namespace WhiteBirdEngine
