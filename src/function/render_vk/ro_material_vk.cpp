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
#include "function/render_vk/ro_material_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TypeID ROMaterialVK::get_type_uuid() const {
    return TypeUUID<ROMaterialVK>::UUID;
}
namespace {

ROMaterialVK::TextureBinding find_texture_binding(const std::vector<ROMaterialVK::TextureBinding>& p_texture_bindings,
    std::string_view p_field_path,
    const ROMaterialVK::TextureBinding& p_default_texture) {
    auto binding = std::ranges::find_if(p_texture_bindings, [&p_field_path](const ROMaterialVK::TextureBinding& p_binding) {
        return p_binding.field_path == p_field_path;
    });
    if (binding == p_texture_bindings.end()) {
        std::size_t field_separator = p_field_path.rfind('.');
        if (field_separator != std::string_view::npos) {
            std::string_view field_name = p_field_path.substr(field_separator + 1);
            binding = std::ranges::find_if(p_texture_bindings, [&field_name](const ROMaterialVK::TextureBinding& p_binding) {
                return p_binding.field_path == field_name;
            });
        }
    }
    if (binding == p_texture_bindings.end()) {
        if (p_default_texture.image_view != MEM_NULL) {
            return p_default_texture;
        }
        throw std::runtime_error(std::format("Failed to bind material texture '{}': image view is missing.", p_field_path));
    }
    if (binding->image_view == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to bind material texture '{}': image view is missing.", p_field_path));
    }
    return *binding;
}

const ROMaterialVK::TextureBinding* find_indexed_texture_binding(
    const std::vector<ROMaterialVK::TextureBinding>& p_texture_bindings, std::string_view p_field_path, uint32_t p_array_index) {
    auto unindexed_binding = std::ranges::find_if(p_texture_bindings, [&p_field_path](const ROMaterialVK::TextureBinding& p_binding) {
        return p_binding.field_path == p_field_path && !p_binding.has_array_index;
    });
    if (unindexed_binding != p_texture_bindings.end()) {
        throw std::runtime_error(
            std::format("Failed to bind material texture array '{}': array index must be specified with [].", p_field_path));
    }
    auto binding = std::ranges::find_if(p_texture_bindings, [&p_field_path, p_array_index](const ROMaterialVK::TextureBinding& p_binding) {
        return p_binding.field_path == p_field_path && p_binding.has_array_index && p_binding.array_index == p_array_index;
    });
    if (binding == p_texture_bindings.end()) {
        return nullptr;
    }
    if (binding->image_view == MEM_NULL) {
        throw std::runtime_error(
            std::format("Failed to bind material texture '{}[{}]': image view is missing.", p_field_path, p_array_index));
    }
    return &*binding;
}

const ROMaterialVK::TextureBinding* find_indexed_texture_binding_fallback(
    const std::vector<ROMaterialVK::TextureBinding>& p_texture_bindings, std::string_view p_field_path) {
    // Use element 0 as the default descriptor for array slots that are inactive but still must be valid for Vulkan.
    auto binding = std::ranges::find_if(p_texture_bindings, [&p_field_path](const ROMaterialVK::TextureBinding& p_binding) {
        return p_binding.field_path == p_field_path && p_binding.has_array_index && p_binding.array_index == 0;
    });
    if (binding == p_texture_bindings.end()) {
        return nullptr;
    }
    if (binding->image_view == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to bind material texture '{}[0]': image view is missing.", p_field_path));
    }
    return &*binding;
}

void validate_indexed_texture_bindings(
    const std::vector<ROMaterialVK::TextureBinding>& p_texture_bindings, std::string_view p_field_path, uint32_t p_descriptor_count) {
    for (const ROMaterialVK::TextureBinding& binding : p_texture_bindings) {
        if (binding.field_path == p_field_path && binding.has_array_index && binding.array_index >= p_descriptor_count) {
            throw std::runtime_error(std::format(
                "Failed to bind material texture array '{}': index {} is out of bounds.", p_field_path, binding.array_index));
        }
    }
}

uint32_t get_variable_sampled_image_descriptor_count(const ROMaterialVK::Builder& p_builder) {
    uint32_t descriptor_count = 0;
    for (const auto& descriptor_binding : p_builder.descriptor_bindings) {
        if (descriptor_binding.set_index != p_builder.set_index || !descriptor_binding.variable_descriptor_count) {
            continue;
        }
        if (descriptor_binding.layout_binding.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
            throw std::runtime_error("Failed to bind runtime descriptor array: only sampled images are supported.");
        }
        if (descriptor_count != 0) {
            throw std::runtime_error("Failed to bind runtime descriptor array: only one variable-count binding is allowed per set.");
        }
        descriptor_count = 1;
        for (const auto& texture_binding : p_builder.texture_bindings) {
            if (texture_binding.field_path != descriptor_binding.parameter_name) {
                continue;
            }
            if (!texture_binding.has_array_index) {
                throw std::runtime_error(std::format("Failed to bind runtime texture array '{}': array index must be specified with [].",
                    descriptor_binding.parameter_name));
            }
            descriptor_count = std::max(descriptor_count, texture_binding.array_index + 1);
        }
    }
    return descriptor_count;
}

// A default sampler lets materials omit one sampler slot while still binding
// separate texture and sampler descriptors from Slang reflection metadata.
Ref<ROSamplerVK> find_sampler_binding(const std::vector<ROMaterialVK::SamplerBinding>& p_sampler_bindings,
    std::string_view p_sampler_key,
    Ref<ROSamplerVK> p_default_sampler) {
    auto binding = std::ranges::find_if(p_sampler_bindings, [&p_sampler_key](const ROMaterialVK::SamplerBinding& p_binding) {
        return p_binding.sampler_key == p_sampler_key;
    });
    if (binding != p_sampler_bindings.end()) {
        if (binding->sampler == MEM_NULL) {
            throw std::runtime_error(std::format("Failed to bind material sampler '{}': sampler is missing.", p_sampler_key));
        }
        return binding->sampler;
    }
    if (p_default_sampler == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to bind material sampler '{}': sampler is missing.", p_sampler_key));
    }
    return p_default_sampler;
}

Ref<ROSamplerVK> find_indexed_sampler_binding(const std::vector<ROMaterialVK::SamplerBinding>& p_sampler_bindings,
    std::string_view p_sampler_key,
    uint32_t p_array_index,
    Ref<ROSamplerVK> p_default_sampler) {
    auto unindexed_binding = std::ranges::find_if(p_sampler_bindings, [&p_sampler_key](const ROMaterialVK::SamplerBinding& p_binding) {
        return p_binding.sampler_key == p_sampler_key && !p_binding.has_array_index;
    });
    if (unindexed_binding != p_sampler_bindings.end()) {
        throw std::runtime_error(
            std::format("Failed to bind material sampler array '{}': array index must be specified with [].", p_sampler_key));
    }
    auto binding = std::ranges::find_if(p_sampler_bindings, [&p_sampler_key, p_array_index](const ROMaterialVK::SamplerBinding& p_binding) {
        return p_binding.sampler_key == p_sampler_key && p_binding.has_array_index && p_binding.array_index == p_array_index;
    });
    if (binding != p_sampler_bindings.end()) {
        if (binding->sampler == MEM_NULL) {
            throw std::runtime_error(
                std::format("Failed to bind material sampler '{}[{}]': sampler is missing.", p_sampler_key, p_array_index));
        }
        return binding->sampler;
    }
    return p_default_sampler;
}

Ref<ROSamplerVK> find_indexed_sampler_binding_fallback(
    const std::vector<ROMaterialVK::SamplerBinding>& p_sampler_bindings, std::string_view p_sampler_key) {
    // Use element 0 as the default descriptor for array slots that are inactive but still must be valid for Vulkan.
    auto binding = std::ranges::find_if(p_sampler_bindings, [&p_sampler_key](const ROMaterialVK::SamplerBinding& p_binding) {
        return p_binding.sampler_key == p_sampler_key && p_binding.has_array_index && p_binding.array_index == 0;
    });
    if (binding == p_sampler_bindings.end()) {
        return MEM_NULL;
    }
    if (binding->sampler == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to bind material sampler '{}[0]': sampler is missing.", p_sampler_key));
    }
    return binding->sampler;
}

void validate_indexed_sampler_bindings(
    const std::vector<ROMaterialVK::SamplerBinding>& p_sampler_bindings, std::string_view p_sampler_key, uint32_t p_descriptor_count) {
    for (const ROMaterialVK::SamplerBinding& binding : p_sampler_bindings) {
        if (binding.sampler_key == p_sampler_key && binding.has_array_index && binding.array_index >= p_descriptor_count) {
            throw std::runtime_error(std::format(
                "Failed to bind material sampler array '{}': index {} is out of bounds.", p_sampler_key, binding.array_index));
        }
    }
}

void write_sampled_image_binding(Ref<RODescriptorSetVK> p_descriptor_set,
    const ROMaterialVK::Builder& p_builder,
    const ShaderDescriptorBindingVK& p_descriptor_binding) {
    uint32_t descriptor_count = p_descriptor_binding.variable_descriptor_count
                                    ? get_variable_sampled_image_descriptor_count(p_builder)
                                    : p_descriptor_binding.layout_binding.descriptorCount;
    if (descriptor_count == 1 && !p_descriptor_binding.variable_descriptor_count) {
        const auto texture_binding =
            find_texture_binding(p_builder.texture_bindings, p_descriptor_binding.parameter_name, p_builder.default_texture);
        p_descriptor_set->write_descriptor_set(RODescriptorSetVK::Writer()
                .set_binding(p_descriptor_binding.layout_binding.binding)
                .add_sampled_image_info(RODescriptorSetVK::Writer::ImageInfo{
                    .image_view = texture_binding.image_view,
                    .image_layout = texture_binding.image_layout,
                }));
        return;
    }
    validate_indexed_texture_bindings(p_builder.texture_bindings, p_descriptor_binding.parameter_name, descriptor_count);
    for (uint32_t i = 0; i < descriptor_count; ++i) {
        const ROMaterialVK::TextureBinding* texture_binding =
            find_indexed_texture_binding(p_builder.texture_bindings, p_descriptor_binding.parameter_name, i);
        if (texture_binding == nullptr) {
            // Descriptor array default-fill: callers may bind only element 0 for resources with fewer active entries than
            // the shader array size. Missing slots reuse element 0 so every descriptor remains valid for Vulkan validation.
            texture_binding = find_indexed_texture_binding_fallback(p_builder.texture_bindings, p_descriptor_binding.parameter_name);
        }
        if (texture_binding == nullptr && p_builder.default_texture.image_view != MEM_NULL) {
            texture_binding = &p_builder.default_texture;
        }
        if (texture_binding == nullptr) {
            throw std::runtime_error(std::format("Failed to bind material texture array '{}': no fallback binding at index 0.",
                p_descriptor_binding.parameter_name));
        }
        p_descriptor_set->write_descriptor_set(RODescriptorSetVK::Writer()
                .set_binding(p_descriptor_binding.layout_binding.binding)
                .set_dst_array_element(i)
                .add_sampled_image_info(RODescriptorSetVK::Writer::ImageInfo{
                    .image_view = texture_binding->image_view,
                    .image_layout = texture_binding->image_layout,
                }));
    }
}

void write_indexed_sampler_bindings(Ref<RODescriptorSetVK> p_descriptor_set,
    const ROMaterialVK::Builder& p_builder,
    const ShaderDescriptorBindingVK& p_descriptor_binding) {
    validate_indexed_sampler_bindings(
        p_builder.sampler_bindings, p_descriptor_binding.parameter_name, p_descriptor_binding.layout_binding.descriptorCount);
    for (uint32_t i = 0; i < p_descriptor_binding.layout_binding.descriptorCount; ++i) {
        Ref<ROSamplerVK> sampler =
            find_indexed_sampler_binding(p_builder.sampler_bindings, p_descriptor_binding.parameter_name, i, MEM_NULL);
        if (sampler == MEM_NULL) {
            // Descriptor array default-fill: callers may bind only element 0 for resources with fewer active entries than
            // the shader array size. Missing slots reuse element 0 so every descriptor remains valid for Vulkan validation.
            sampler = find_indexed_sampler_binding_fallback(p_builder.sampler_bindings, p_descriptor_binding.parameter_name);
        }
        if (sampler == MEM_NULL) {
            sampler = p_builder.default_sampler;
        }
        if (sampler == MEM_NULL) {
            throw std::runtime_error(std::format("Failed to bind material sampler array '{}': no fallback binding at index 0.",
                p_descriptor_binding.parameter_name));
        }
        p_descriptor_set->write_descriptor_set(RODescriptorSetVK::Writer()
                .set_binding(p_descriptor_binding.layout_binding.binding)
                .set_dst_array_element(i)
                .add_sampler_info(RODescriptorSetVK::Writer::ImageInfo{.sampler = sampler}));
    }
}

void write_sampler_binding(Ref<RODescriptorSetVK> p_descriptor_set,
    const ROMaterialVK::Builder& p_builder,
    const ShaderDescriptorBindingVK& p_descriptor_binding) {
    if (p_descriptor_binding.layout_binding.descriptorCount == 1) {
        p_descriptor_set->write_descriptor_set(RODescriptorSetVK::Writer()
                .set_binding(p_descriptor_binding.layout_binding.binding)
                .add_sampler_info(RODescriptorSetVK::Writer::ImageInfo{
                    .sampler = find_sampler_binding(
                        p_builder.sampler_bindings, p_descriptor_binding.parameter_name, p_builder.default_sampler),
                }));
        return;
    }
    write_indexed_sampler_bindings(p_descriptor_set, p_builder, p_descriptor_binding);
}

void write_combined_image_sampler_binding(Ref<RODescriptorSetVK> p_descriptor_set,
    const ROMaterialVK::Builder& p_builder,
    const ShaderDescriptorBindingVK& p_descriptor_binding) {
    const auto texture_binding =
        find_texture_binding(p_builder.texture_bindings, p_descriptor_binding.parameter_name, p_builder.default_texture);
    auto sampler = find_sampler_binding(p_builder.sampler_bindings, p_descriptor_binding.parameter_name, p_builder.default_sampler);
    RODescriptorSetVK::Writer writer;
    writer.set_binding(p_descriptor_binding.layout_binding.binding);
    for (uint32_t i = 0; i < p_descriptor_binding.layout_binding.descriptorCount; ++i) {
        writer.add_combined_image_sampler_info(RODescriptorSetVK::Writer::ImageInfo{
            .image_view = texture_binding.image_view,
            .sampler = sampler,
            .image_layout = texture_binding.image_layout,
        });
    }
    p_descriptor_set->write_descriptor_set(writer);
}

} // namespace

