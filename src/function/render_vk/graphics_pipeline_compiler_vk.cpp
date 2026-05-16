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
#include "core/memory/unique.hh"
#include "core/parser/parser_json.hh"
#include "function/function_utils.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
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
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/shader_metadata_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <glm/ext/vector_float4.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

uint32_t image_upload_channel_count(const RUImage& p_image) {
    if (p_image.channel_count == 3 || p_image.channel_count == 4) {
        return 4;
    }
    throw std::runtime_error(std::format("Failed to compile material texture: unsupported image channel count {}.", p_image.channel_count));
}

VkFormat image_format_from_resource(const RUImage& p_image) {
    image_upload_channel_count(p_image);
    if (p_image.config.test(RUImage::IS_HDR)) {
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    return p_image.color_space == ColorSpace::SRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
}

std::vector<uint8_t> build_image_upload_data(const RUImage& p_image) {
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
        std::memcpy(upload_data.data(), source_data, upload_data.size());
        return upload_data;
    }

    if (p_image.config.test(RUImage::IS_HDR)) {
        const float alpha = 1.0F;
        const size_t source_stride = static_cast<size_t>(source_channels) * sizeof(float);
        const size_t target_stride = static_cast<size_t>(target_channels) * sizeof(float);
        for (size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
            const size_t source_offset = pixel_index * source_stride;
            const size_t target_offset = pixel_index * target_stride;
            std::memcpy(upload_data.data() + target_offset, source_data + source_offset, source_stride);
            std::memcpy(upload_data.data() + target_offset + source_stride, &alpha, sizeof(alpha));
        }
        return upload_data;
    }

    for (size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
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
    if (p_text == nullptr || p_text->text_data == nullptr) {
        throw std::runtime_error("Failed to compile material texture: texture key is missing.");
    }
    return std::string(p_text->text_data.get());
}

ShaderMetadata load_shader_metadata(RefRes<RUShader> p_shader) {
    if (p_shader == nullptr || p_shader->metadata_path == nullptr) {
        throw std::runtime_error("Failed to load shader metadata: metadata path is missing.");
    }
    ParserJSON metadata_parser;
    metadata_parser.parse(get_resource_path(p_shader->metadata_path.get()));
    ShaderMetadata metadata;
    metadata.deserialize(metadata_parser.get_data());
    return metadata;
}

void validate_uniform_bindings(std::span<const RenderUniformBinding> p_uniform_bindings) {
    if (p_uniform_bindings.empty()) {
        return;
    }
    for (const auto& uniform_binding : p_uniform_bindings) {
        if (uniform_binding.field_path.empty()) {
            throw std::runtime_error("Failed to compile graphics pipeline: uniform binding field path is empty.");
        }
        if (uniform_binding.data == nullptr || uniform_binding.size == 0) {
            throw std::runtime_error(
                std::format("Failed to compile graphics pipeline: uniform binding '{}' has no data.", uniform_binding.field_path));
        }
    }
}

const RenderUniformBinding* find_uniform_binding(std::span<const RenderUniformBinding> p_uniform_bindings, const std::string& p_field_path) {
    for (const auto& uniform_binding : p_uniform_bindings) {
        if (uniform_binding.field_path == p_field_path) {
            return &uniform_binding;
        }
    }
    return nullptr;
}

class RTRunCompiledGraphicsPipelineVK final : public RenderTask {
public:
    RTRunCompiledGraphicsPipelineVK(const BaseRenderDataVK* p_render_data, RendererVK* p_renderer, RendPipelineDescVK&& p_pipeline_desc)
        : renderer(p_renderer), graph(make_ref<RenderGraphVK>(p_render_data->allocator, p_render_data)) {
        if (renderer == nullptr) {
            throw std::runtime_error("Failed to create compiled graphics pipeline task: renderer is null.");
        }
        graph->build_from_descriptor(p_pipeline_desc);
    }

    virtual void init() override {
    }

    virtual void perform(double p_delta) override {
        renderer->execute_graph(graph, p_delta);
    }

private:
    RendererVK* renderer = nullptr;
    Ref<RenderGraphVK> graph;
};

class GraphicsPipelineCompilerVKImpl {
public:
    GraphicsPipelineCompilerVKImpl(const GraphicsPipelineCompilerVKContext& p_context) : context(p_context) {
        if (context.allocator == nullptr || context.render_data == nullptr || context.render_database == MEM_NULL ||
            context.render_pass == MEM_NULL || context.renderer == nullptr) {
            throw std::runtime_error("Failed to create graphics pipeline compiler: invalid context.");
        }
    }

    Unique<RenderTask> compile(const std::string& p_resource_key,
        RefRes<RUGraphicsPipeline> p_pipeline_resource,
        std::span<const RenderUniformBinding> p_uniform_bindings) {
        validate_uniform_bindings(p_uniform_bindings);
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
        auto graphics_pipeline =
            compile_graphics_pipeline(p_resource_key, *p_pipeline_resource, vertex_shader, fragment_shader, pipeline_layout);
        auto descriptor_sets = compile_descriptor_sets(
            p_resource_key, *p_pipeline_resource, descriptor_set_layouts, descriptor_bindings, p_uniform_bindings);
        auto pipeline_desc = build_pipeline_desc(p_resource_key, graphics_pipeline, descriptor_sets);
        return make_unique<RTRunCompiledGraphicsPipelineVK>(
            context.allocator, context.render_data, context.renderer, std::move(pipeline_desc));
    }

private:
    GraphicsPipelineCompilerVKContext context;

    // Shader resources are cached by their own manifest keys because multiple
    // graphics pipelines can point at the same shader resource unit.
    Ref<ROShaderVK> compile_shader(RefRes<RUShader> p_shader) {
        if (p_shader == nullptr || p_shader->resource_key == nullptr) {
            throw std::runtime_error("Failed to compile shader: shader resource key was not propagated.");
        }
        std::string shader_key = p_shader->resource_key;
        auto registered_shader = get_registered_render_object<ROShaderVK>(shader_key, TypeUUID<ROShaderVK>::UUID);
        if (registered_shader != MEM_NULL) {
            return registered_shader;
        }
        auto shader = ROShaderVK::Builder().set_code(p_shader).create_ref(context.render_data);
        context.render_database->register_compiled_render_resource(
            shader_key, TypeUUID<ROShaderVK>::UUID, RefWeak<IRenderObject>(shader));
        return shader;
    }

    std::vector<Ref<RODescriptorSetLayoutVK>> compile_descriptor_set_layouts(
        const std::string& p_resource_key, const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings) {
        std::vector<Ref<RODescriptorSetLayoutVK>> result;
        auto descriptor_set_layout_bindings = build_graphics_pipeline_descriptor_set_layout_bindings(p_descriptor_bindings);
        result.reserve(descriptor_set_layout_bindings.size());
        for (const auto& descriptor_set_layout_binding : descriptor_set_layout_bindings) {
            // Descriptor set layouts are compiler-generated subresources of the
            // graphics pipeline, so they use derived resource keys.
            std::string descriptor_set_layout_key =
                std::format("{}#descriptor_set_layout_{}", p_resource_key, descriptor_set_layout_binding.set_index);
            auto descriptor_set_layout = get_registered_render_object<RODescriptorSetLayoutVK>(
                descriptor_set_layout_key, TypeUUID<RODescriptorSetLayoutVK>::UUID);
            if (descriptor_set_layout == MEM_NULL) {
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

    Ref<ROPipelineLayoutVK> compile_pipeline_layout(
        const std::string& p_resource_key, const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts) {
        std::string pipeline_layout_key = std::format("{}#pipeline_layout", p_resource_key);
        auto pipeline_layout =
            get_registered_render_object<ROPipelineLayoutVK>(pipeline_layout_key, TypeUUID<ROPipelineLayoutVK>::UUID);
        if (pipeline_layout != MEM_NULL) {
            return pipeline_layout;
        }
        ROPipelineLayoutVK::Builder builder;
        for (const auto& descriptor_set_layout : p_descriptor_set_layouts) {
            builder.add_descriptor_set_layout(descriptor_set_layout);
        }
        pipeline_layout = builder.create_ref(context.render_data);
        context.render_database->register_compiled_render_resource(
            pipeline_layout_key, TypeUUID<ROPipelineLayoutVK>::UUID, RefWeak<IRenderObject>(pipeline_layout));
        return pipeline_layout;
    }

    Ref<ROGraphicsPipelineVK> compile_graphics_pipeline(const std::string& p_resource_key,
        const RUGraphicsPipeline& p_pipeline_resource,
        Ref<ROShaderVK> p_vertex_shader,
        Ref<ROShaderVK> p_fragment_shader,
        Ref<ROPipelineLayoutVK> p_pipeline_layout) {
        auto graphics_pipeline =
            get_registered_render_object<ROGraphicsPipelineVK>(p_resource_key, TypeUUID<ROGraphicsPipelineVK>::UUID);
        if (graphics_pipeline != MEM_NULL) {
            return graphics_pipeline;
        }
        ROGraphicsPipelineVK::Config::ShaderConfig shader_config;
        shader_config.set_shaders(p_vertex_shader,
            p_fragment_shader,
            get_stage_entry(p_pipeline_resource.vertex_stage_entry),
            get_stage_entry(p_pipeline_resource.fragment_stage_entry));
        auto pipeline_builder = ROGraphicsPipelineVK::Builder().default_render_opaque_configuration(
            context.render_data, shader_config, p_pipeline_layout, context.render_pass);
        pipeline_builder.config.depth_test.set_depth_test(p_pipeline_resource.depth_test ? VK_TRUE : VK_FALSE);
        pipeline_builder.config.depth_test.set_depth_write(p_pipeline_resource.depth_write ? VK_TRUE : VK_FALSE);
        graphics_pipeline = pipeline_builder.create_ref(context.render_data);
        context.render_database->register_compiled_render_resource(
            p_resource_key, TypeUUID<ROGraphicsPipelineVK>::UUID, RefWeak<IRenderObject>(graphics_pipeline));
        return graphics_pipeline;
    }

    std::vector<Ref<RODescriptorSetVK>> compile_descriptor_sets(const std::string& p_resource_key,
        const RUGraphicsPipeline& p_pipeline_resource,
        const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        std::span<const RenderUniformBinding> p_uniform_bindings) {
        if (p_descriptor_set_layouts.empty()) {
            return {};
        }
        RefRes<RUMaterial> material = find_material_for_pipeline(p_pipeline_resource);
        if (material == nullptr && has_material_descriptor_bindings(p_descriptor_bindings)) {
            throw std::runtime_error(
                std::format("Failed to compile graphics pipeline '{}': no static opaque material uses it.", p_resource_key));
        }

        std::vector<Ref<RODescriptorSetVK>> descriptor_sets;
        descriptor_sets.reserve(p_descriptor_set_layouts.size());
        for (uint32_t set_index = 0; set_index < p_descriptor_set_layouts.size(); ++set_index) {
            Ref<RODescriptorSetVK> descriptor_set =
                material == nullptr
                    ? RODescriptorSetVK::Builder()
                          .add_descriptor_set_layout(p_descriptor_set_layouts[set_index])
                          .create_ref(context.render_data)
                    : compile_material_descriptor_set(
                          p_resource_key, material, p_descriptor_set_layouts[set_index], p_descriptor_bindings, set_index);
            write_uniform_descriptors(descriptor_set, p_descriptor_bindings, set_index, p_uniform_bindings);
            descriptor_sets.push_back(descriptor_set);
        }
        return descriptor_sets;
    }

    Ref<RODescriptorSetVK> compile_material_descriptor_set(const std::string& p_resource_key,
        RefRes<RUMaterial> p_material,
        Ref<RODescriptorSetLayoutVK> p_descriptor_set_layout,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        uint32_t p_set_index) {
        if (p_material->resource_key == nullptr) {
            throw std::runtime_error(std::format(
                "Failed to compile graphics pipeline '{}': material resource key was not propagated.", p_resource_key));
        }
        auto default_sampler = ROSamplerVK::Builder().create_ref(context.render_data);
        ROMaterialVK::Builder builder;
        builder.set_descriptor_set_layout(p_descriptor_set_layout)
            .set_set_index(p_set_index)
            .set_descriptor_bindings(p_descriptor_bindings)
            .set_default_sampler(default_sampler);
        std::span<RUMaterial::TextureSlot> texture_slots(p_material->textures.get(), p_material->texture_count);
        for (const auto& texture_slot : texture_slots) {
            std::string texture_key = text_resource_value(texture_slot.texture_key);
            if (texture_slot.texture == nullptr) {
                throw std::runtime_error(std::format(
                    "Failed to compile material '{}': texture '{}' is missing.", p_material->resource_key, texture_key));
            }
            builder.add_texture_binding(ROMaterialVK::TextureBinding{
                .texture_key = texture_key,
                .image_view =
                    compile_image_view(std::format("{}#texture_{}", p_material->resource_key, texture_key), texture_slot.texture),
                .image_layout = VK_IMAGE_LAYOUT_GENERAL,
            });
        }
        return builder.create_ref(context.render_data)->get_descriptor_set();
    }

    Ref<ROImageViewVK> compile_image_view(const std::string& p_resource_key, RefRes<RUImage> p_image_resource) {
        auto image_view = get_registered_render_object<ROImageViewVK>(p_resource_key + "#image_view", TypeUUID<ROImageViewVK>::UUID);
        if (image_view != MEM_NULL) {
            return image_view;
        }
        VkFormat image_format = image_format_from_resource(*p_image_resource);
        auto image = compile_image(p_resource_key + "#image", p_image_resource, image_format);
        image_view =
            ROImageViewVK::Builder()
                .set_image(image)
                .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                .set_format(image_format)
                .set_subresource_range(VkImageSubresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1})
                .create_ref(context.render_data);
        context.render_database->register_compiled_render_resource(
            p_resource_key + "#image_view", TypeUUID<ROImageViewVK>::UUID, RefWeak<IRenderObject>(image_view));
        return image_view;
    }

    Ref<ROImageVK> compile_image(const std::string& p_resource_key, RefRes<RUImage> p_image_resource, VkFormat p_image_format) {
        auto image = get_registered_render_object<ROImageVK>(p_resource_key, TypeUUID<ROImageVK>::UUID);
        if (image != MEM_NULL) {
            return image;
        }
        image = ROImageVK::Builder()
                    .set_image_type(VK_IMAGE_TYPE_2D)
                    .set_format(p_image_format)
                    .set_extent(VkExtent3D{.width = p_image_resource->width, .height = p_image_resource->height, .depth = 1})
                    .set_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                    .create_ref(context.render_data);
        context.render_database->register_compiled_render_resource(
            p_resource_key, TypeUUID<ROImageVK>::UUID, RefWeak<IRenderObject>(image));

        auto upload_data = build_image_upload_data(*p_image_resource);
        auto upload_buffer = ROBufferVK::Builder()
                                 .set_domain(DeviceDomain::HOST)
                                 .set_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                                 .set_size(upload_data.size())
                                 .create_ref(context.render_data);
        upload_buffer->update_buffer(upload_data.data(), upload_data.size());
        context.renderer->add_pre_execute_task(
            [image, upload_buffer, width = p_image_resource->width, height = p_image_resource->height](VkCommandBuffer p_command_buffer) mutable {
                VkBufferImageCopy region_info{};
                region_info.imageSubresource = VkImageSubresourceLayers{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1};
                region_info.imageExtent = VkExtent3D{.width = width, .height = height, .depth = 1};
                image->set_image_data_immediate(p_command_buffer, upload_buffer, region_info);
            });
        return image;
    }

    void write_uniform_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        uint32_t p_set_index,
        std::span<const RenderUniformBinding> p_uniform_bindings) const {
        for (const auto& descriptor_binding : p_descriptor_bindings) {
            if (descriptor_binding.set_index != p_set_index ||
                descriptor_binding.layout_binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                continue;
            }
            if (descriptor_binding.layout_binding.descriptorCount != 1) {
                throw std::runtime_error(std::format("Failed to compile uniform descriptor '{}': descriptor arrays are not supported yet.",
                    descriptor_binding.parameter_name));
            }
            auto uniform_data = build_uniform_data(descriptor_binding, p_uniform_bindings);
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

    static std::vector<uint8_t> build_uniform_data(
        const ShaderDescriptorBindingVK& p_descriptor_binding, std::span<const RenderUniformBinding> p_uniform_bindings) {
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
            const auto* uniform_binding = find_uniform_binding(p_uniform_bindings, field.field_path);
            if (uniform_binding == nullptr) {
                throw std::runtime_error(
                    std::format("Failed to bind uniform field '{}': caller did not provide a binding.", field.field_path));
            }
            if (uniform_binding->size != field.size) {
                throw std::runtime_error(std::format("Failed to bind uniform field '{}': program field size {} does not match shader field size {}.",
                    field.field_path,
                    uniform_binding->size,
                    field.size));
            }
            if (field.offset + field.size > uniform_data.size()) {
                throw std::runtime_error(std::format(
                    "Failed to bind uniform field '{}': reflected field range exceeds uniform buffer size.", field.field_path));
            }
            std::memcpy(uniform_data.data() + field.offset, uniform_binding->data, field.size);
        }
        return uniform_data;
    }

    RefRes<RUMaterial> find_material_for_pipeline(const RUGraphicsPipeline& p_pipeline_resource) const {
        const auto& triangles_info = context.render_database->get_static_opaque_triangles_info();
        for (const auto& material_unit : triangles_info.materials) {
            if (material_unit.material != nullptr && material_unit.material->graphics_pipeline.get() == &p_pipeline_resource) {
                return material_unit.material;
            }
        }
        return MEM_NULL;
    }

    static bool has_material_descriptor_bindings(const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings) {
        for (const auto& descriptor_binding : p_descriptor_bindings) {
            VkDescriptorType descriptor_type = descriptor_binding.layout_binding.descriptorType;
            if (descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER ||
                descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                return true;
            }
        }
        return false;
    }

    RendPipelineDescVK build_pipeline_desc(const std::string& p_resource_key,
        Ref<ROGraphicsPipelineVK> p_graphics_pipeline,
        const std::vector<Ref<RODescriptorSetVK>>& p_descriptor_sets) const {
        RendResID pipeline_res_id = dynam_hash(p_resource_key);
        if (pipeline_res_id == REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK || pipeline_res_id == REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK ||
            pipeline_res_id == REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK ||
            pipeline_res_id == REND_RES_STATIC_OPAQUE_TRIANGLES_OBJ_IDS_VK) {
            throw std::runtime_error(
                std::format("Failed to compile graphics pipeline '{}': generated render resource id collides.", p_resource_key));
        }
        RendPipelineDescVK pipeline_desc(context.allocator);
        pipeline_desc.resources.emplace_back(RendResVK{
            .res_id = pipeline_res_id,
            .res_type_id = TypeUUID<ROGraphicsPipelineVK>::UUID,
            .res = p_graphics_pipeline,
        });
        RendTaskDescVK task_desc(context.allocator);
        task_desc.task_id = dynam_hash(std::format("{}#run_graphics_pipeline", p_resource_key));
        task_desc.create = [render_allocator = context.allocator,
                               render_data = context.render_data,
                               pipeline_res_id,
                               descriptor_sets = p_descriptor_sets]() -> Unique<RenderTaskVK> {
            return make_unique<RTRunGraphicsPipelineVK>(render_allocator,
                render_data,
                glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
                REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK,
                pipeline_res_id,
                REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
                REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
                std::span<const Ref<RODescriptorSetVK>>(descriptor_sets.data(), descriptor_sets.size()));
        };
        pipeline_desc.tasks.emplace_back(std::move(task_desc));
        return pipeline_desc;
    }

    template <typename RenderObjectType>
    Ref<RenderObjectType> get_registered_render_object(const std::string& p_resource_key, HashCode p_render_object_type_id) {
        auto render_object = context.render_database->get_compiled_render_resource(p_resource_key, p_render_object_type_id);
        if (render_object == MEM_NULL) {
            return MEM_NULL;
        }
        return render_object.dynamic_cast_ref<RenderObjectType>();
    }

    static std::string get_stage_entry(RefRes<char> p_stage_entry) {
        return p_stage_entry == nullptr ? std::string("main") : std::string(p_stage_entry.get());
    }
};

} // namespace

Unique<RenderTask> compile_graphics_pipeline_task_vk(const GraphicsPipelineCompilerVKContext& p_context,
    const std::string& p_resource_key,
    RefRes<RUGraphicsPipeline> p_pipeline_resource,
    std::span<const RenderUniformBinding> p_uniform_bindings) {
    GraphicsPipelineCompilerVKImpl compiler(p_context);
    return compiler.compile(p_resource_key, p_pipeline_resource, p_uniform_bindings);
}

} // namespace WhiteBirdEngine
