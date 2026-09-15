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
#include "function/render_vk/descriptor_heap_allocator_vk.hh"

#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_vk_defs.hh"

#include <algorithm>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

static VkDeviceSize checked_multiply(VkDeviceSize p_left, VkDeviceSize p_right) {
    if (p_left != 0 && p_right > std::numeric_limits<VkDeviceSize>::max() / p_left) {
        throw std::runtime_error("Failed to create descriptor heap: heap size overflow.");
    }
    return p_left * p_right;
}

static VkDeviceSize checked_add(VkDeviceSize p_left, VkDeviceSize p_right) {
    if (p_right > std::numeric_limits<VkDeviceSize>::max() - p_left) {
        throw std::runtime_error("Failed to create descriptor heap: heap size overflow.");
    }
    return p_left + p_right;
}

static VkDeviceSize checked_align_up(VkDeviceSize p_value, VkDeviceSize p_alignment) {
    VkDeviceSize remainder = p_value % p_alignment;
    if (remainder == 0) {
        return p_value;
    }
    return checked_add(p_value, p_alignment - remainder);
}

static uint32_t checked_descriptor_index(VkDeviceSize p_offset, VkDeviceSize p_stride) {
    VkDeviceSize index = p_offset / p_stride;
    if (index > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to create descriptor heap allocator: descriptor index exceeds uint32.");
    }
    return static_cast<uint32_t>(index);
}

DescriptorHeapAllocatorVK::DescriptorHeapAllocatorVK(RenderAllocator* p_allocator, const BaseRenderDataVK* p_render_data)
    : render_data(p_render_data), buffer_free_ranges(p_allocator), sampled_image_free_ranges(p_allocator),
      storage_image_free_ranges(p_allocator), sampler_free_ranges(p_allocator) {
    if (p_allocator == nullptr || p_render_data == nullptr || p_render_data->descriptor_heap_properties.bufferDescriptorSize == 0 ||
        p_render_data->descriptor_heap_properties.imageDescriptorSize == 0 || p_render_data->sampler_descriptor_stride == 0) {
        throw std::runtime_error("Failed to create descriptor heap allocator: invalid allocator, render data, or stride.");
    }

    const VkDeviceSize buffer_stride = render_data->descriptor_heap_properties.bufferDescriptorSize;
    const VkDeviceSize image_stride = render_data->descriptor_heap_properties.imageDescriptorSize;
    const VkDeviceSize buffer_region_size = checked_multiply(BUFFER_CAPACITY, buffer_stride);
    const VkDeviceSize image_region_offset = checked_align_up(buffer_region_size, image_stride);
    constexpr uint32_t IMAGE_CAPACITY = SAMPLED_IMAGE_CAPACITY + STORAGE_IMAGE_CAPACITY;
    const VkDeviceSize image_region_size = checked_multiply(IMAGE_CAPACITY, image_stride);
    const VkDeviceSize resource_size = checked_add(image_region_offset, image_region_size);
    VkDeviceSize sampler_size = checked_multiply(SAMPLER_CAPACITY, render_data->sampler_descriptor_stride);
    if (checked_add(resource_size, render_data->descriptor_heap_properties.minResourceHeapReservedRange) >
            render_data->descriptor_heap_properties.maxResourceHeapSize ||
        checked_add(sampler_size, render_data->descriptor_heap_properties.minSamplerHeapReservedRange) >
            render_data->descriptor_heap_properties.maxSamplerHeapSize) {
        throw std::runtime_error("Failed to create descriptor heap allocator: requested heap capacity exceeds device limits.");
    }

    auto resource_backing = make_ref<RODescriptorHeapVK::Backing>(p_allocator,
        render_data,
        resource_size,
        render_data->descriptor_heap_properties.minResourceHeapReservedRange,
        render_data->descriptor_heap_properties.resourceHeapAlignment);
    auto sampler_backing = make_ref<RODescriptorHeapVK::Backing>(p_allocator,
        render_data,
        sampler_size,
        render_data->descriptor_heap_properties.minSamplerHeapReservedRange,
        render_data->descriptor_heap_properties.samplerHeapAlignment);

    const uint32_t sampled_image_first = checked_descriptor_index(image_region_offset, image_stride);
    if (sampled_image_first > std::numeric_limits<uint32_t>::max() - SAMPLED_IMAGE_CAPACITY) {
        throw std::runtime_error("Failed to create descriptor heap allocator: image descriptor index exceeds uint32.");
    }
    const uint32_t storage_image_first = sampled_image_first + SAMPLED_IMAGE_CAPACITY;
    buffer_heap = make_ref<RODescriptorHeapVK>(p_allocator,
        render_data,
        resource_backing,
        DescriptorHeapTypeVK::BUFFER,
        0,
        BUFFER_CAPACITY,
        buffer_stride);
    sampled_image_heap = make_ref<RODescriptorHeapVK>(p_allocator,
        render_data,
        resource_backing,
        DescriptorHeapTypeVK::SAMPLED_IMAGE,
        sampled_image_first,
        SAMPLED_IMAGE_CAPACITY,
        image_stride);
    storage_image_heap = make_ref<RODescriptorHeapVK>(p_allocator,
        render_data,
        resource_backing,
        DescriptorHeapTypeVK::STORAGE_IMAGE,
        storage_image_first,
        STORAGE_IMAGE_CAPACITY,
        image_stride);
    sampler_heap = make_ref<RODescriptorHeapVK>(p_allocator,
        render_data,
        sampler_backing,
        DescriptorHeapTypeVK::SAMPLER,
        0,
        SAMPLER_CAPACITY,
        render_data->sampler_descriptor_stride);

    buffer_free_ranges.push_back({.first_index = 0, .count = BUFFER_CAPACITY});
    sampled_image_free_ranges.push_back({.first_index = sampled_image_first, .count = SAMPLED_IMAGE_CAPACITY});
    storage_image_free_ranges.push_back({.first_index = storage_image_first, .count = STORAGE_IMAGE_CAPACITY});
    sampler_free_ranges.push_back({.first_index = 0, .count = SAMPLER_CAPACITY});
}

