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
#include "function/render_vk/render_vk_defs.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROPipelineLayoutVK::Builder& ROPipelineLayoutVK::Builder::set_descriptor_set_layouts(const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts) {
    descriptor_set_layouts = p_descriptor_set_layouts;
    return *this;
}

ROPipelineLayoutVK::Builder& ROPipelineLayoutVK::Builder::add_descriptor_set_layout(Ref<RODescriptorSetLayoutVK> p_descriptor_set_layout) {
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

VkPipelineLayout ROPipelineLayoutVK::Builder::build(const BaseRenderDataVK* p_render_data) const {
    VkPipelineLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts_vk;
    descriptor_set_layouts_vk.reserve(descriptor_set_layouts.size());
    for (const auto& descriptor_set_layout : descriptor_set_layouts) {
        descriptor_set_layouts_vk.push_back(descriptor_set_layout->get_descriptor_set_layout());
    }
    create_info.setLayoutCount = descriptor_set_layouts_vk.size();
    create_info.pSetLayouts = descriptor_set_layouts_vk.data();
    create_info.pushConstantRangeCount = push_constant_ranges.size();
    create_info.pPushConstantRanges = push_constant_ranges.data();
    
    VkPipelineLayout result;
    if (vkCreatePipelineLayout(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout.");
    }
    return result;
}

ROPipelineLayoutVK::ROPipelineLayoutVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data), exposed_fields(p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Render data is nullptr.");
    }
    init(p_builder);
}

ROPipelineLayoutVK::ROPipelineLayoutVK(const BaseRenderDataVK* p_render_data)
    : RenderObjectVK(p_render_data), exposed_fields(p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Render data is nullptr.");
    }
    // Use default builder
    Builder builder;
    init(builder);
}

ROPipelineLayoutVK::~ROPipelineLayoutVK() {
    render_data->register_delayed_call(
        [render_data = render_data, pipeline_layout = pipeline_layout] (){
            vkDestroyPipelineLayout(render_data->logic_device, pipeline_layout, render_data->allocation_callbacks);
        }, render_data->frames_in_flight);
}

void ROPipelineLayoutVK::init(const Builder& p_builder) {
    pipeline_layout = p_builder.build(render_data);
    exposed_fields.descriptor_set_layouts.reserve(p_builder.descriptor_set_layouts.size());
    for (const auto& descriptor_set_layout : p_builder.descriptor_set_layouts) {
        exposed_fields.descriptor_set_layouts.push_back(descriptor_set_layout);
    }
}

} // namespace WhiteBirdEngine
