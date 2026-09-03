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
#include "function/render_vk/pipeline_compiler_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/function_utils.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

struct PipelineIndexedFieldPath {
    std::string field_path;
    std::optional<uint32_t> array_index;
};

static PipelineIndexedFieldPath parse_pipeline_bindpoint_field_path(std::string_view p_field_path) {
    size_t open_index = p_field_path.find('[');
    size_t stray_close_index = p_field_path.find(']');
    if (open_index == std::string_view::npos) {
        if (stray_close_index != std::string_view::npos) {
            throw std::runtime_error(std::format("Failed to parse bindpoint '{}': closing bracket without opening bracket.", p_field_path));
        }
        return PipelineIndexedFieldPath{.field_path = std::string(p_field_path)};
    }
    size_t close_index = p_field_path.find(']', open_index + 1);
    if (close_index == std::string_view::npos) {
        throw std::runtime_error(std::format("Failed to parse bindpoint '{}': missing closing bracket.", p_field_path));
    }
    if (close_index + 1 < p_field_path.size() && p_field_path[close_index + 1] != '.') {
        throw std::runtime_error(
            std::format("Failed to parse bindpoint '{}': array index must be followed by a field separator.", p_field_path));
    }
    uint32_t array_index = 0;
    for (char character : p_field_path.substr(open_index + 1, close_index - open_index - 1)) {
        if (character < '0' || character > '9') {
            throw std::runtime_error(std::format("Failed to parse bindpoint '{}': array index is invalid.", p_field_path));
        }
        array_index = array_index * 10 + static_cast<uint32_t>(character - '0');
    }
    std::string result;
    result.reserve(p_field_path.size());
    result.append(p_field_path.substr(0, open_index));
    result.append(p_field_path.substr(close_index + 1));
    return PipelineIndexedFieldPath{.field_path = std::move(result), .array_index = array_index};
}

