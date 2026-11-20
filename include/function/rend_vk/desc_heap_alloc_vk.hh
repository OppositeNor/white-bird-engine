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
#ifndef WBE_FILE_DESC_HEAP_ALLOC_VK_HH
#define WBE_FILE_DESC_HEAP_ALLOC_VK_HH

#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_desc_heap_vk.hh"

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @struct DescHeapConfigVK
 * @brief Size and Vulkan bind domain of one physical descriptor heap.
 */
struct DescHeapConfigVK {
    /** Vulkan bind domain. */
    DescHeapTypeVK type = DescHeapTypeVK::RESOURCE;
    /** Bytes available for descriptor arenas. */
    VkDeviceSize usable_size = 0;
    /** Bytes reserved for the implementation after the usable range. */
    VkDeviceSize reserved_size = 0;
    /** Required physical heap address alignment. */
    VkDeviceSize alignment = 0;

    /**
     * @brief Append an aligned arena range and return its byte offset.
     * @throws std::runtime_error If the range cannot be represented.
     */
    VkDeviceSize reserve_arena_bytes(uint32_t p_desc_count, VkDeviceSize p_stride, VkDeviceSize p_alignment);
};

/**
 * @struct DescArenaConfigVK
 * @brief Location and indexing rules for an arena in one descriptor heap.
 */
struct DescArenaConfigVK {
    /** Byte offset from the beginning of the physical heap. */
    VkDeviceSize byte_offset = 0;
    /** Bytes between consecutive descriptors in this arena. */
    VkDeviceSize desc_stride = 0;
    /** Number of descriptors in the arena. */
    uint32_t desc_count = 0;
    /** Required alignment of each descriptor in this arena. */
    VkDeviceSize desc_alignment = 0;
};

/**
 * @class DescHeapAllocVK
 * @brief Owns physical Vulkan descriptor heaps and their logical arenas.
 *
 * Heap IDs identify physical backing buffers; arena IDs identify typed index
 * ranges inside one heap. Callers define layout and descriptor types. The
 * allocator is confined to the rendering thread.
 *
 * This object is neither copyable nor movable.
 */
class DescHeapAllocVK final {
public:
    DescHeapAllocVK() = delete;
    ~DescHeapAllocVK() = default;
    DescHeapAllocVK(const DescHeapAllocVK&) = delete;
    DescHeapAllocVK(DescHeapAllocVK&&) noexcept = delete;
    DescHeapAllocVK& operator=(const DescHeapAllocVK&) = delete;
    DescHeapAllocVK& operator=(DescHeapAllocVK&&) noexcept = delete;

    /**
     * @brief Construct an empty descriptor-heap allocator.
     */
    DescHeapAllocVK(RendAlloc* p_alloc, const RendContextVK* p_rend_data);

    /**
     * @brief Create a physical descriptor heap and return its identifier.
     * @throws std::runtime_error If the configuration is invalid or the backing cannot be created.
     */
    DescHeapIDVK create_heap(const DescHeapConfigVK& p_config);
    /**
     * @brief Reserve an arena inside an existing physical heap.
     * @throws std::runtime_error If the arena overlaps another arena or exceeds the heap.
     */
    DescArenaIDVK allocate_arena(DescHeapIDVK p_heap_id, const DescArenaConfigVK& p_config);
    /**
     * @brief Allocate a contiguous descriptor range and return its shader-visible index.
     */
    uint32_t allocate_desc(DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id, uint32_t p_count = 1);
    /**
     * @brief Release a previously allocated descriptor range.
     */
    void deallocate_desc(
        DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id, uint32_t p_first_index, uint32_t p_count = 1);
    /**
     * @brief Check whether a range belongs to an arena and is currently allocated.
     */
    bool owns_desc(DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id, uint32_t p_first_index, uint32_t p_count = 1) const;

    /**
     * @brief Encode a resource descriptor into an allocated arena slot.
     */
    void write_resource_desc(
        DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id, uint32_t p_index, const VkResourceDescriptorInfoEXT& p_desc) const;
    /**
     * @brief Encode a sampler descriptor into an allocated arena slot.
     */
    void write_sampler_desc(
        DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id, uint32_t p_index, const VkSamplerCreateInfo& p_sampler) const;

    /**
     * @brief Bind one resource heap and one sampler heap to a command buffer.
     */
    void bind_heaps(VkCommandBuffer p_command_buffer, DescHeapIDVK p_resource_heap_id, DescHeapIDVK p_sampler_heap_id) const;

private:
    struct FreeRange {
        uint32_t first_index = 0;
        uint32_t count = 0;
    };

    using RangeVector = Vector<FreeRange, RendAlloc>;

    struct Arena {
        Arena() = delete;
        ~Arena() = default;
        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;
        Arena(Arena&&) noexcept = default;
        Arena& operator=(Arena&&) noexcept = default;
        Arena(RendAlloc* p_alloc, Ref<RODescHeapVK> p_view, VkDeviceSize p_byte_offset, VkDeviceSize p_byte_size);

        Ref<RODescHeapVK> view;
        VkDeviceSize byte_offset = 0;
        VkDeviceSize byte_size = 0;
        RangeVector free_ranges;
    };

    struct Heap {
        Heap() = delete;
        ~Heap() = default;
        Heap(const Heap&) = delete;
        Heap& operator=(const Heap&) = delete;
        Heap(Heap&&) noexcept = default;
        Heap& operator=(Heap&&) noexcept = default;
        Heap(RendAlloc* p_alloc, DescHeapTypeVK p_type, Ref<RODescHeapVK::Backing> p_backing, VkDeviceSize p_usable_size);

        DescHeapTypeVK type = DescHeapTypeVK::RESOURCE;
        Ref<RODescHeapVK::Backing> backing;
        VkDeviceSize usable_size = 0;
        Vector<Arena, RendAlloc> arenas;
    };

    Heap& get_heap(DescHeapIDVK p_heap_id);
    const Heap& get_heap(DescHeapIDVK p_heap_id) const;
    Arena& get_arena(DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id);
    const Arena& get_arena(DescHeapIDVK p_heap_id, DescArenaIDVK p_arena_id) const;
    static bool is_allocated(const Arena& p_arena, uint32_t p_first_index, uint32_t p_count);

    const RendContextVK* rend_data = nullptr;
    RendAlloc* alloc = nullptr;
    Vector<Heap, RendAlloc> heaps;
};

} // namespace WhiteBirdEngine

#endif
