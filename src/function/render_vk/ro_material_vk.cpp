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
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include <algorithm>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

// Material texture keys are shader parameter names. This keeps manifest
// material slots independent of backend descriptor binding numbers.
const ROMaterialVK::TextureBinding& find_texture_binding(
    const std::vector<ROMaterialVK::TextureBinding>& p_texture_bindings, const std::string& p_texture_key) {
    auto binding = std::ranges::find_if(p_texture_bindings, [&p_texture_key](const ROMaterialVK::TextureBinding& p_binding) {
        return p_binding.texture_key == p_texture_key;
    });
    if (binding == p_texture_bindings.end() || binding->image_view == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to bind material texture '{}': image view is missing.", p_texture_key));
    }
    return *binding;
}

// A default sampler lets materials omit one sampler slot while still binding
// separate texture and sampler descriptors from Slang reflection metadata.
Ref<ROSamplerVK> find_sampler_binding(
    const std::vector<ROMaterialVK::SamplerBinding>& p_sampler_bindings, const std::string& p_sampler_key, Ref<ROSamplerVK> p_default_sampler) {
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

ROMaterialVK::Builder& ROMaterialVK::Builder::set_default_sampler(Ref<ROSamplerVK> p_sampler) {
    default_sampler = p_sampler;
    return *this;
}

ROMaterialVK::ROMaterialVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder) : RenderObjectVK(p_render_data) {
    if (p_builder.descriptor_set_layout == MEM_NULL) {
        throw std::runtime_error("Failed to create ROMaterialVK: descriptor set layout is missing.");
    }
    descriptor_set = RODescriptorSetVK::Builder().add_descriptor_set_layout(p_builder.descriptor_set_layout).create_ref(p_render_data);
    descriptor_sets.push_back(descriptor_set);
    write_material_bindings(p_builder);
}

void ROMaterialVK::write_material_bindings(const Builder& p_builder) {
    for (const auto& descriptor_binding : p_builder.descriptor_bindings) {
        if (descriptor_binding.set_index != p_builder.set_index) {
            continue;
        }
        RODescriptorSetVK::Writer writer;
        writer.set_binding(descriptor_binding.layout_binding.binding);
        // The reflected descriptor type decides which writer path is valid for
        // this shader parameter.
        if (descriptor_binding.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
            const auto& texture_binding = find_texture_binding(p_builder.texture_bindings, descriptor_binding.parameter_name);
            writer.add_sampled_image_info(RODescriptorSetVK::Writer::ImageInfo{
                .image_view = texture_binding.image_view,
                .image_layout = texture_binding.image_layout,
            });
            descriptor_set->write_descriptor_set(writer);
            continue;
        }
        if (descriptor_binding.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) {
            writer.add_sampler_info(RODescriptorSetVK::Writer::ImageInfo{
                .sampler = find_sampler_binding(p_builder.sampler_bindings, descriptor_binding.parameter_name, p_builder.default_sampler),
            });
            descriptor_set->write_descriptor_set(writer);
            continue;
        }
        if (descriptor_binding.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            const auto& texture_binding = find_texture_binding(p_builder.texture_bindings, descriptor_binding.parameter_name);
            writer.add_combined_image_sampler_info(RODescriptorSetVK::Writer::ImageInfo{
                .image_view = texture_binding.image_view,
                .sampler = find_sampler_binding(p_builder.sampler_bindings, descriptor_binding.parameter_name, p_builder.default_sampler),
                .image_layout = texture_binding.image_layout,
            });
            descriptor_set->write_descriptor_set(writer);
        }
    }
}

} // namespace WhiteBirdEngine