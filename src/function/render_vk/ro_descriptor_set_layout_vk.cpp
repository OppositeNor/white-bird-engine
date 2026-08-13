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
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RODescriptorSetLayoutVK::Builder& RODescriptorSetLayoutVK::Builder::set_bindings(const std::span<VkDescriptorSetLayoutBinding>& p_bindings) {
    bindings = std::vector<VkDescriptorSetLayoutBinding>(p_bindings.begin(), p_bindings.end());
    binding_flags.assign(bindings.size(), 0);
    return *this;
}

RODescriptorSetLayoutVK::Builder& RODescriptorSetLayoutVK::Builder::add_binding(const VkDescriptorSetLayoutBinding& p_binding) {
    return add_binding(p_binding, 0);
}

RODescriptorSetLayoutVK::Builder& RODescriptorSetLayoutVK::Builder::add_binding(
    const VkDescriptorSetLayoutBinding& p_binding, VkDescriptorBindingFlags p_binding_flags) {
    bindings.push_back(p_binding);
    binding_flags.push_back(p_binding_flags);
    return *this;
}

RODescriptorSetLayoutVK::Builder& RODescriptorSetLayoutVK::Builder::add_ubo(
    uint32_t p_binding, uint32_t p_descriptor_count, VkShaderStageFlags p_shader_stages) {
    add_binding({
        .binding = p_binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = p_descriptor_count,
        .stageFlags = p_shader_stages,
    });
    return *this;
}

VkDescriptorSetLayout RODescriptorSetLayoutVK::Builder::create_vk_object(const BaseRenderDataVK* p_render_data) const {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Render data is nullptr.");
    }
    if (bindings.size() != binding_flags.size()) {
        throw std::runtime_error("Failed to create descriptor set layout: binding flags do not match bindings.");
    }
    uint32_t variable_binding_count = 0;
    uint32_t highest_binding = 0;
    for (const auto& binding : bindings) {
        highest_binding = std::max(highest_binding, binding.binding);
    }
    for (size_t i = 0; i < bindings.size(); ++i) {
        if ((binding_flags[i] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) == 0) {
            continue;
        }
        ++variable_binding_count;
        if (bindings[i].binding != highest_binding) {
            throw std::runtime_error(
                "Failed to create descriptor set layout: the variable-count binding must have the highest binding number.");
        }
        if (bindings[i].descriptorCount == 0) {
            throw std::runtime_error("Failed to create descriptor set layout: the variable-count binding must have a non-zero upper bound.");
        }
    }
    if (variable_binding_count > 1) {
        throw std::runtime_error("Failed to create descriptor set layout: only one variable-count binding is allowed.");
    }
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info{};
    binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags_create_info.bindingCount = static_cast<uint32_t>(binding_flags.size());
    binding_flags_create_info.pBindingFlags = binding_flags.data();
    VkDescriptorSetLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.pNext = binding_flags.empty() ? nullptr : &binding_flags_create_info;
    create_info.bindingCount = bindings.size();
    create_info.pBindings = bindings.data();

    VkDescriptorSetLayout result;
    if (vkCreateDescriptorSetLayout(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout.");
    }
    return result;
}

RODescriptorSetLayoutVK::RODescriptorSetLayoutVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data) {
    init(p_builder);
}

RODescriptorSetLayoutVK::~RODescriptorSetLayoutVK() {
    // Deallocate resource after frames in flight.
    render_data->register_delayed_call(
        [render_data = render_data, descriptor_set_layout = descriptor_set_layout]() {
            vkDestroyDescriptorSetLayout(render_data->logic_device, descriptor_set_layout, render_data->allocation_callbacks);
        },
        render_data->frames_in_flight);
}

void RODescriptorSetLayoutVK::init(const Builder& p_builder) {
    descriptor_set_layout = p_builder.create_vk_object(render_data);
    for (size_t i = 0; i < p_builder.bindings.size(); ++i) {
        if ((p_builder.binding_flags[i] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) != 0) {
            variable_descriptor_count_upper_bound = p_builder.bindings[i].descriptorCount;
            break;
        }
    }
}

} // namespace WhiteBirdEngine
