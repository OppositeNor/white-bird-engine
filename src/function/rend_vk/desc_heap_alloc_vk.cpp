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
#include "function/rend_vk/desc_heap_alloc_vk.hh"

#include "core/memory/reference_strong.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_desc_heap_vk.hh"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

VkDeviceSize checked_add(VkDeviceSize p_left, VkDeviceSize p_right) {
    if (p_right > std::numeric_limits<VkDeviceSize>::max() - p_left) {
        throw std::runtime_error("Failed to create descriptor heap: byte offset overflow.");
    }
    return p_left + p_right;
}

VkDeviceSize checked_multiply(VkDeviceSize p_left, VkDeviceSize p_right) {
    if (p_left != 0 && p_right > std::numeric_limits<VkDeviceSize>::max() / p_left) {
        throw std::runtime_error("Failed to create descriptor arena: byte size overflow.");
    }
    return p_left * p_right;
}

VkDeviceSize checked_align_up(VkDeviceSize p_value, VkDeviceSize p_alignment) {
    const VkDeviceSize remainder = p_value % p_alignment;
    return remainder == 0 ? p_value : checked_add(p_value, p_alignment - remainder);
}

} // namespace

VkDeviceSize DescHeapConfigVK::reserve_arena_bytes(uint32_t p_desc_count, VkDeviceSize p_stride, VkDeviceSize p_alignment) {
    if (p_desc_count == 0 || p_stride == 0 || p_alignment == 0 || p_stride % p_alignment != 0) {
        throw std::runtime_error("Failed to reserve descriptor arena: invalid count, stride, or alignment.");
    }
    const VkDeviceSize byte_offset = checked_align_up(usable_size, p_stride);
    usable_size = checked_add(byte_offset, checked_multiply(p_desc_count, p_stride));
    return byte_offset;
}

DescHeapAllocVK::Arena::Arena(
    RendAlloc* p_alloc, Ref<RODescHeapVK> p_view, VkDeviceSize p_byte_offset, VkDeviceSize p_byte_size)
    : view(std::move(p_view)), byte_offset(p_byte_offset), byte_size(p_byte_size), free_ranges(p_alloc) {
    free_ranges.push_back({.first_index = view->get_first_index(), .count = view->get_capacity()});
}

DescHeapAllocVK::Heap::Heap(
    RendAlloc* p_alloc, DescHeapTypeVK p_type, Ref<RODescHeapVK::Backing> p_backing, VkDeviceSize p_usable_size)
    : type(p_type), backing(std::move(p_backing)), usable_size(p_usable_size), arenas(p_alloc) {
}

DescHeapAllocVK::DescHeapAllocVK(RendAlloc* p_alloc, const RendContextVK* p_rend_data)
    : rend_data(p_rend_data), alloc(p_alloc), heaps(p_alloc) {
    if (p_alloc == nullptr || p_rend_data == nullptr) {
        throw std::runtime_error("Failed to create descriptor heap allocator: allocator or render data is missing.");
    }
}

DescHeapIDVK DescHeapAllocVK::create_heap(const DescHeapConfigVK& p_config) {
    if (p_config.usable_size == 0 || p_config.alignment == 0) {
        throw std::runtime_error("Failed to create descriptor heap: size or alignment is zero.");
    }
    const VkDeviceSize max_heap_size = p_config.type == DescHeapTypeVK::RESOURCE
                                           ? rend_data->desc_heap_properties.maxResourceHeapSize
                                           : rend_data->desc_heap_properties.maxSamplerHeapSize;
    if (checked_add(p_config.usable_size, p_config.reserved_size) > max_heap_size) {
        throw std::runtime_error("Failed to create descriptor heap: requested size exceeds the device limit.");
    }
    if (heaps.size() >= std::numeric_limits<DescHeapIDVK>::max()) {
        throw std::runtime_error("Failed to create descriptor heap: heap identifier exceeds uint32.");
    }
    auto backing = make_ref<RODescHeapVK::Backing>(
        alloc, rend_data, p_config.usable_size, p_config.reserved_size, p_config.alignment);
    DescHeapIDVK heap_id = static_cast<DescHeapIDVK>(heaps.size());
    heaps.emplace_back(alloc, p_config.type, std::move(backing), p_config.usable_size);
    return heap_id;
}

