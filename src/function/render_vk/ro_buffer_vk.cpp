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
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/function_utils.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/descriptor_heap_allocator_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/vma_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

DebugData make_descriptor_row(
    std::string_view p_resource_type, std::string_view p_descriptor_type, uint32_t p_heap_index, VkDeviceAddress p_device_address) {
    DebugData row;
    row.set_value("resource_type", std::string(p_resource_type));
    row.set_value("descriptor_type", std::string(p_descriptor_type));
    row.set_value("heap", "resource");
    row.set_value("heap_index", p_heap_index);
    row.set_value("device_address", std::format("0x{:x}", p_device_address));
    return row;
}

} // namespace

TypeID ROBufferVK::get_type_uuid() const {
    return TypeUUID<ROBufferVK>::UUID;
}

static VmaAllocationCreateInfo buffer_allocation_create_info(DeviceDomain p_domain) {
    VmaAllocationCreateInfo result{};
    switch (p_domain) {
    case DeviceDomain::HOST:
        result.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        result.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        result.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
    case DeviceDomain::DEVICE:
        result.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        result.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    case DeviceDomain::HOST_DEVICE:
        result.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        result.usage = VMA_MEMORY_USAGE_AUTO;
        result.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
    }
    return result;
}

static VkDeviceAddressRangeEXT get_buffer_address_range(const BaseRenderDataVK* p_render_data, VkBuffer p_buffer, VkDeviceSize p_size) {
    VkBufferDeviceAddressInfo address_info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = p_buffer};
    return VkDeviceAddressRangeEXT{.address = vkGetBufferDeviceAddress(p_render_data->logic_device, &address_info), .size = p_size};
}

