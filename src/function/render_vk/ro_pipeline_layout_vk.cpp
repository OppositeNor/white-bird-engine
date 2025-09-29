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
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROPipelineLayoutVK::Builder& ROPipelineLayoutVK::Builder::set_descriptor_set_layouts(const std::vector<VkDescriptorSetLayout>& p_descriptor_set_layouts) {
    descriptor_set_layouts = p_descriptor_set_layouts;
    return *this;
}

ROPipelineLayoutVK::Builder& ROPipelineLayoutVK::Builder::add_descriptor_set_layout(VkDescriptorSetLayout p_descriptor_set_layout) {
    descriptor_set_layouts.push_back(p_descriptor_set_layout);
    return *this;
}

ROPipelineLayoutVK::Builder& ROPipelineLayoutVK::Builder::set_push_constant_ranges(const std::vector<VkPushConstantRange>& p_push_constant_ranges) {
    push_constant_ranges = p_push_constant_ranges;
    return *this;
}

ROPipelineLayoutVK::Builder& ROPipelineLayoutVK::Builder::add_push_constant_range(const VkPushConstantRange& p_push_constant_range) {
    push_constant_ranges.push_back(p_push_constant_range);
    return *this;
}

VkPipelineLayout ROPipelineLayoutVK::Builder::build(const RendererVK::BaseRenderDataVK* p_render_data) const {
    VkPipelineLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    create_info.setLayoutCount = descriptor_set_layouts.size();
    create_info.pSetLayouts = descriptor_set_layouts.data();
    create_info.pushConstantRangeCount = push_constant_ranges.size();
    create_info.pPushConstantRanges = push_constant_ranges.data();
    
    VkPipelineLayout result;
    if (vkCreatePipelineLayout(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout.");
    }
    return result;
}

ROPipelineLayoutVK::ROPipelineLayoutVK(const RendererVK::BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : render_data(p_render_data) {
    init(p_builder);
}

ROPipelineLayoutVK::~ROPipelineLayoutVK() {
    vkDestroyPipelineLayout(render_data->logic_device, pipeline_layout, render_data->allocation_callbacks);
}

void ROPipelineLayoutVK::init(const Builder& p_builder) {
    pipeline_layout = p_builder.build(render_data);
}

}
