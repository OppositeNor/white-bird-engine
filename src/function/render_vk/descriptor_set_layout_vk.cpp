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
#include "function/render_vk/descriptor_set_layout_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

DescriptorSetLayoutVK::DescriptorSetLayoutVK(const RendererVK::BaseRenderDataVK* p_render_data, const Config& p_config)
    : render_data(p_render_data) {
    init(p_config);
}

DescriptorSetLayoutVK::~DescriptorSetLayoutVK() {
    vkDestroyDescriptorSetLayout(render_data->logic_device, descriptor_set_layout, render_data->allocation_callbacks);
}

void DescriptorSetLayoutVK::init(const Config& p_config) {
    VkDescriptorSetLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.bindingCount = p_config.bindings.size();
    create_info.pBindings = p_config.bindings.data();
    if (vkCreateDescriptorSetLayout(render_data->logic_device, &create_info, render_data->allocation_callbacks, &descriptor_set_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout.");
    }
}

}