ROBufferVK::ROBufferVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder) : IRenderObjectVK(p_render_data) {
    if (p_builder.uniform_view_index.has_value() && p_builder.storage_view_index.has_value() &&
        p_builder.uniform_view_index == p_builder.storage_view_index) {
        throw std::runtime_error("Failed to create buffer: uniform and storage descriptors cannot adopt the same heap index.");
    }
    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = p_builder.size;
    usage = WBE_REQUIRED(p_builder.usage);
    if ((usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) != 0) {
        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    create_info.usage = usage;
    std::vector<uint32_t> initial_queues;
    if (p_builder.queue_family_indices.empty()) {
        initial_queues =
            std::vector<uint32_t>(p_render_data->queue_family_indices.begin(), p_render_data->queue_family_indices.end());
    } else {
        initial_queues = std::vector<uint32_t>(p_builder.queue_family_indices.begin(), p_builder.queue_family_indices.end());
    }
    if (initial_queues.size() > 1) {
        create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.pQueueFamilyIndices = initial_queues.data();
        create_info.queueFamilyIndexCount = static_cast<uint32_t>(initial_queues.size());
    } else {
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    exposed_fields.buffer_size = p_builder.size;
    exposed_fields.domain = WBE_REQUIRED(p_builder.domain);
    if (p_builder.uniform_view_index.has_value()) {
        if ((usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) == 0) {
            throw std::runtime_error("Failed to adopt uniform-buffer descriptor index: buffer does not have uniform-buffer usage.");
        }
        if (render_data->descriptor_heap_allocator == nullptr ||
            !render_data->descriptor_heap_allocator->owns_buffer_range(*p_builder.uniform_view_index)) {
            throw std::runtime_error("Failed to adopt uniform-buffer descriptor index: the index is not allocated by the buffer arena.");
        }
    }
    if (p_builder.storage_view_index.has_value()) {
        if ((usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) == 0) {
            throw std::runtime_error("Failed to adopt storage-buffer descriptor index: buffer does not have storage-buffer usage.");
        }
        if (render_data->descriptor_heap_allocator == nullptr ||
            !render_data->descriptor_heap_allocator->owns_buffer_range(*p_builder.storage_view_index)) {
            throw std::runtime_error("Failed to adopt storage-buffer descriptor index: the index is not allocated by the buffer arena.");
        }
    }
    VmaAllocationCreateInfo allocation_create_info = buffer_allocation_create_info(exposed_fields.domain);
    VmaAllocationInfo allocation_info{};
    VkResult result =
        vmaCreateBuffer(render_data->vma_allocator, &create_info, &allocation_create_info, &buffer, &allocation, &allocation_info);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to create Vulkan buffer: {}.", string_VkResult(result)));
    }
    mapped_memory = allocation_info.pMappedData;
    if (p_builder.uniform_view_index.has_value()) {
        uniform_view_index = p_builder.uniform_view_index;
        render_data->descriptor_heap_allocator->write_buffer_descriptor(*uniform_view_index,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            get_buffer_address_range(render_data, buffer, exposed_fields.buffer_size));
    }
    if (p_builder.storage_view_index.has_value()) {
        storage_view_index = p_builder.storage_view_index;
        render_data->descriptor_heap_allocator->write_buffer_descriptor(*storage_view_index,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            get_buffer_address_range(render_data, buffer, exposed_fields.buffer_size));
    }
}

ROBufferVK::~ROBufferVK() {
    // Deallocate resource after frames in flight.
    render_data->register_delayed_call(
        [render_data = render_data, buffer = buffer, allocation = allocation, uniform_view_index = uniform_view_index, storage_view_index = storage_view_index]() mutable {
            if (uniform_view_index.has_value()) {
                render_data->descriptor_heap_allocator->deallocate_buffer(*uniform_view_index);
            }
            if (storage_view_index.has_value()) {
                render_data->descriptor_heap_allocator->deallocate_buffer(*storage_view_index);
            }
            vmaDestroyBuffer(render_data->vma_allocator, buffer, allocation);
        },
        render_data->frames_in_flight);
}

VkDeviceAddress ROBufferVK::get_device_address() const {
    VkBufferDeviceAddressInfo address_info{};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.buffer = buffer;
    return vkGetBufferDeviceAddress(render_data->logic_device, &address_info);
}

uint32_t ROBufferVK::get_uniform_view_index() const {
    if ((usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) == 0) {
        throw std::runtime_error("Failed to create uniform-buffer descriptor: buffer does not have uniform-buffer usage.");
    }
    if (!uniform_view_index.has_value()) {
        DescriptorHeapAllocatorVK* allocator = render_data->descriptor_heap_allocator;
        if (allocator == nullptr) {
            throw std::runtime_error("Failed to create uniform-buffer descriptor: descriptor heap allocator is unavailable.");
        }
        uniform_view_index = allocator->allocate_buffer();
        allocator->write_buffer_descriptor(*uniform_view_index,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            get_buffer_address_range(render_data, buffer, exposed_fields.buffer_size));
    }
    return *uniform_view_index;
}

uint32_t ROBufferVK::get_storage_view_index() const {
    if ((usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) == 0) {
        throw std::runtime_error("Failed to create storage-buffer descriptor: buffer does not have storage-buffer usage.");
    }
    if (!storage_view_index.has_value()) {
        DescriptorHeapAllocatorVK* allocator = render_data->descriptor_heap_allocator;
        if (allocator == nullptr) {
            throw std::runtime_error("Failed to create storage-buffer descriptor: descriptor heap allocator is unavailable.");
        }
        storage_view_index = allocator->allocate_buffer();
        allocator->write_buffer_descriptor(*storage_view_index,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            get_buffer_address_range(render_data, buffer, exposed_fields.buffer_size));
    }
    return *storage_view_index;
}

DebugDataRows ROBufferVK::descriptor_debug_data() const {
    DebugDataRows rows(render_data->allocator);
    if (!uniform_view_index.has_value() && !storage_view_index.has_value()) {
        return rows;
    }
    VkDeviceAddress device_address = get_buffer_address_range(render_data, buffer, exposed_fields.buffer_size).address;
    if (uniform_view_index.has_value()) {
        rows.emplace_back(make_descriptor_row("ROBufferVK", "uniform_buffer", *uniform_view_index, device_address));
    }
    if (storage_view_index.has_value()) {
        rows.emplace_back(make_descriptor_row("ROBufferVK", "storage_buffer", *storage_view_index, device_address));
    }
    return rows;
}

void ROBufferVK::update_buffer(const void* p_data, size_t p_dst_offset, size_t p_size) {
    if (exposed_fields.domain != DeviceDomain::HOST && exposed_fields.domain != DeviceDomain::HOST_DEVICE) {
        throw std::runtime_error(
            "Failed to update buffer: the domain of the device must be declared as host to update buffer from host memory.");
    }
    if (p_dst_offset + p_size > exposed_fields.buffer_size) {
        throw std::runtime_error(std::format("Failed to update buffer: trying to map a region ({} to {}) onto a buffer with capacity {}.",
            p_dst_offset,
            p_dst_offset + p_size,
            exposed_fields.buffer_size));
    }
    memcpy(static_cast<uint8_t*>(mapped_memory) + p_dst_offset, p_data, p_size);
    VkResult flush_result = vmaFlushAllocation(render_data->vma_allocator, allocation, p_dst_offset, p_size);
    if (flush_result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to flush Vulkan buffer allocation: {}.", string_VkResult(flush_result)));
    }
}

void ROBufferVK::update_buffer(Ref<ROBufferVK> p_data, size_t p_src_offset, size_t p_dst_offset, size_t p_size) {
    if (p_dst_offset + p_size > exposed_fields.buffer_size) {
        throw std::runtime_error(std::format("Failed to update buffer: trying to map a region ({} to {}) onto a buffer with capacity {}.",
            p_dst_offset,
            p_dst_offset + p_size,
            exposed_fields.buffer_size));
    }
    if (p_src_offset + p_size > p_data->exposed_fields.buffer_size) {
        throw std::runtime_error(std::format("Failed to update buffer: trying to read a region ({} to {}) from a buffer with capacity {}.",
            p_src_offset,
            p_src_offset + p_size,
            p_data->exposed_fields.buffer_size));
    }
    VkBufferCopy copy_region{};
    copy_region.srcOffset = p_src_offset;
    copy_region.dstOffset = p_dst_offset;
    copy_region.size = p_size;
    // Register a renderer-owned GPU job to copy the buffer before graph execution.
    RendererVK::get_singleton()->register_gpu_job(
        [src_buffer = p_data, dst_buffer = this->get_buffer(), copy_region](VkCommandBuffer p_command_buffer) {
            vkCmdCopyBuffer(p_command_buffer, src_buffer->get_buffer(), dst_buffer, 1, &copy_region);
        });
}

void ROBufferVK::update_buffer(const void* p_data, size_t p_size) {
    update_buffer(p_data, 0, p_size);
}

void ROBufferVK::update_buffer(Ref<ROBufferVK> p_data, size_t p_dst_offset, size_t p_size) {
    update_buffer(p_data, 0, p_dst_offset, p_size);
}

void ROBufferVK::update_buffer(Ref<ROBufferVK> p_data, size_t p_size) {
    update_buffer(p_data, 0, 0, p_size);
}

void ROBufferVK::update_buffer(Ref<ROBufferVK> p_data) {
    update_buffer(p_data, 0, 0, p_data->exposed_fields.buffer_size);
}

void ROBufferVK::update_buffer(Ref<ROStagingBufferVK> p_buffer, void* p_data, size_t p_size) { // NOLINT
    size_t src_offset = reinterpret_cast<uintptr_t>(p_data) - reinterpret_cast<uintptr_t>(p_buffer->mapped_memory);
    VkResult flush_result = vmaFlushAllocation(render_data->vma_allocator, p_buffer->allocation, src_offset, p_size);
    if (flush_result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to flush Vulkan staging-buffer allocation: {}.", string_VkResult(flush_result)));
    }
    VkBufferCopy copy_region{};
    copy_region.srcOffset = src_offset;
    copy_region.dstOffset = 0;
    copy_region.size = p_size;
    RendererVK::get_singleton()->register_gpu_job(
        [src_buffer = p_buffer, dst_buffer = this->get_buffer(), copy_region](VkCommandBuffer p_command_buffer) {
            vkCmdCopyBuffer(p_command_buffer, src_buffer->get_buffer(), dst_buffer, 1, &copy_region);
        });
}

VkBufferMemoryBarrier2 ROBufferVK::MemoryBarrierBuilder::build() const {
    VkBufferMemoryBarrier2 result{};
    result.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    result.srcAccessMask = src_access_mask;
    result.srcStageMask = src_stage_mask;
    result.dstAccessMask = dst_access_mask;
    result.dstStageMask = dst_stage_mask;
    result.srcQueueFamilyIndex = src_queue_family_index;
    result.dstQueueFamilyIndex = dst_queue_family_index;
    result.buffer = WBE_REQUIRED(buffer).get_buffer();
    result.offset = offset;
    result.size = size == 0 ? buffer->fields().buffer_size : size;
    return result;
}

} // namespace WhiteBirdEngine