ROMaterialVK::Builder& ROMaterialVK::Builder::set_descriptor_set_layout(Ref<RODescriptorSetLayoutVK> p_descriptor_set_layout) {
    descriptor_set_layout = p_descriptor_set_layout;
    return *this;
}

ROMaterialVK::Builder& ROMaterialVK::Builder::set_set_index(uint32_t p_set_index) {
    set_index = p_set_index;
    return *this;
}

ROMaterialVK::Builder& ROMaterialVK::Builder::set_descriptor_bindings(const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings) {
    descriptor_bindings = p_descriptor_bindings;
    return *this;
}

ROMaterialVK::Builder& ROMaterialVK::Builder::add_texture_binding(const TextureBinding& p_texture_binding) {
    texture_bindings.push_back(p_texture_binding);
    return *this;
}

ROMaterialVK::Builder& ROMaterialVK::Builder::add_sampler_binding(const SamplerBinding& p_sampler_binding) {
    sampler_bindings.push_back(p_sampler_binding);
    return *this;
}

ROMaterialVK::Builder& ROMaterialVK::Builder::set_default_texture(Ref<ROImageViewVK> p_image_view, VkImageLayout p_image_layout) {
    default_texture.image_view = p_image_view;
    default_texture.image_layout = p_image_layout;
    return *this;
}

