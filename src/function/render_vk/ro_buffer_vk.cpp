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
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "function/render_vk/renderer_vk.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

VkBuffer ROBufferVK::Builder::build(const BaseRenderDataVK* p_render_data) const {
    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = size;
    create_info.usage = WBE_REQUIRED(usage);
    if (queue_family_indices.size() > 1) {
        create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        std::vector<uint32_t> vec_queue_family_indices(queue_family_indices.begin(), queue_family_indices.end());
        create_info.queueFamilyIndexCount = vec_queue_family_indices.size();
        create_info.pQueueFamilyIndices = vec_queue_family_indices.data();
    }
    else {
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    VkBuffer result;
    if (vkCreateBuffer(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer.");
    }
    return result;
}

ROBufferVK::ROBufferVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data) {
    buffer = p_builder.build(p_render_data);
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(render_data->logic_device, buffer, &memory_requirements);
    buffer_memory = allocate_device_memory(memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    exposed_fields.buffer_size = p_builder.size;
    vkMapMemory(render_data->logic_device, buffer_memory, 0, exposed_fields.buffer_size, 0, &mapped_memory);
}

ROBufferVK::~ROBufferVK() {
    vkDestroyBuffer(render_data->logic_device, buffer, render_data->allocation_callbacks);
    deallocate_device_memory(buffer_memory);
}

void ROBufferVK::update_buffer(const void* p_data, size_t p_offset, size_t p_size) {
    if (p_offset + p_size > exposed_fields.buffer_size) {
        throw std::runtime_error(
            std::format("Failed to update buffer: trying to map a region ({} to {}) onto a buffer with capacity {}.",
                        p_offset, p_offset + p_size, exposed_fields.buffer_size));
    }
    // TODO: Prevent data race
    memcpy(mapped_memory, p_data, p_size);
    vkUnmapMemory(render_data->logic_device, buffer_memory);
}

void ROBufferVK::update_buffer(const void* p_data, size_t p_size) {
    update_buffer(p_data, 0, p_size);
}

void ROBufferVK::update_buffer(Ref<ROBufferVK> p_data, size_t p_src_offset, size_t p_dst_offset, size_t p_size) {
    if (p_dst_offset + p_size > exposed_fields.buffer_size) {
        throw std::runtime_error(
            std::format("Failed to update buffer: trying to map a region ({} to {}) onto a buffer with capacity {}.",
                        p_dst_offset, p_dst_offset + p_size, exposed_fields.buffer_size));
    }
    if (p_src_offset + p_size > p_data->exposed_fields.buffer_size) {
        throw std::runtime_error(
            std::format("Failed to update buffer: trying to read a region ({} to {}) from a buffer with capacity {}.",
                        p_src_offset, p_src_offset + p_size, p_data->exposed_fields.buffer_size));
    }
    VkBufferCopy copy_region{};
    copy_region.srcOffset = p_src_offset;
    copy_region.dstOffset = p_dst_offset;
    copy_region.size = p_size;
    // Register a pre-execute task to copy the buffer.
    RendererVK::get_singleton()->add_pre_execute_task(
        [src_buffer = p_data->get_buffer(), dst_buffer = this->get_buffer(), copy_region](VkCommandBuffer p_command_buffer) {
            vkCmdCopyBuffer(p_command_buffer, src_buffer, dst_buffer, 1, &copy_region);
        }
    );
}

void ROBufferVK::update_buffer(Ref<ROBufferVK> p_data, size_t p_offset, size_t p_size) {
    update_buffer(p_data, 0, p_offset, p_size);
}

void ROBufferVK::update_buffer(Ref<ROBufferVK> p_data, size_t p_size) {
    update_buffer(p_data, 0, 0, p_size);
}

void ROBufferVK::update_buffer(Ref<ROBufferVK> p_data) {
    update_buffer(p_data, 0, 0, p_data->exposed_fields.buffer_size);
}

}
