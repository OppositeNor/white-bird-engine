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
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "global/stl_allocator.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {
void write_buffer_info(VkDescriptorType p_descriptor_type,
    VkWriteDescriptorSet& p_write_info,
    const std::vector<RODescriptorSetVK::Writer::BufferInfo>& p_buffer_infos,
    Map<uint32_t, Vector<Ref<RenderObjectVK>>>::iterator p_bound_object_it,
    std::vector<VkDescriptorBufferInfo>& p_result_buffer_infos) {
    WBE_DEBUG_ASSERT(p_result_buffer_infos.empty());
    p_write_info.descriptorType = p_descriptor_type;
    p_result_buffer_infos.reserve(p_buffer_infos.size());
    p_bound_object_it->second.reserve(p_buffer_infos.size());
    for (const auto& buffer_info : p_buffer_infos) {
        VkDescriptorBufferInfo vk_buffer_info{};
        vk_buffer_info.buffer = buffer_info.buffer->get_buffer();
        vk_buffer_info.offset = buffer_info.offset;
        vk_buffer_info.range = buffer_info.range;
        p_result_buffer_infos.push_back(vk_buffer_info);
        // Keep a reference to the buffer to prevent it from being destroyed.
        p_bound_object_it->second.emplace_back(buffer_info.buffer);
    }
    p_write_info.descriptorCount = p_result_buffer_infos.size();
    p_write_info.pBufferInfo = p_result_buffer_infos.data();
}

void write_image_info(VkDescriptorType p_descriptor_type,
    VkWriteDescriptorSet& p_write_info,
    const std::vector<RODescriptorSetVK::Writer::ImageInfo>& p_image_infos,
    Map<uint32_t, Vector<Ref<RenderObjectVK>>>::iterator p_bound_object_it,
    std::vector<VkDescriptorImageInfo>& p_result_image_infos) {
    WBE_DEBUG_ASSERT(p_result_image_infos.empty());
    p_write_info.descriptorType = p_descriptor_type;
    p_result_image_infos.reserve(p_image_infos.size());
    p_bound_object_it->second.reserve(p_image_infos.size() * 2);
    for (const auto& image_info : p_image_infos) {
        VkDescriptorImageInfo vk_image_info{};
        vk_image_info.imageLayout = image_info.image_layout;
        if (p_descriptor_type != VK_DESCRIPTOR_TYPE_SAMPLER) {
            if (image_info.image_view == MEM_NULL) {
                throw std::runtime_error("Failed to write descriptor set: image view is required.");
            }
            vk_image_info.imageView = image_info.image_view->get_image_view();
            // Keep the image view alive for as long as this descriptor set uses it.
            p_bound_object_it->second.emplace_back(image_info.image_view);
        }
        if (p_descriptor_type != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
            if (image_info.sampler == MEM_NULL) {
                throw std::runtime_error("Failed to write descriptor set: sampler is required.");
            }
            vk_image_info.sampler = image_info.sampler->get_sampler();
            // Keep the sampler alive for as long as this descriptor set uses it.
            p_bound_object_it->second.emplace_back(image_info.sampler);
        }
        p_result_image_infos.push_back(vk_image_info);
    }
    p_write_info.descriptorCount = p_result_image_infos.size();
    p_write_info.pImageInfo = p_result_image_infos.data();
}

} // namespace

VkDescriptorSet RODescriptorSetVK::Builder::create_vk_object(const BaseRenderDataVK* p_render_data) const {
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
    : RenderObjectVK(p_render_data), descriptor_set_layouts(p_render_data->allocator), bound_objects(p_render_data->allocator) {
    descriptor_set = p_builder.create_vk_object(p_render_data);
    descriptor_set_layouts.reserve(p_builder.descriptor_set_layouts.size());
    for (const auto& layout : p_builder.descriptor_set_layouts) {
        descriptor_set_layouts.push_back(layout);
    }
}

RODescriptorSetVK::~RODescriptorSetVK() {
    vkFreeDescriptorSets(render_data->logic_device, render_data->descriptor_pool, 1, &descriptor_set);
}

void RODescriptorSetVK::write_descriptor_set(const Writer& p_writer) {
    // Make sure only one type of resource is being written.
    size_t resource_type_count = 0;
    if (!p_writer.storage_buffer_infos.empty()) {
        resource_type_count++;
    }
    if (!p_writer.uniform_buffer_infos.empty()) {
        resource_type_count++;
    }
    if (!p_writer.sampled_image_infos.empty()) {
        resource_type_count++;
    }
    if (!p_writer.sampler_infos.empty()) {
        resource_type_count++;
    }
    if (!p_writer.combined_image_sampler_infos.empty()) {
        resource_type_count++;
    }
    if (resource_type_count != 1) {
        throw std::runtime_error("Failed to write descriptor set: only one "
                                 "type of resource can be written at a time.");
    }
    VkWriteDescriptorSet write_info{};
    write_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_info.dstSet = descriptor_set;
    write_info.dstBinding = WBE_REQUIRED(p_writer.binding);
    write_info.dstArrayElement = 0;
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    // Prepare buffer infos
    auto emplace_result = bound_objects.emplace(write_info.dstBinding, Vector<Ref<RenderObjectVK>>(render_data->allocator));
    auto bound_object_it = emplace_result.first;
    if (!emplace_result.second) {
        // If previously existed, clear the bound objects.
        bound_object_it->second.clear();
    }

    if (!p_writer.storage_buffer_infos.empty()) {
        write_buffer_info(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, write_info, p_writer.storage_buffer_infos, bound_object_it, buffer_infos);
    }
    if (!p_writer.uniform_buffer_infos.empty()) {
        write_buffer_info(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, write_info, p_writer.uniform_buffer_infos, bound_object_it, buffer_infos);
    }
    std::vector<VkDescriptorImageInfo> image_infos;
    if (!p_writer.sampled_image_infos.empty()) {
        write_image_info(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, write_info, p_writer.sampled_image_infos, bound_object_it, image_infos);
    }
    if (!p_writer.sampler_infos.empty()) {
        write_image_info(VK_DESCRIPTOR_TYPE_SAMPLER, write_info, p_writer.sampler_infos, bound_object_it, image_infos);
    }
    if (!p_writer.combined_image_sampler_infos.empty()) {
        write_image_info(
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, write_info, p_writer.combined_image_sampler_infos, bound_object_it, image_infos);
    }
    // TODO: Texel buffer view
    vkUpdateDescriptorSets(render_data->logic_device, 1, &write_info, 0, nullptr);
}

} // namespace WhiteBirdEngine