ROMaterialVK::Builder& ROMaterialVK::Builder::set_default_sampler(Ref<ROSamplerVK> p_sampler) {
    default_sampler = p_sampler;
    return *this;
}

ROMaterialVK::ROMaterialVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder) : RenderObjectVK(p_render_data) {
    if (p_builder.descriptor_set_layout == MEM_NULL) {
        throw std::runtime_error("Failed to create ROMaterialVK: descriptor set layout is missing.");
    }
    uint32_t variable_descriptor_count = get_variable_sampled_image_descriptor_count(p_builder);
    uint32_t variable_descriptor_count_upper_bound = p_builder.descriptor_set_layout->get_variable_descriptor_count_upper_bound();
    if (variable_descriptor_count != 0 && variable_descriptor_count > variable_descriptor_count_upper_bound) {
        throw std::runtime_error(std::format("Failed to create ROMaterialVK: runtime texture count {} exceeds the supported maximum of {}.",
            variable_descriptor_count,
            variable_descriptor_count_upper_bound));
    }
    RODescriptorSetVK::Builder descriptor_set_builder;
    descriptor_set_builder.add_descriptor_set_layout(p_builder.descriptor_set_layout);
    if (variable_descriptor_count != 0) {
        descriptor_set_builder.set_variable_descriptor_count(variable_descriptor_count);
    }
    descriptor_set = descriptor_set_builder.create_ref(p_render_data);
    descriptor_sets.push_back(descriptor_set);
    write_material_bindings(p_builder);
}

void ROMaterialVK::write_material_bindings(const Builder& p_builder) {
    for (const auto& descriptor_binding : p_builder.descriptor_bindings) {
        if (descriptor_binding.set_index != p_builder.set_index) {
            continue;
        }
        if (descriptor_binding.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
            write_sampled_image_binding(descriptor_set, p_builder, descriptor_binding);
            continue;
        }
        if (descriptor_binding.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) {
            write_sampler_binding(descriptor_set, p_builder, descriptor_binding);
            continue;
        }
        if (descriptor_binding.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            write_combined_image_sampler_binding(descriptor_set, p_builder, descriptor_binding);
        }
    }
}

} // namespace WhiteBirdEngine
