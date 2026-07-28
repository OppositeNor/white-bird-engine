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
#include <any>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

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

bool is_material_bindpoint(const RenderBindpointDeclaration& p_bindpoint_declaration) {
    // Materials are supplied as bindpoint declarations rather than discovered from render resources.
    return std::any_cast<RefRes<RUMaterial>>(&p_bindpoint_declaration.data) != nullptr;
}

RefRes<RUMaterial> find_material_bindpoint(std::span<const RenderBindpointDeclaration> p_bindpoint_declarations) {
    // Use the first material bindpoint declaration as the material source for this pipeline compilation.
    for (const auto& bindpoint_declaration : p_bindpoint_declarations) {
        if (const auto* material = std::any_cast<RefRes<RUMaterial>>(&bindpoint_declaration.data)) {
            if (*material != nullptr) {
                return *material;
            }
        }
    }
    return MEM_NULL;
}

template <typename DataType>
const void* try_get_uniform_data(const std::any& p_data) {
    // std::any exposes data only through typed casts, so supported uniform value types are checked explicitly.
    const auto* value = std::any_cast<DataType>(&p_data);
    if (value == nullptr) {
        return nullptr;
    }
    return value;
}

struct UniformDataView {
    const void* data = nullptr;
    size_t size = 0;
};

template <size_t TypeIndex = 0>
UniformDataView get_supported_uniform_data(const std::any& p_data) {
    // Walk the supported uniform value type list until std::any contains the reflected field's exact type and size.
    using SupportedUniformDataTypes = GraphicsPipelineCompilerVK::SupportedUniformDataTypes;
    if constexpr (TypeIndex >= std::tuple_size_v<SupportedUniformDataTypes>) {
        return {};
    } else {
        using DataType = std::tuple_element_t<TypeIndex, SupportedUniformDataTypes>;
        if (const void* data = try_get_uniform_data<DataType>(p_data)) {
            return UniformDataView{.data = data, .size = sizeof(DataType)};
        }
        return get_supported_uniform_data<TypeIndex + 1>(p_data);
    }
}

UniformDataView get_uniform_data(const RenderBindpointDeclaration& p_bindpoint_declaration) {
    // The compiler copies supported scalar and GLM value types into reflected uniform buffers.
    UniformDataView data_view = get_supported_uniform_data(p_bindpoint_declaration.data);
    if (data_view.data != nullptr) {
        return data_view;
    }
    throw std::runtime_error(std::format(
        "Failed to bind uniform field '{}': unsupported std::any data type.", p_bindpoint_declaration.field_path));
}

void validate_bindpoint_declarations(std::span<const RenderBindpointDeclaration> p_bindpoint_declarations) {
    // Uniforms are optional, but every provided binding must be complete before descriptor writes begin.
    if (p_bindpoint_declarations.empty()) {
        return;
    }
    for (const auto& bindpoint_declaration : p_bindpoint_declarations) {
        if (!bindpoint_declaration.data.has_value()) {
            throw std::runtime_error("Failed to compile graphics pipeline: bindpoint declaration has no data.");
        }
        if (is_material_bindpoint(bindpoint_declaration)) {
            continue;
        }
        // Validate the lookup key first so later errors can identify the shader field path reliably.
        if (bindpoint_declaration.field_path.empty()) {
            throw std::runtime_error("Failed to compile graphics pipeline: uniform binding field path is empty.");
        }
    }
}

const RenderBindpointDeclaration* find_uniform_binding(
    std::span<const RenderBindpointDeclaration> p_bindpoint_declarations, std::string_view p_field_path) {
    // The binding list is small and call-site ordered, so a linear scan keeps ownership and lifetime simple.
    for (const auto& bindpoint_declaration : p_bindpoint_declarations) {
        if (!is_material_bindpoint(bindpoint_declaration) && bindpoint_declaration.field_path == p_field_path) {
            return &bindpoint_declaration;
        }
    }
    return nullptr;
}