uint32_t DescriptorHeapAllocatorVK::allocate(RangeVector& p_free_ranges, uint32_t p_count, const char* p_arena_name) {
    if (p_count == 0) {
        throw std::runtime_error(std::format("Failed to allocate {} descriptor range: count is zero.", p_arena_name));
    }
    for (auto range = p_free_ranges.begin(); range != p_free_ranges.end(); ++range) {
        if (range->count < p_count) {
            continue;
        }
        uint32_t result = range->first_index;
        range->first_index += p_count;
        range->count -= p_count;
        if (range->count == 0) {
            p_free_ranges.erase(range);
        }
        return result;
    }
    throw std::runtime_error(std::format("Failed to allocate {} descriptor range: capacity is exhausted.", p_arena_name));
}

void DescriptorHeapAllocatorVK::deallocate(Ref<RODescriptorHeapVK> p_heap,
    RangeVector& p_free_ranges,
    uint32_t p_first_index,
    uint32_t p_count,
    const char* p_arena_name) {
    if (!p_heap->owns_range(p_first_index, p_count)) {
        throw std::runtime_error(std::format("Failed to release {} descriptor range: range is outside the arena.", p_arena_name));
    }
    uint64_t released_end = static_cast<uint64_t>(p_first_index) + p_count;
    for (const FreeRange& free_range : p_free_ranges) {
        uint64_t free_end = static_cast<uint64_t>(free_range.first_index) + free_range.count;
        if (p_first_index < free_end && free_range.first_index < released_end) {
            throw std::runtime_error(std::format("Failed to release {} descriptor range: range is already free.", p_arena_name));
        }
    }
    for (uint32_t i = 0; i < p_count; ++i) {
        p_heap->clear_descriptor(p_first_index + i);
    }
    auto insert_position = std::lower_bound(p_free_ranges.begin(),
        p_free_ranges.end(),
        p_first_index,
        [](const FreeRange& p_range, uint32_t p_index) { return p_range.first_index < p_index; });
    insert_position = p_free_ranges.insert(insert_position, {.first_index = p_first_index, .count = p_count});
    if (insert_position != p_free_ranges.begin()) {
        auto previous = insert_position - 1;
        if (previous->first_index + previous->count == insert_position->first_index) {
            previous->count += insert_position->count;
            insert_position = p_free_ranges.erase(insert_position) - 1;
        }
    }
    auto next = insert_position + 1;
    if (next != p_free_ranges.end() && insert_position->first_index + insert_position->count == next->first_index) {
        insert_position->count += next->count;
        p_free_ranges.erase(next);
    }
}

