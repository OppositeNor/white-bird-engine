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
#ifndef WBE_FILE_RO_DESC_HEAP_VK_HH
#define WBE_FILE_RO_DESC_HEAP_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/i_rend_object.hh"
#include "core/core_utils.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/vma_vk.hh"
#include "utils/utils.hh"

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class DescHeapAllocVK;

/**
 * @enum DescHeapTypeVK
 * @brief Logical descriptor arena represented by a descriptor heap object.
 */
enum class DescHeapTypeVK : uint8_t {
    RESOURCE,
    SAMPLER,
};

/**
 * @class RODescHeapVK
 * @brief Logical descriptor arena backed by a Vulkan descriptor heap buffer.
 *
 * Resource arenas may share one physical backing buffer. An arena's indices are
 * offsets in the descriptor-type-specific SPIR-V view of that backing buffer.
 *
 * This object is neither copyable nor movable.
 */
class WBE_META(WBE_REND_OBJECT) RODescHeapVK final : public IRendObject {
public:
    /**
     * @class Backing
     * @brief Shared physical storage used by logical descriptor arenas.
     */
    class Backing final {
    public:
        Backing() = delete;
        ~Backing();
        Backing(const Backing&) = delete;
        Backing(Backing&&) noexcept = delete;
        Backing& operator=(const Backing&) = delete;
        Backing& operator=(Backing&&) noexcept = delete;

        /**
         * @brief Create aligned host-visible and device-local heap storage.
         */
        Backing(const RendContextVK* p_rend_data, VkDeviceSize p_usable_size, VkDeviceSize p_reserved_size, VkDeviceSize p_alignment);

        /**
         * @brief Get the physical range used when binding this heap.
         */
        VkBindHeapInfoEXT get_bind_info() const;

    private:
        friend class RODescHeapVK;
        const RendContextVK* rend_data = nullptr;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        void* mapped_memory = nullptr;
        VkDeviceAddress device_address = 0;
        VkDeviceSize usable_size = 0;
        VkDeviceSize reserved_size = 0;
    };

    RODescHeapVK() = delete;
    virtual ~RODescHeapVK() override;
    RODescHeapVK(const RODescHeapVK&) = delete;
    RODescHeapVK(RODescHeapVK&&) noexcept = delete;
    RODescHeapVK& operator=(const RODescHeapVK&) = delete;
    RODescHeapVK& operator=(RODescHeapVK&&) noexcept = delete;

    /**
     * @brief Construct a logical arena over descriptor-heap backing storage.
     */
    RODescHeapVK(const RendContextVK* p_rend_data,
        Ref<Backing> p_backing,
        DescHeapTypeVK p_type,
        uint32_t p_first_index,
        uint32_t p_capacity,
        VkDeviceSize p_stride);

    virtual TypeID get_type_uuid() const override;

    /**
     * @brief Get the logical arena type.
     */
    DescHeapTypeVK get_type() const {
        return type;
    }

    /**
     * @brief Get the first descriptor index owned by this arena's typed heap view.
     */
    uint32_t get_first_index() const {
        return first_index;
    }

    /**
     * @brief Get the number of descriptors in this arena.
     */
    uint32_t get_capacity() const {
        return capacity;
    }

    /**
     * @brief Get the byte stride between descriptors in this arena.
     */
    VkDeviceSize get_stride() const {
        return stride;
    }

    /**
     * @brief Check whether a descriptor-index range belongs to this arena's typed heap view.
     */
    bool owns_range(uint32_t p_first_index, uint32_t p_count = 1) const;

private:
    friend class DescHeapAllocVK;

    void write_resource_desc(uint32_t p_index, const VkResourceDescriptorInfoEXT& p_desc) const;
    void write_sampler_desc(uint32_t p_index, const VkSamplerCreateInfo& p_sampler) const;
    void clear_desc(uint32_t p_index) const;
    VkBindHeapInfoEXT get_bind_info() const;

    const RendContextVK* rend_data = nullptr;
    Ref<Backing> backing;
    DescHeapTypeVK type = DescHeapTypeVK::RESOURCE;
    uint32_t first_index = 0;
    uint32_t capacity = 0;
    VkDeviceSize stride = 0;
};

} // namespace WhiteBirdEngine

#endif