static const RenderBindpointBufferDeclaration* find_storage_buffer_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path, uint32_t p_array_index, bool p_has_array_index) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        if (bindpoint_declaration.buffer_render_object == MEM_NULL) {
            continue;
        }
        PipelineIndexedFieldPath indexed_path = parse_pipeline_bindpoint_field_path(bindpoint_declaration.field_path);
        if (indexed_path.field_path != p_field_path || indexed_path.array_index.has_value() != p_has_array_index) {
            continue;
        }
        if (indexed_path.array_index.has_value() && *indexed_path.array_index != p_array_index) {
            continue;
        }
        return &bindpoint_declaration;
    }
    if (p_has_array_index && p_array_index > 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
            PipelineIndexedFieldPath indexed_path = parse_pipeline_bindpoint_field_path(bindpoint_declaration.field_path);
            if (bindpoint_declaration.buffer_render_object != MEM_NULL && indexed_path.field_path == p_field_path &&
                indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    return nullptr;
}

static const RenderBindpointImageDeclaration* find_image_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path, uint32_t p_array_index, bool p_has_array_index) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
        PipelineIndexedFieldPath indexed_path = parse_pipeline_bindpoint_field_path(bindpoint_declaration.field_path);
        if (indexed_path.field_path != p_field_path || indexed_path.array_index.has_value() != p_has_array_index) {
            continue;
        }
        if (indexed_path.array_index.has_value() && *indexed_path.array_index != p_array_index) {
            continue;
        }
        return &bindpoint_declaration;
    }
    if (p_has_array_index && p_array_index > 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
            PipelineIndexedFieldPath indexed_path = parse_pipeline_bindpoint_field_path(bindpoint_declaration.field_path);
            if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    if (!p_has_array_index && p_array_index == 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
            PipelineIndexedFieldPath indexed_path = parse_pipeline_bindpoint_field_path(bindpoint_declaration.field_path);
            if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    return nullptr;
}

static const RenderBindpointSamplerDeclaration* find_sampler_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path, uint32_t p_array_index, bool p_has_array_index) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.sampler_declerations) {
        PipelineIndexedFieldPath indexed_path = parse_pipeline_bindpoint_field_path(bindpoint_declaration.field_path);
        if (indexed_path.field_path != p_field_path || indexed_path.array_index.has_value() != p_has_array_index) {
            continue;
        }
        if (indexed_path.array_index.has_value() && *indexed_path.array_index != p_array_index) {
            continue;
        }
        return &bindpoint_declaration;
    }
    if (p_has_array_index && p_array_index > 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.sampler_declerations) {
            PipelineIndexedFieldPath indexed_path = parse_pipeline_bindpoint_field_path(bindpoint_declaration.field_path);
            if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    if (!p_has_array_index && p_array_index == 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.sampler_declerations) {
            PipelineIndexedFieldPath indexed_path = parse_pipeline_bindpoint_field_path(bindpoint_declaration.field_path);
            if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    return nullptr;
}

PipelineCompilerVK::PipelineCompilerVK(const PipelineCompilerVKContext& p_context, std::string_view p_compiler_name)
    : base_context(p_context) {
    if (base_context.allocator == nullptr || base_context.render_data == nullptr || base_context.render_database == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to create {} compiler: invalid context.", p_compiler_name));
    }
}

Ref<ROShaderVK> PipelineCompilerVK::compile_shader(RefRes<RUShader> p_shader) {
    if (p_shader == nullptr || p_shader->resource_key == nullptr) {
        throw std::runtime_error("Failed to compile shader: shader resource key was not propagated.");
    }
    std::string shader_key = p_shader->resource_key;
    auto registered_shader =
        get_registered_render_object<ROShaderVK>(base_context.render_database, shader_key, TypeUUID<ROShaderVK>::UUID);
    if (registered_shader != MEM_NULL) {
        return registered_shader;
    }
    auto shader = ROShaderVK::Builder().set_code(p_shader).create_ref(base_context.render_data);
    base_context.render_database->register_compiled_render_resource(shader_key, shader);
    return shader;
}

std::vector<Ref<RODescriptorSetLayoutVK>> PipelineCompilerVK::compile_descriptor_set_layouts(
    std::string_view p_resource_key, const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings) {
    std::vector<Ref<RODescriptorSetLayoutVK>> result;
    auto descriptor_set_layout_bindings = build_shader_descriptor_set_layout_bindings(p_descriptor_bindings);
    set_variable_descriptor_count_upper_bounds(
        descriptor_set_layout_bindings, base_context.render_data->max_variable_sampled_image_descriptor_count);
    result.reserve(descriptor_set_layout_bindings.size());
    for (const auto& descriptor_set_layout_binding : descriptor_set_layout_bindings) {
        std::string descriptor_set_layout_key =
            std::format("{}#descriptor_set_layout_{}", p_resource_key, descriptor_set_layout_binding.set_index);
        auto descriptor_set_layout = get_registered_render_object<RODescriptorSetLayoutVK>(
            base_context.render_database, descriptor_set_layout_key, TypeUUID<RODescriptorSetLayoutVK>::UUID);
        if (descriptor_set_layout == MEM_NULL) {
            RODescriptorSetLayoutVK::Builder builder;
            for (size_t binding_index = 0; binding_index < descriptor_set_layout_binding.bindings.size(); ++binding_index) {
                builder.add_binding(descriptor_set_layout_binding.bindings[binding_index],
                    descriptor_set_layout_binding.binding_flags[binding_index]);
            }
            descriptor_set_layout = builder.create_ref(base_context.render_data);
            base_context.render_database->register_compiled_render_resource(descriptor_set_layout_key, descriptor_set_layout);
        }
        result.push_back(descriptor_set_layout);
    }
    return result;
}

Ref<ROPipelineLayoutVK> PipelineCompilerVK::compile_pipeline_layout(
    std::string_view p_resource_key, const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts) {
    std::string pipeline_layout_key = std::format("{}#pipeline_layout", p_resource_key);
    auto pipeline_layout = get_registered_render_object<ROPipelineLayoutVK>(
        base_context.render_database, pipeline_layout_key, TypeUUID<ROPipelineLayoutVK>::UUID);
    if (pipeline_layout != MEM_NULL) {
        return pipeline_layout;
    }
    ROPipelineLayoutVK::Builder builder;
    for (const auto& descriptor_set_layout : p_descriptor_set_layouts) {
        builder.add_descriptor_set_layout(descriptor_set_layout);
    }
    pipeline_layout = builder.create_ref(base_context.render_data);
    base_context.render_database->register_compiled_render_resource(pipeline_layout_key, pipeline_layout);
    return pipeline_layout;
}

PipelineCompilerVK::DescriptorSetBindings PipelineCompilerVK::build_descriptor_set_bindings(
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings) {
    DescriptorSetBindings result;
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        result.emplace(descriptor_binding.parameter_name,
            DescriptorSetBinding{descriptor_binding.set_index, descriptor_binding.layout_binding.binding});
    }
    return result;
}

void PipelineCompilerVK::write_storage_buffer_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    uint32_t p_set_index,
    const RenderBindpointDeclarations& p_bindpoint_declarations) const {
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        if (descriptor_binding.set_index != p_set_index ||
            descriptor_binding.layout_binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            continue;
        }
        const uint32_t descriptor_count = std::max(descriptor_binding.layout_binding.descriptorCount, 1U);
        const bool has_array_index = descriptor_binding.layout_binding.descriptorCount > 1;
        for (uint32_t array_index = 0; array_index < descriptor_count; ++array_index) {
            const auto* bindpoint_declaration = find_storage_buffer_binding(
                p_bindpoint_declarations, descriptor_binding.parameter_name, array_index, has_array_index);
            if (bindpoint_declaration == nullptr) {
                throw std::runtime_error(std::format("Failed to bind pipeline storage buffer '{}'.", descriptor_binding.parameter_name));
            }
            auto buffer = bindpoint_declaration->buffer_render_object.dynamic_cast_ref<ROBufferVK>();
            if (buffer == MEM_NULL) {
                throw std::runtime_error(std::format("Failed to bind pipeline storage buffer '{}': buffer render object is missing.",
                    descriptor_binding.parameter_name));
            }
            const VkDeviceSize range = bindpoint_declaration->size == std::numeric_limits<size_t>::max()
                                           ? VK_WHOLE_SIZE
                                           : static_cast<VkDeviceSize>(bindpoint_declaration->size);
            p_descriptor_set->write_descriptor_set(RODescriptorSetVK::Writer()
                    .set_binding(descriptor_binding.layout_binding.binding)
                    .set_dst_array_element(array_index)
                    .add_storage_buffer_info(RODescriptorSetVK::Writer::BufferInfo{
                        .buffer = buffer,
                        .offset = static_cast<VkDeviceSize>(bindpoint_declaration->offset),
                        .range = range,
                    }));
        }
    }
}