uint32_t DescriptorHeapAllocatorVK::allocate_buffer(uint32_t p_count) {
    return allocate(buffer_free_ranges, p_count, "buffer");
}

uint32_t DescriptorHeapAllocatorVK::allocate_sampled_image(uint32_t p_count) {
    return allocate(sampled_image_free_ranges, p_count, "sampled-image");
}

uint32_t DescriptorHeapAllocatorVK::allocate_storage_image(uint32_t p_count) {
    return allocate(storage_image_free_ranges, p_count, "storage-image");
}

uint32_t DescriptorHeapAllocatorVK::allocate_sampler(uint32_t p_count) {
    return allocate(sampler_free_ranges, p_count, "sampler");
}

void DescriptorHeapAllocatorVK::deallocate_buffer(uint32_t p_first_index, uint32_t p_count) {
    deallocate(buffer_heap, buffer_free_ranges, p_first_index, p_count, "buffer");
}

void DescriptorHeapAllocatorVK::deallocate_sampled_image(uint32_t p_first_index, uint32_t p_count) {
    deallocate(sampled_image_heap, sampled_image_free_ranges, p_first_index, p_count, "sampled-image");
}

void DescriptorHeapAllocatorVK::deallocate_storage_image(uint32_t p_first_index, uint32_t p_count) {
    deallocate(storage_image_heap, storage_image_free_ranges, p_first_index, p_count, "storage-image");
}

void DescriptorHeapAllocatorVK::deallocate_sampler(uint32_t p_first_index, uint32_t p_count) {
    deallocate(sampler_heap, sampler_free_ranges, p_first_index, p_count, "sampler");
}

bool DescriptorHeapAllocatorVK::owns_buffer_range(uint32_t p_first_index, uint32_t p_count) const {
    return is_allocated(buffer_heap, buffer_free_ranges, p_first_index, p_count);
}

bool DescriptorHeapAllocatorVK::owns_sampled_image_range(uint32_t p_first_index, uint32_t p_count) const {
    return is_allocated(sampled_image_heap, sampled_image_free_ranges, p_first_index, p_count);
}

bool DescriptorHeapAllocatorVK::owns_storage_image_range(uint32_t p_first_index, uint32_t p_count) const {
    return is_allocated(storage_image_heap, storage_image_free_ranges, p_first_index, p_count);
}

bool DescriptorHeapAllocatorVK::owns_sampler_range(uint32_t p_first_index, uint32_t p_count) const {
    return is_allocated(sampler_heap, sampler_free_ranges, p_first_index, p_count);
}

bool DescriptorHeapAllocatorVK::is_allocated(
    const Ref<RODescriptorHeapVK>& p_heap, const RangeVector& p_free_ranges, uint32_t p_first_index, uint32_t p_count) {
    if (!p_heap->owns_range(p_first_index, p_count)) {
        return false;
    }
    uint64_t requested_end = static_cast<uint64_t>(p_first_index) + p_count;
    for (const FreeRange& free_range : p_free_ranges) {
        uint64_t free_end = static_cast<uint64_t>(free_range.first_index) + free_range.count;
        if (p_first_index < free_end && free_range.first_index < requested_end) {
            return false;
        }
    }
    return true;
}