DescArenaIDVK DescHeapAllocVK::allocate_arena(
    DescHeapIDVK p_heap_id, const DescArenaConfigVK& p_config) {
    Heap& heap = get_heap(p_heap_id);
    if (p_config.desc_stride == 0 || p_config.desc_count == 0 ||
        p_config.desc_alignment == 0 || p_config.byte_offset % p_config.desc_stride != 0 ||
        p_config.byte_offset % p_config.desc_alignment != 0 ||
        p_config.desc_stride % p_config.desc_alignment != 0) {
        throw std::runtime_error("Failed to create descriptor arena: invalid stride, alignment, or count.");
    }
    const VkDeviceSize byte_size = checked_multiply(p_config.desc_count, p_config.desc_stride);
    const VkDeviceSize byte_end = checked_add(p_config.byte_offset, byte_size);
    if (byte_end > heap.usable_size) {
        throw std::runtime_error("Failed to create descriptor arena: range exceeds the physical heap.");
    }
    for (const Arena& arena : heap.arenas) {
        if (p_config.byte_offset < checked_add(arena.byte_offset, arena.byte_size) && arena.byte_offset < byte_end) {
            throw std::runtime_error("Failed to create descriptor arena: range overlaps another arena.");
        }
    }
    const VkDeviceSize first_index = p_config.byte_offset / p_config.desc_stride;
    if (first_index > std::numeric_limits<uint32_t>::max() - p_config.desc_count ||
        heap.arenas.size() >= std::numeric_limits<DescArenaIDVK>::max()) {
        throw std::runtime_error("Failed to create descriptor arena: index exceeds uint32.");
    }
    auto view = make_ref<RODescHeapVK>(alloc,
        rend_data,
        heap.backing,
        heap.type,
        static_cast<uint32_t>(first_index),
        p_config.desc_count,
        p_config.desc_stride);
    DescArenaIDVK arena_id = static_cast<DescArenaIDVK>(heap.arenas.size());
    heap.arenas.emplace_back(alloc, std::move(view), p_config.byte_offset, byte_size);
    return arena_id;
}

DescHeapAllocVK::Heap& DescHeapAllocVK::get_heap(DescHeapIDVK p_heap_id) {
    if (p_heap_id >= heaps.size()) {
        throw std::runtime_error("Descriptor heap identifier is invalid.");
    }
    return heaps[p_heap_id];
}

const DescHeapAllocVK::Heap& DescHeapAllocVK::get_heap(DescHeapIDVK p_heap_id) const {
    if (p_heap_id >= heaps.size()) {
        throw std::runtime_error("Descriptor heap identifier is invalid.");
    }
    return heaps[p_heap_id];
}

DescHeapAllocVK::Arena& DescHeapAllocVK::get_arena(DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id) {
    Heap& heap = get_heap(p_heap_id);
    if (p_arena_id >= heap.arenas.size()) {
        throw std::runtime_error("Descriptor arena identifier is invalid.");
    }
    return heap.arenas[p_arena_id];
}

const DescHeapAllocVK::Arena& DescHeapAllocVK::get_arena(
    DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id) const {
    const Heap& heap = get_heap(p_heap_id);
    if (p_arena_id >= heap.arenas.size()) {
        throw std::runtime_error("Descriptor arena identifier is invalid.");
    }
    return heap.arenas[p_arena_id];
}

uint32_t DescHeapAllocVK::allocate_desc(DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id, uint32_t p_count) {
    Arena& arena = get_arena(p_heap_id, p_arena_id);
    if (p_count == 0) {
        throw std::runtime_error("Failed to allocate descriptor range: count is zero.");
    }
    for (auto range = arena.free_ranges.begin(); range != arena.free_ranges.end(); ++range) {
        if (range->count < p_count) {
            continue;
        }
        uint32_t result = range->first_index;
        range->first_index += p_count;
        range->count -= p_count;
        if (range->count == 0) {
            arena.free_ranges.erase(range);
        }
        return result;
    }
    throw std::runtime_error("Failed to allocate descriptor range: arena capacity is exhausted.");
}

