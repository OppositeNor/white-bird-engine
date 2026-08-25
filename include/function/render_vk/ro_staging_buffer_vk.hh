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
#ifndef WBE_RO_STAGING_BUFFER_VK_HH
#define WBE_RO_STAGING_BUFFER_VK_HH

#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/vma_vk.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class ROStagingBufferVK final : public RenderObjectVK {
    friend class ROBufferVK;

public:
    /**
     * @class ExposedFields
     * @brief The exposed fields of ROStagingBufferVK..
     *
     */
    struct ExposedFields {
        // The size of the buffer.
        size_t buffer_size = 0;
    };

    /**
     * @class Builder
     * @brief The builder object for ROStagingBufferVK.
     *
     */
    struct Builder {
        friend ROStagingBufferVK;

        // The size of the buffer.
        size_t size = WBE_KI_B(4UL);
        // The usage of the buffer.
        Required<VkBufferUsageFlags> usage;
        // Queue families this buffer is going to be used across. Either this
        // size is 1 or is empty will make the buffer created in exclusive
        // sharing mode, otherwise it will be created in concurrent sharing
        // mode.
        std::set<uint32_t> queue_family_indices;

        /**
         * @brief Set the size of the buffer.
         *
         * @param p_size The size to be set to.
         * @return *this.
         */
        Builder& set_size(size_t p_size) {
            size = p_size;
            return *this;
        }

        /**
         * @brief Set the usage of this buffer.
         *
         * @param p_usage The usage of this buffer.
         * @return *this.
         */
        Builder& set_usage(VkBufferUsageFlags p_usage) {
            usage = p_usage;
            return *this;
        }

        /**
         * @brief Add usage to this buffer.
         *
         * @param p_usage The usage to be added to this buffer.
         * @return *this.
         */
        Builder& add_usage(VkBufferUsageFlags p_usage) {
            if (usage.has_value()) {
                *usage |= p_usage;
            } else {
                usage = p_usage;
            }
            return *this;
        }

        /**
         * @brief Set the queue families that may use this buffer.
         *
         * @param p_queue_family_indices The queue family indices that might use
         * this buffer.
         * @return *this.
         */
        Builder& set_queue_family_indices(const std::set<uint32_t>& p_queue_family_indices) {
            queue_family_indices = p_queue_family_indices;
            return *this;
        }

        /**
         * @brief Set the queue families that may use this buffer.
         *
         * @param p_queue_family_indices The queue family indices that might use
         * this buffer.
         * @return *this.
         */
        Builder& set_queue_family_indices(std::set<uint32_t>&& p_queue_family_indices) {
            queue_family_indices = std::move(p_queue_family_indices);
            return *this;
        }

        Ref<ROStagingBufferVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<ROStagingBufferVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<ROStagingBufferVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<ROStagingBufferVK>(p_render_data->allocator, p_render_data, *this);
        }
    };

    ROStagingBufferVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);
    virtual ~ROStagingBufferVK() override;
    ROStagingBufferVK(const ROStagingBufferVK&) = delete;
    ROStagingBufferVK(ROStagingBufferVK&&) noexcept = delete;
    ROStagingBufferVK& operator=(const ROStagingBufferVK&) = delete;
    ROStagingBufferVK& operator=(ROStagingBufferVK&&) noexcept = delete;

    /**
     * @brief Get the exposed fields.
     *
     * @return The exposed fields.
     */
    const ExposedFields& fields() const {
        return exposed_fields;
    }

    /**
     * @brief Allocate a chunk of memory.
     * Every allocation will be automatically freed after per_frame_in_flight
     * frames.
     */
    void* allocate(size_t p_size);

    /**
     * @brief Get the vulkan buffer instance.
     *
     * @return The buffer instance.
     */
    VkBuffer get_buffer() const {
        return buffer;
    }

private:
    struct DataChunkHeader {
        bool is_allocated;
        uint32_t size;
        uint64_t frame;
    };
    static_assert(sizeof(DataChunkHeader) == 16, "The size of the data chunk header should be 16.");
    static constexpr size_t DATA_CHUNK_HEADER_SIZE = sizeof(DataChunkHeader);
    void* mapped_memory = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    ExposedFields exposed_fields;
};
} // namespace WhiteBirdEngine

#endif
