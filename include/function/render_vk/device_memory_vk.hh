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
#ifndef WBE_FILE_DEVICE_MEMORY_VK_HH
#define WBE_FILE_DEVICE_MEMORY_VK_HH

#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/function_utils.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/vma_vk.hh"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class DeviceMemoryVK
 * @brief One VMA-managed Vulkan device-memory allocation that buffers and images bind to.
 *
 * The resource that the allocation was made for binds it through get(). Further resources may bind the same memory as aliases
 * through get_alias(); aliasing resources share the memory contents, so callers must synchronize their use of it. Buffers and
 * images retain a Ref to the memory they are bound to until their own delayed destruction, so the allocation outlives every
 * bound resource.
 *
 * The allocation is freed immediately when the last Ref is released, which must only happen after the GPU no longer uses the
 * memory. The object is neither copyable nor movable and is confined to the rendering thread.
 */
class DeviceMemoryVK final {
public:
    /**
     * @class Builder
     * @brief Configuration of a device-memory allocation.
     *
     * Exactly one requirement source is used: a buffer, an image, or explicit memory requirements. Buffer and image sources let
     * VMA select memory for their usage and apply dedicated-allocation requirements.
     */
    struct Builder {
        /** Buffer whose memory requirements the allocation satisfies. */
        VkBuffer buffer = VK_NULL_HANDLE;
        /** Image whose memory requirements the allocation satisfies. */
        VkImage image = VK_NULL_HANDLE;
        /** Explicit memory requirements, typically covering several aliasing resources. */
        Required<VkMemoryRequirements> requirements;
        /** Domain where the memory is allocated. Host-visible domains are persistently mapped. */
        Required<DeviceDomain> domain;

        /**
         * @brief Allocate memory for a buffer's requirements.
         * @param p_buffer The buffer; it is not bound by the allocation.
         * @return *this.
         */
        Builder& set_buffer_requirements(VkBuffer p_buffer);

        /**
         * @brief Allocate memory for an image's requirements.
         * @param p_image The image; it is not bound by the allocation.
         * @return *this.
         */
        Builder& set_image_requirements(VkImage p_image);

        /**
         * @brief Allocate memory for explicit requirements.
         * @param p_requirements Size, alignment, and allowed memory types.
         * @return *this.
         */
        Builder& set_requirements(const VkMemoryRequirements& p_requirements);

        /**
         * @brief Set the memory domain.
         * @param p_domain The domain.
         * @return *this.
         */
        Builder& set_domain(DeviceDomain p_domain);

        /**
         * @brief Allocate the memory.
         * @param p_render_data The base render data.
         * @return The shared memory handle.
         * @throws std::runtime_error If the configuration is incomplete or the allocation fails.
         */
        Ref<DeviceMemoryVK> create_ref(const BaseRenderDataVK* p_render_data) const;
    };

    /**
     * @class ExposedFields
     * @brief Read-only properties of the allocation.
     */
    struct ExposedFields {
        /** Vulkan memory block containing the allocation; owned by VMA and never freed directly. */
        VkDeviceMemory device_memory = VK_NULL_HANDLE;
        /** Offset of the allocation inside device_memory. */
        VkDeviceSize offset = 0;
        /** Size of the allocation in bytes. */
        VkDeviceSize size = 0;
        /** Vulkan memory type of the allocation. */
        uint32_t memory_type_index = 0;
        /** Persistent host mapping of the allocation start, or null for device-only memory. */
        void* mapped_data = nullptr;
        /** Domain where the memory is allocated. */
        DeviceDomain domain = DeviceDomain::DEVICE;
    };

    DeviceMemoryVK() = delete;

    /**
     * @brief Allocate memory.
     * @param p_render_data The base render data.
     * @param p_builder The allocation configuration.
     * @throws std::runtime_error If the configuration is incomplete or the allocation fails.
     */
    DeviceMemoryVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);
    ~DeviceMemoryVK();
    DeviceMemoryVK(const DeviceMemoryVK&) = delete;
    DeviceMemoryVK(DeviceMemoryVK&&) noexcept = delete;
    DeviceMemoryVK& operator=(const DeviceMemoryVK&) = delete;
    DeviceMemoryVK& operator=(DeviceMemoryVK&&) noexcept = delete;

    /**
     * @brief Get the allocation for binding the resource it was made for.
     * @return The VMA allocation.
     */
    VmaAllocation get() const {
        return allocation;
    }

    /**
     * @brief Acquire the allocation for binding an aliasing resource.
     *
     * Each call records one more alias of the memory.
     *
     * @return The VMA allocation.
     */
    VmaAllocation get_alias();

    /**
     * @brief Get the number of aliases acquired through get_alias().
     * @return The alias count.
     */
    uint32_t get_alias_count() const {
        return alias_count;
    }

    /**
     * @brief Check whether a resource with the given requirements can be bound at an offset of this memory.
     * @param p_requirements The resource's memory requirements.
     * @param p_offset Offset inside the allocation.
     * @return True when the memory type, alignment, and size are compatible.
     */
    bool is_compatible(const VkMemoryRequirements& p_requirements, VkDeviceSize p_offset) const;

    /**
     * @brief Flush host writes to a range of a host-visible allocation.
     * @param p_offset Offset inside the allocation.
     * @param p_size Size of the range in bytes.
     * @throws std::runtime_error If flushing fails.
     */
    void flush(VkDeviceSize p_offset, VkDeviceSize p_size) const;

    /**
     * @brief Get the exposed fields.
     * @return The exposed fields.
     */
    const ExposedFields& fields() const {
        return exposed_fields;
    }

private:
    const BaseRenderDataVK* render_data;
    VmaAllocation allocation = VK_NULL_HANDLE;
    ExposedFields exposed_fields{};
    uint32_t alias_count = 0;
};

} // namespace WhiteBirdEngine

#endif