void DescriptorHeapAllocatorVK::write_buffer_descriptor(
    uint32_t p_index, VkDescriptorType p_type, const VkDeviceAddressRangeEXT& p_address_range) const {
    if (p_type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && p_type != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        throw std::runtime_error("Failed to write buffer descriptor: descriptor type is not a uniform or storage buffer.");
    }
    VkResourceDescriptorInfoEXT descriptor{};
    descriptor.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
    descriptor.type = p_type;
    descriptor.data.pAddressRange = &p_address_range;
    if (!is_allocated(buffer_heap, buffer_free_ranges, p_index, 1)) {
        throw std::runtime_error("Failed to write buffer descriptor: descriptor index is not allocated.");
    }
    buffer_heap->write_resource_descriptor(p_index, descriptor);
}

static VkResourceDescriptorInfoEXT make_image_descriptor(
    VkDescriptorType p_type, const VkImageDescriptorInfoEXT* p_image_info) {
    VkResourceDescriptorInfoEXT descriptor{};
    descriptor.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
    descriptor.type = p_type;
    descriptor.data.pImage = p_image_info;
    return descriptor;
}

static VkImageDescriptorInfoEXT make_image_info(const VkImageViewCreateInfo& p_view, VkImageLayout p_layout) {
    VkImageDescriptorInfoEXT image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT;
    image_info.pView = &p_view;
    image_info.layout = p_layout;
    return image_info;
}

void DescriptorHeapAllocatorVK::write_sampled_image_descriptor(
    uint32_t p_index, const VkImageViewCreateInfo& p_view, VkImageLayout p_layout) const {
    if (!is_allocated(sampled_image_heap, sampled_image_free_ranges, p_index, 1)) {
        throw std::runtime_error("Failed to write sampled-image descriptor: descriptor index is not allocated.");
    }
    VkImageDescriptorInfoEXT image_info = make_image_info(p_view, p_layout);
    VkResourceDescriptorInfoEXT descriptor = make_image_descriptor(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &image_info);
    sampled_image_heap->write_resource_descriptor(p_index, descriptor);
}

void DescriptorHeapAllocatorVK::write_storage_image_descriptor(
    uint32_t p_index, const VkImageViewCreateInfo& p_view, VkImageLayout p_layout) const {
    if (!is_allocated(storage_image_heap, storage_image_free_ranges, p_index, 1)) {
        throw std::runtime_error("Failed to write storage-image descriptor: descriptor index is not allocated.");
    }
    VkImageDescriptorInfoEXT image_info = make_image_info(p_view, p_layout);
    VkResourceDescriptorInfoEXT descriptor = make_image_descriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &image_info);
    storage_image_heap->write_resource_descriptor(p_index, descriptor);
}

void DescriptorHeapAllocatorVK::write_sampler_descriptor(uint32_t p_index, const VkSamplerCreateInfo& p_sampler) const {
    if (!is_allocated(sampler_heap, sampler_free_ranges, p_index, 1)) {
        throw std::runtime_error("Failed to write sampler descriptor: descriptor index is not allocated.");
    }
    sampler_heap->write_sampler_descriptor(p_index, p_sampler);
}

void DescriptorHeapAllocatorVK::bind_heaps(VkCommandBuffer p_command_buffer) const {
    VkBindHeapInfoEXT resource_bind_info = buffer_heap->get_bind_info();
    VkBindHeapInfoEXT sampler_bind_info = sampler_heap->get_bind_info();
    render_data->command_bind_resource_heap(p_command_buffer, &resource_bind_info);
    render_data->command_bind_sampler_heap(p_command_buffer, &sampler_bind_info);
}

} // namespace WhiteBirdEngine