bool DescHeapAllocVK::is_allocated(const Arena& p_arena, uint32_t p_first_index, uint32_t p_count) {
    if (!p_arena.view->owns_range(p_first_index, p_count)) {
        return false;
    }
    const uint64_t requested_end = static_cast<uint64_t>(p_first_index) + p_count;
    for (const FreeRange& free_range : p_arena.free_ranges) {
        const uint64_t free_end = static_cast<uint64_t>(free_range.first_index) + free_range.count;
        if (p_first_index < free_end && free_range.first_index < requested_end) {
            return false;
        }
    }
    return true;
}

bool DescHeapAllocVK::owns_desc(
    DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id, uint32_t p_first_index, uint32_t p_count) const {
    return is_allocated(get_arena(p_heap_id, p_arena_id), p_first_index, p_count);
}

void DescHeapAllocVK::deallocate_desc(
    DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id, uint32_t p_first_index, uint32_t p_count) {
    Arena& arena = get_arena(p_heap_id, p_arena_id);
    if (!is_allocated(arena, p_first_index, p_count)) {
        throw std::runtime_error("Failed to release descriptor range: range is not allocated in this arena.");
    }
    for (uint32_t i = 0; i < p_count; ++i) {
        arena.view->clear_desc(p_first_index + i);
    }
    auto insert_position = std::lower_bound(
        arena.free_ranges.begin(), arena.free_ranges.end(), p_first_index, [](const FreeRange& p_range, uint32_t p_index) {
            return p_range.first_index < p_index;
        });
    insert_position = arena.free_ranges.insert(insert_position, {.first_index = p_first_index, .count = p_count});
    if (insert_position != arena.free_ranges.begin()) {
        auto previous = insert_position - 1;
        if (previous->first_index + previous->count == insert_position->first_index) {
            previous->count += insert_position->count;
            insert_position = arena.free_ranges.erase(insert_position) - 1;
        }
    }
    auto next = insert_position + 1;
    if (next != arena.free_ranges.end() && insert_position->first_index + insert_position->count == next->first_index) {
        insert_position->count += next->count;
        arena.free_ranges.erase(next);
    }
}

void DescHeapAllocVK::write_resource_desc(
    DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id, uint32_t p_index, const VkResourceDescriptorInfoEXT& p_desc) const {
    const Heap& heap = get_heap(p_heap_id);
    const Arena& arena = get_arena(p_heap_id, p_arena_id);
    if (heap.type != DescHeapTypeVK::RESOURCE || !is_allocated(arena, p_index, 1)) {
        throw std::runtime_error("Failed to write resource descriptor: arena or descriptor index is invalid.");
    }
    arena.view->write_resource_desc(p_index, p_desc);
}

void DescHeapAllocVK::write_sampler_desc(
    DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id, uint32_t p_index, const VkSamplerCreateInfo& p_sampler) const {
    const Heap& heap = get_heap(p_heap_id);
    const Arena& arena = get_arena(p_heap_id, p_arena_id);
    if (heap.type != DescHeapTypeVK::SAMPLER || !is_allocated(arena, p_index, 1)) {
        throw std::runtime_error("Failed to write sampler descriptor: arena or descriptor index is invalid.");
    }
    arena.view->write_sampler_desc(p_index, p_sampler);
}

void DescHeapAllocVK::bind_heaps(
    VkCommandBuffer p_command_buffer, DescHeapIDVK p_resource_heap_id, DescHeapIDVK p_sampler_heap_id) const {
    const Heap& resource_heap = get_heap(p_resource_heap_id);
    const Heap& sampler_heap = get_heap(p_sampler_heap_id);
    if (resource_heap.type != DescHeapTypeVK::RESOURCE || sampler_heap.type != DescHeapTypeVK::SAMPLER) {
        throw std::runtime_error("Failed to bind descriptor heaps: heap types are invalid.");
    }
    VkBindHeapInfoEXT resource_bind_info = resource_heap.backing->get_bind_info();
    VkBindHeapInfoEXT sampler_bind_info = sampler_heap.backing->get_bind_info();
    rend_data->command_bind_resource_heap(p_command_buffer, &resource_bind_info);
    rend_data->command_bind_sampler_heap(p_command_buffer, &sampler_bind_info);
}

} // namespace WhiteBirdEngine
