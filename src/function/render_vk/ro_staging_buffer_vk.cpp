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
#include "core/core_utils.hh"
#include "function/function_utils.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/vma_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TypeID ROStagingBufferVK::get_type_uuid() const {
    return TypeUUID<ROStagingBufferVK>::UUID;
}

ROStagingBufferVK::ROStagingBufferVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : IRenderObjectVK(p_render_data) {
    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = p_builder.size;
    create_info.usage = WBE_REQUIRED(p_builder.usage);
    std::vector<uint32_t> queue_family_indices(p_builder.queue_family_indices.begin(), p_builder.queue_family_indices.end());
    if (queue_family_indices.size() > 1) {
        create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices.size());
        create_info.pQueueFamilyIndices = queue_family_indices.data();
    } else {
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    exposed_fields.buffer_size = p_builder.size;
    VmaAllocationCreateInfo allocation_create_info{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    VmaAllocationInfo allocation_info{};
    VkResult result =
        vmaCreateBuffer(render_data->vma_allocator, &create_info, &allocation_create_info, &buffer, &allocation, &allocation_info);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to create Vulkan staging buffer: {}.", string_VkResult(result)));
    }
    mapped_memory = allocation_info.pMappedData;
    static_cast<DataChunkHeader*>(mapped_memory)->size = exposed_fields.buffer_size;
    static_cast<DataChunkHeader*>(mapped_memory)->is_allocated = false;
    static_cast<DataChunkHeader*>(mapped_memory)->frame = 0;
}

ROStagingBufferVK::~ROStagingBufferVK() {
    // Deallocate resource after frames in flight.
    render_data->register_delayed_call(
        [render_data = render_data, buffer = buffer, allocation = allocation]() {
            vmaDestroyBuffer(render_data->vma_allocator, buffer, allocation);
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
