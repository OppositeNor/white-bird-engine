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
#ifndef WBE_FILE_DESCRIPTOR_HEAP_ALLOCATOR_VK_HH
#define WBE_FILE_DESCRIPTOR_HEAP_ALLOCATOR_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/ro_descriptor_heap_vk.hh"

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class DescriptorHeapAllocatorVK
 * @brief Owns Vulkan descriptor heaps and allocates their logical index ranges.
 *
 * Resource descriptors use one physical heap split into buffer, sampled-image,
 * and storage-image arenas. Samplers use a separate physical heap. Like render
 * objects, this allocator is confined to the rendering thread.
 *
 * This object is neither copyable nor movable.
 */
class DescriptorHeapAllocatorVK final {
public:
    /**
     * @brief Number of buffer descriptors available to the renderer.
     */
    static constexpr uint32_t BUFFER_CAPACITY = 272;
    /**
     * @brief Number of sampled-image descriptors available to the renderer.
     */
    static constexpr uint32_t SAMPLED_IMAGE_CAPACITY = 256;
    /**
     * @brief Number of storage-image descriptors available to the renderer.
     */
    static constexpr uint32_t STORAGE_IMAGE_CAPACITY = 64;
    /**
     * @brief Number of sampler descriptors available to the renderer.
     */
    static constexpr uint32_t SAMPLER_CAPACITY = 256;

    DescriptorHeapAllocatorVK() = delete;
    ~DescriptorHeapAllocatorVK() = default;
    DescriptorHeapAllocatorVK(const DescriptorHeapAllocatorVK&) = delete;
    DescriptorHeapAllocatorVK(DescriptorHeapAllocatorVK&&) noexcept = delete;
    DescriptorHeapAllocatorVK& operator=(const DescriptorHeapAllocatorVK&) = delete;
    DescriptorHeapAllocatorVK& operator=(DescriptorHeapAllocatorVK&&) noexcept = delete;

    /**
     * @brief Construct the renderer-owned descriptor heaps.
     * @throws std::runtime_error If heap storage cannot be created.
     */
    DescriptorHeapAllocatorVK(RenderAllocator* p_allocator, const BaseRenderDataVK* p_render_data);

    /**
     * @brief Allocate a contiguous buffer-descriptor range.
     */
    uint32_t allocate_buffer(uint32_t p_count = 1);
    /**
     * @brief Allocate a contiguous sampled-image-descriptor range.
     */
    uint32_t allocate_sampled_image(uint32_t p_count = 1);
    /**
     * @brief Allocate a contiguous storage-image-descriptor range.
     */
    uint32_t allocate_storage_image(uint32_t p_count = 1);
    /**
     * @brief Allocate a contiguous sampler-descriptor range.
     */
    uint32_t allocate_sampler(uint32_t p_count = 1);

    /**
     * @brief Release a buffer-descriptor range.
     */
    void deallocate_buffer(uint32_t p_first_index, uint32_t p_count = 1);
    /**
     * @brief Release a sampled-image-descriptor range.
     */
    void deallocate_sampled_image(uint32_t p_first_index, uint32_t p_count = 1);
    /**
     * @brief Release a storage-image-descriptor range.
     */
    void deallocate_storage_image(uint32_t p_first_index, uint32_t p_count = 1);
    /**
     * @brief Release a sampler-descriptor range.
     */
    void deallocate_sampler(uint32_t p_first_index, uint32_t p_count = 1);

    /**
     * @brief Check whether a range is currently allocated in the buffer arena.
     */
    bool owns_buffer_range(uint32_t p_first_index, uint32_t p_count = 1) const;
    /**
     * @brief Check whether a range is currently allocated in the sampled-image arena.
     */
    bool owns_sampled_image_range(uint32_t p_first_index, uint32_t p_count = 1) const;
    /**
     * @brief Check whether a range is currently allocated in the storage-image arena.
     */
    bool owns_storage_image_range(uint32_t p_first_index, uint32_t p_count = 1) const;
    /**
     * @brief Check whether a range is currently allocated in the sampler arena.
     */
    bool owns_sampler_range(uint32_t p_first_index, uint32_t p_count = 1) const;

    /**
     * @brief Write a uniform or storage-buffer descriptor.
     * @throws std::runtime_error If the type or index is invalid.
     */
    void write_buffer_descriptor(
        uint32_t p_index, VkDescriptorType p_type, const VkDeviceAddressRangeEXT& p_address_range) const;
    /**
     * @brief Write a sampled-image descriptor.
     */
    void write_sampled_image_descriptor(
        uint32_t p_index, const VkImageViewCreateInfo& p_view, VkImageLayout p_layout) const;
    /**
     * @brief Write a storage-image descriptor.
     */
    void write_storage_image_descriptor(
        uint32_t p_index, const VkImageViewCreateInfo& p_view, VkImageLayout p_layout) const;
    /**
     * @brief Write a sampler descriptor.
     */
    void write_sampler_descriptor(uint32_t p_index, const VkSamplerCreateInfo& p_sampler) const;

    /**
     * @brief Bind the renderer's resource and sampler heaps to a command buffer.
     */
    void bind_heaps(VkCommandBuffer p_command_buffer) const;

    /**
     * @brief Get the buffer descriptor arena.
     */
    const Ref<RODescriptorHeapVK>& get_buffer_heap() const {
        return buffer_heap;
    }
    /**
     * @brief Get the sampled-image descriptor arena.
     */
    const Ref<RODescriptorHeapVK>& get_sampled_image_heap() const {
        return sampled_image_heap;
    }
    /**
     * @brief Get the storage-image descriptor arena.
     */
    const Ref<RODescriptorHeapVK>& get_storage_image_heap() const {
        return storage_image_heap;
    }
    /**
     * @brief Get the sampler descriptor arena.
     */
    const Ref<RODescriptorHeapVK>& get_sampler_heap() const {
        return sampler_heap;
    }

private:
    struct FreeRange {
        uint32_t first_index = 0;
        uint32_t count = 0;
    };

    using RangeVector = Vector<FreeRange, RenderAllocator>;

    uint32_t allocate(RangeVector& p_free_ranges, uint32_t p_count, const char* p_arena_name);
    void deallocate(Ref<RODescriptorHeapVK> p_heap,
        RangeVector& p_free_ranges,
        uint32_t p_first_index,
        uint32_t p_count,
        const char* p_arena_name);
    static bool is_allocated(
        const Ref<RODescriptorHeapVK>& p_heap, const RangeVector& p_free_ranges, uint32_t p_first_index, uint32_t p_count);

    const BaseRenderDataVK* render_data = nullptr;
    Ref<RODescriptorHeapVK> buffer_heap;
    Ref<RODescriptorHeapVK> sampled_image_heap;
    Ref<RODescriptorHeapVK> storage_image_heap;
    Ref<RODescriptorHeapVK> sampler_heap;
    RangeVector buffer_free_ranges;
    RangeVector sampled_image_free_ranges;
    RangeVector storage_image_free_ranges;
    RangeVector sampler_free_ranges;
};

} // namespace WhiteBirdEngine

#endif