void PipelineCompilerVK::write_image_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    uint32_t p_set_index,
    const RenderBindpointDeclarations& p_bindpoint_declarations) const {
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        if (descriptor_binding.set_index != p_set_index) {
            continue;
        }
        VkDescriptorType descriptor_type = descriptor_binding.layout_binding.descriptorType;
        if (descriptor_type != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && descriptor_type != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE &&
            descriptor_type != VK_DESCRIPTOR_TYPE_SAMPLER) {
            continue;
        }
        RODescriptorSetVK::Writer writer;
        writer.set_binding(descriptor_binding.layout_binding.binding);
        uint32_t descriptor_count = std::max(descriptor_binding.layout_binding.descriptorCount, 1U);
        for (uint32_t array_index = 0; array_index < descriptor_count; ++array_index) {
            bool has_array_index = descriptor_binding.layout_binding.descriptorCount > 1;
            if (descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER) {
                const auto* sampler_binding =
                    find_sampler_binding(p_bindpoint_declarations, descriptor_binding.parameter_name, array_index, has_array_index);
                if (sampler_binding == nullptr) {
                    throw std::runtime_error(std::format("Failed to bind pipeline sampler '{}'.", descriptor_binding.parameter_name));
                }
                auto sampler = sampler_binding->sampler_render_object.dynamic_cast_ref<ROSamplerVK>();
                if (sampler == MEM_NULL) {
                    throw std::runtime_error(std::format("Failed to bind pipeline sampler '{}': sampler object is missing.",
                        descriptor_binding.parameter_name));
                }
                writer.add_sampler_info({.sampler = sampler, .image_layout = VK_IMAGE_LAYOUT_GENERAL});
                continue;
            }
            const auto* image_binding =
                find_image_binding(p_bindpoint_declarations, descriptor_binding.parameter_name, array_index, has_array_index);
            if (image_binding == nullptr) {
                throw std::runtime_error(std::format("Failed to bind pipeline image '{}'.", descriptor_binding.parameter_name));
            }
            auto image_view = image_binding->image_render_object.dynamic_cast_ref<ROImageViewVK>();
            if (image_view == MEM_NULL) {
                throw std::runtime_error(std::format("Failed to bind pipeline image '{}': image view render object is missing.",
                    descriptor_binding.parameter_name));
            }
            if (descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
                writer.add_storage_image_info({.image_view = image_view, .image_layout = VK_IMAGE_LAYOUT_GENERAL});
            } else {
                writer.add_sampled_image_info({.image_view = image_view, .image_layout = VK_IMAGE_LAYOUT_GENERAL});
            }
        }
        p_descriptor_set->write_descriptor_set(writer);
    }
}

void PipelineCompilerVK::write_uniform_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    uint32_t p_set_index,
    const RenderBindpointDeclarations& p_bindpoint_declarations,
    UniformDataBuilder p_uniform_data_builder) const {
    if (p_uniform_data_builder == nullptr) {
        throw std::runtime_error("Failed to write pipeline uniform descriptors: uniform data builder is missing.");
    }
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        if (descriptor_binding.set_index != p_set_index ||
            descriptor_binding.layout_binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            continue;
        }
        std::vector<uint8_t> uniform_data = p_uniform_data_builder(descriptor_binding, p_bindpoint_declarations);
        auto uniform_buffer = ROBufferVK::Builder()
                                  .set_domain(DeviceDomain::HOST_DEVICE)
                                  .set_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                  .set_size(uniform_data.size())
                                  .create_ref(base_context.render_data);
        uniform_buffer->update_buffer(uniform_data.data(), uniform_data.size());
        RODescriptorSetVK::Writer writer;
        writer.set_binding(descriptor_binding.layout_binding.binding);
        uint32_t descriptor_count = std::max(descriptor_binding.layout_binding.descriptorCount, 1U);
        for (uint32_t array_index = 0; array_index < descriptor_count; ++array_index) {
            writer.add_uniform_buffer_info(RODescriptorSetVK::Writer::BufferInfo{
                .buffer = uniform_buffer,
                .offset = descriptor_binding.buffer_size * array_index,
                .range = descriptor_binding.buffer_size,
            });
        }
        p_descriptor_set->write_descriptor_set(writer);
    }
}

} // namespace WhiteBirdEngine
