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
#include "function/render_vk/renderer_vk.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RODescriptorSetLayoutVK::Builder& RODescriptorSetLayoutVK::Builder::set_bindings(const std::vector<VkDescriptorSetLayoutBinding>& p_bindings) {
    bindings = p_bindings;
    return *this;
}

RODescriptorSetLayoutVK::Builder& RODescriptorSetLayoutVK::Builder::add_binding(const VkDescriptorSetLayoutBinding& p_binding) {
    bindings.push_back(p_binding);
    return *this;
}

VkDescriptorSetLayout RODescriptorSetLayoutVK::Builder::build(const RendererVK::BaseRenderDataVK* p_render_data) const {
    VkDescriptorSetLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.bindingCount = bindings.size();
    create_info.pBindings = bindings.data();
    
    VkDescriptorSetLayout result;
    if (vkCreateDescriptorSetLayout(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout.");
    }
    return result;
}

RODescriptorSetLayoutVK::RODescriptorSetLayoutVK(const RendererVK::BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : render_data(p_render_data) {
    init(p_builder);
}

RODescriptorSetLayoutVK::~RODescriptorSetLayoutVK() {
    vkDestroyDescriptorSetLayout(render_data->logic_device, descriptor_set_layout, render_data->allocation_callbacks);
}

void RODescriptorSetLayoutVK::init(const Builder& p_builder) {
    descriptor_set_layout = p_builder.build(render_data);
}

}
