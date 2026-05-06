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
#include "function/render_vk/ro_staging_buffer_vk.hh"
#include "function/function_utils.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
VkBuffer ROStagingBufferVK::Builder::create_vk_object(const BaseRenderDataVK* p_render_data) const {
    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = size;
    create_info.usage = WBE_REQUIRED(usage);
    if (queue_family_indices.size() > 1) {
        create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        std::vector<uint32_t> vec_queue_family_indices(queue_family_indices.begin(), queue_family_indices.end());
        create_info.queueFamilyIndexCount = vec_queue_family_indices.size();
        create_info.pQueueFamilyIndices = vec_queue_family_indices.data();
    } else {
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    VkBuffer result;
    if (vkCreateBuffer(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer.");
    }
    return result;
}

ROStagingBufferVK::ROStagingBufferVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data) {
    buffer = p_builder.create_vk_object(p_render_data);
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(render_data->logic_device, buffer, &memory_requirements);
    exposed_fields.buffer_size = p_builder.size;
    buffer_memory = allocate_device_memory(memory_requirements, DeviceDomain::HOST);
    vkMapMemory(render_data->logic_device, buffer_memory, 0, exposed_fields.buffer_size, 0, &mapped_memory);
    if (vkBindBufferMemory(render_data->logic_device, buffer, buffer_memory, 0) != VK_SUCCESS) {
        throw std::runtime_error("Failed to bind buffer memory.");
    }
    static_cast<DataChunkHeader*>(mapped_memory)->size = exposed_fields.buffer_size;
    static_cast<DataChunkHeader*>(mapped_memory)->is_allocated = false;
    static_cast<DataChunkHeader*>(mapped_memory)->frame = 0;
}

ROStagingBufferVK::~ROStagingBufferVK() {
    vkUnmapMemory(render_data->logic_device, buffer_memory);
    // Deallocate resource after frames in flight.
    render_data->register_delayed_call(
        [render_data = render_data, buffer = buffer, buffer_memory = buffer_memory]() {
            vkDestroyBuffer(render_data->logic_device, buffer, render_data->allocation_callbacks);
            vkFreeMemory(render_data->logic_device, buffer_memory, render_data->allocation_callbacks);
        },
        render_data->frames_in_flight);
}

void* ROStagingBufferVK::allocate(size_t p_size) {
    p_size = get_align_size(p_size, DATA_CHUNK_HEADER_SIZE);
    DataChunkHeader* header = static_cast<DataChunkHeader*>(mapped_memory);
    uintptr_t end_addr = reinterpret_cast<uintptr_t>(mapped_memory) + exposed_fields.buffer_size;
    while (reinterpret_cast<uintptr_t>(header) < end_addr) {
        auto* next = reinterpret_cast<DataChunkHeader*>(reinterpret_cast<char*>(header) + header->size);
        if (header->is_allocated) {
            header = next;
            continue;
        }
        while (reinterpret_cast<uintptr_t>(next) < end_addr && !(next->is_allocated)) {
            // coalesce
            header->size += next->size;
            next = reinterpret_cast<DataChunkHeader*>(reinterpret_cast<char*>(header) + header->size);
        }
        if (header->size >= p_size + DATA_CHUNK_HEADER_SIZE) {
            if (header->size >= p_size + DATA_CHUNK_HEADER_SIZE * 2) {
                next = reinterpret_cast<DataChunkHeader*>(reinterpret_cast<char*>(header) + p_size + DATA_CHUNK_HEADER_SIZE);
                next->is_allocated = false;
                next->size = header->size - p_size - DATA_CHUNK_HEADER_SIZE;
                header->size = p_size + DATA_CHUNK_HEADER_SIZE;
            }
            header->is_allocated = true;
            // Free after frames_in_flight
            render_data->register_delayed_call(
                [header]() {
                    header->is_allocated = false;
                },
                render_data->frames_in_flight);
            return header + 1;
        }
        header += header->size / DATA_CHUNK_HEADER_SIZE;
    }
    throw std::runtime_error(std::format("Failed to allocate memory with size {}: Buffer overflow.", p_size));
}

} // namespace WhiteBirdEngine