std::vector<uint8_t> build_uniform_data(
    const ShaderDescriptorBindingVK& p_descriptor_binding, std::span<const RenderBindpointDeclaration> p_bindpoint_declarations) {
    // Pack caller-provided field values into a zero-initialized buffer matching the shader-reflected layout.
    if (p_descriptor_binding.buffer_size == 0) {
        throw std::runtime_error(std::format("Failed to compile uniform descriptor '{}': reflected buffer size is missing.",
            p_descriptor_binding.parameter_name));
    }
    if (p_descriptor_binding.fields.empty()) {
        throw std::runtime_error(std::format("Failed to compile uniform descriptor '{}': reflected uniform fields are missing.",
            p_descriptor_binding.parameter_name));
    }
    std::vector<uint8_t> uniform_data(p_descriptor_binding.buffer_size, 0);
    for (const auto& field : p_descriptor_binding.fields) {
        // Each reflected field must have a caller-provided binding with the exact byte size expected by the shader.
        const auto* bindpoint_declaration = find_uniform_binding(p_bindpoint_declarations, field.field_path);
        if (bindpoint_declaration == nullptr) {
            throw std::runtime_error(
                std::format("Failed to bind uniform field '{}': caller did not provide a binding.", field.field_path));
        }
        UniformDataView data_view = get_uniform_data(*bindpoint_declaration);
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

bool has_material_descriptor_bindings(const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings) {
    // Only sampled image and sampler descriptors require a static material resource at compile time.
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        VkDescriptorType descriptor_type = descriptor_binding.layout_binding.descriptorType;
        if (descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER ||
            descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            return true;
        }
    }
    return false;
}

std::string get_stage_entry(RefRes<char> p_stage_entry) {
    // Empty resource data means the shader stage should use the conventional SPIR-V entry point.
    if (p_stage_entry == nullptr || p_stage_entry.get()[0] == '\0') {
        return std::string("main");
    }
    return std::string(p_stage_entry.get());
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

} // namespace

GraphicsPipelineCompilerVK::GraphicsPipelineCompilerVK(const GraphicsPipelineCompilerVKContext& p_context)
    : context(p_context) {
    // Reject incomplete context up front because every compile phase depends on these shared render services.
    if (context.allocator == nullptr || context.render_data == nullptr || context.render_database == MEM_NULL ||
        context.renderer == nullptr || context.color_attachment_formats.empty()) {
        throw std::runtime_error("Failed to create graphics pipeline compiler: invalid context.");
    }
}

std::tuple<Ref<ROGraphicsPipelineVK>, std::vector<Ref<RODescriptorSetVK>>> GraphicsPipelineCompilerVK::compile(
    std::string_view p_resource_key,
    RefRes<RUGraphicsPipeline> p_pipeline_resource,
    std::span<const RenderBindpointDeclaration> p_bindpoint_declarations) {
    // Keep the public API tuple-shaped while the internal path names the intermediate pipeline objects.
    auto compiled = compile_pipeline_objects(p_resource_key, p_pipeline_resource, p_bindpoint_declarations);
    return {compiled.graphics_pipeline, compiled.descriptor_sets};
}

GraphicsPipelineCompilerVK::CompiledPipelineObjects GraphicsPipelineCompilerVK::compile_pipeline_objects(
    std::string_view p_resource_key,
    RefRes<RUGraphicsPipeline> p_pipeline_resource,
    std::span<const RenderBindpointDeclaration> p_bindpoint_declarations) {
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
    auto descriptor_bindings =
        collect_graphics_pipeline_descriptor_bindings(*p_pipeline_resource, vertex_shader_metadata, fragment_shader_metadata);
    auto descriptor_set_layouts = compile_descriptor_set_layouts(p_resource_key, descriptor_bindings);
    auto pipeline_layout = compile_pipeline_layout(p_resource_key, descriptor_set_layouts);
    CompiledPipelineObjects result;
    result.graphics_pipeline =
        compile_graphics_pipeline(p_resource_key, *p_pipeline_resource, vertex_shader, fragment_shader, pipeline_layout);
    result.descriptor_sets = compile_descriptor_sets(
        p_resource_key, *p_pipeline_resource, descriptor_set_layouts, descriptor_bindings, p_bindpoint_declarations);
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
            for (const auto& binding : descriptor_set_layout_binding.bindings) {
                builder.add_binding(binding);
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
    Ref<ROPipelineLayoutVK> p_pipeline_layout) {
    auto graphics_pipeline = get_registered_render_object<ROGraphicsPipelineVK>(
        context.render_database, p_resource_key, TypeUUID<ROGraphicsPipelineVK>::UUID);
    if (graphics_pipeline != MEM_NULL) {
        // The final pipeline is keyed by the graphics pipeline resource itself.
        return graphics_pipeline;
    }
    // Shader config connects compiled shader modules to their manifest-defined entry points.
    ROGraphicsPipelineVK::Config::ShaderConfig shader_config;
    shader_config.set_shaders(p_vertex_shader,
        p_fragment_shader,
        get_stage_entry(p_pipeline_resource.vertex_stage_entry),
        get_stage_entry(p_pipeline_resource.fragment_stage_entry));
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
    const RUGraphicsPipeline& /*p_pipeline_resource*/,
    const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    std::span<const RenderBindpointDeclaration> p_bindpoint_declarations) {
    // Pipelines with no descriptor sets need no descriptor allocation or material lookup.
    if (p_descriptor_set_layouts.empty()) {
        return {};
    }
    RefRes<RUMaterial> material = find_material_bindpoint(p_bindpoint_declarations);
    if (material == nullptr && has_material_descriptor_bindings(p_descriptor_bindings)) {
        // Material image bindings are static today, so they must come from a material already registered in render data.
        throw std::runtime_error(
            std::format("Failed to compile graphics pipeline '{}': no material bindpoint declaration was provided.", p_resource_key));
    }

    std::vector<Ref<RODescriptorSetVK>> descriptor_sets;
    descriptor_sets.reserve(p_descriptor_set_layouts.size());
    for (uint32_t set_index = 0; set_index < p_descriptor_set_layouts.size(); ++set_index) {
        // Build an empty descriptor set when the set only contains caller-provided uniforms; otherwise bind material textures too.
        Ref<RODescriptorSetVK> descriptor_set =
            material == nullptr
                ? RODescriptorSetVK::Builder()
                      .add_descriptor_set_layout(p_descriptor_set_layouts[set_index])
                      .create_ref(context.render_data)
                : compile_material_descriptor_set(
                      p_resource_key, material, p_descriptor_set_layouts[set_index], p_descriptor_bindings, set_index);
              // Uniform descriptors are written after material bindings so both sources can populate the same descriptor set.
              write_uniform_descriptors(descriptor_set, p_descriptor_bindings, set_index, p_bindpoint_declarations);
        descriptor_sets.push_back(descriptor_set);
    }
    return descriptor_sets;
}

Ref<RODescriptorSetVK> GraphicsPipelineCompilerVK::compile_material_descriptor_set(std::string_view p_resource_key,
    RefRes<RUMaterial> p_material,
    Ref<RODescriptorSetLayoutVK> p_descriptor_set_layout,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    uint32_t p_set_index) {
    // Material resource keys seed stable derived keys for texture image views.
    if (p_material->resource_key == nullptr) {
        throw std::runtime_error(
            std::format("Failed to compile graphics pipeline '{}': material resource key was not propagated.", p_resource_key));
    }
    auto default_sampler = ROSamplerVK::Builder().create_ref(context.render_data);
    ROMaterialVK::Builder builder;
    builder.set_descriptor_set_layout(p_descriptor_set_layout)
        .set_set_index(p_set_index)
        .set_descriptor_bindings(p_descriptor_bindings)
        .set_default_sampler(default_sampler);
    std::span<RUMaterial::TextureSlot> texture_slots(p_material->textures.get(), p_material->texture_count);
    for (const auto& texture_slot : texture_slots) {
        // Texture keys connect material slots to shader-reflected descriptor binding names.
        std::string texture_key = text_resource_value(texture_slot.texture_key);
        if (texture_slot.texture == nullptr) {
            throw std::runtime_error(
                std::format("Failed to compile material '{}': texture '{}' is missing.", p_material->resource_key, texture_key));
        }
        // Compile or reuse the texture image view before adding the binding to the material descriptor set builder.
        builder.add_texture_binding(ROMaterialVK::TextureBinding{
            .texture_key = texture_key,
            .image_view =
                compile_image_view(std::format("{}#texture_{}", p_material->resource_key, texture_key), texture_slot.texture),
            .image_layout = VK_IMAGE_LAYOUT_GENERAL,
        });
    }
    return builder.create_ref(context.render_data)->get_descriptor_set();
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
    std::span<const RenderBindpointDeclaration> p_bindpoint_declarations) const {
    // Scan all reflected bindings and write only uniform buffers belonging to this descriptor set index.
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        if (descriptor_binding.set_index != p_set_index ||
            descriptor_binding.layout_binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            // Non-uniform descriptors are handled by material compilation; other sets are written in their own iteration.
            continue;
        }
        if (descriptor_binding.layout_binding.descriptorCount != 1) {
            throw std::runtime_error(std::format("Failed to compile uniform descriptor '{}': descriptor arrays are not supported yet.",
                descriptor_binding.parameter_name));
        }
        // Allocate one host-visible buffer per reflected uniform descriptor and write it into the descriptor set.
        auto uniform_data = build_uniform_data(descriptor_binding, p_bindpoint_declarations);
        auto uniform_buffer = ROBufferVK::Builder()
                                  .set_domain(DeviceDomain::HOST_DEVICE)
                                  .set_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                  .set_size(uniform_data.size())
                                  .create_ref(context.render_data);
        uniform_buffer->update_buffer(uniform_data.data(), uniform_data.size());
        p_descriptor_set->write_descriptor_set(RODescriptorSetVK::Writer()
                .set_binding(descriptor_binding.layout_binding.binding)
                .add_uniform_buffer_info(RODescriptorSetVK::Writer::BufferInfo{
                    .buffer = uniform_buffer,
                    .offset = 0,
                    .range = uniform_data.size(),
                }));
    }
}

} // namespace WhiteBirdEngine
