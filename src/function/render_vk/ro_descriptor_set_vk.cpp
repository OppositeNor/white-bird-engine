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
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "utils/defs.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

VkDescriptorSet RODescriptorSetVK::Builder::build(const BaseRenderDataVK* p_render_data) const {
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = p_render_data->descriptor_pool;
    std::vector<VkDescriptorSetLayout> layouts;
    layouts.reserve(descriptor_set_layouts.size());
    for (const auto& layout : descriptor_set_layouts) {
        layouts.push_back(layout->get_descriptor_set_layout());
    }
    alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();
    VkDescriptorSet result;
    if (vkAllocateDescriptorSets(p_render_data->logic_device, &alloc_info, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set.");
    }
    return result;
}

RODescriptorSetVK::RODescriptorSetVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data), descriptor_set_layouts(p_render_data->allocator), bound_buffers(p_render_data->allocator),
      bound_image_views(p_render_data->allocator) {
    descriptor_set = p_builder.build(p_render_data);
    descriptor_set_layouts.reserve(p_builder.descriptor_set_layouts.size());
    for (const auto& layout : p_builder.descriptor_set_layouts) {
        descriptor_set_layouts.push_back(layout);
    }
}

RODescriptorSetVK::~RODescriptorSetVK() {
    vkFreeDescriptorSets(render_data->logic_device, render_data->descriptor_pool, 1, &descriptor_set);
}

void RODescriptorSetVK::write_descriptor_set(const Writer& p_writer) {
    VkWriteDescriptorSet write_info{};
    write_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_info.dstSet = descriptor_set;
    write_info.dstBinding = WBE_REQUIRED(p_writer.binding);
    write_info.dstArrayElement = 0;
    write_info.descriptorCount = 1;
    VkDescriptorBufferInfo buffer_info{};
    if (p_writer.buffer_info.buffer != MEM_NULL) {
        buffer_info.buffer = p_writer.buffer_info.buffer->get_buffer();
        buffer_info.offset = p_writer.buffer_info.offset;
        buffer_info.range = p_writer.buffer_info.range;
        write_info.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write_info.pBufferInfo = &buffer_info;
        bound_buffers[WBE_REQUIRED(p_writer.binding)] = p_writer.buffer_info.buffer;
        bound_image_views.erase(WBE_REQUIRED(p_writer.binding));
    }
    // TODO: Image view
    vkUpdateDescriptorSets(render_data->logic_device, 1, &write_info, 0, nullptr);
}

} // namespace WhiteBirdEngine
