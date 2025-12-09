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
#ifndef __WBE_RO_BUFFER_VK_HH__
#define __WBE_RO_BUFFER_VK_HH__

#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <set>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ROBufferVK
 * @brief Buffer render object.
 */
class WBE_META(WBE_RENDER_OBJECT) ROBufferVK final : public RenderObjectVK {
public:

    /**
     * @class ExposedFields
     * @brief The exposed fields of ro_buffer_vk.
     *
     */
    struct ExposedFields {
        size_t buffer_size;
    };

    /**
     * @class Builder
     * @brief The builder object for ROBufferVK.
     *
     */
    struct Builder {
        friend ROBufferVK;

        // The size of the buffer.
        size_t size = WBE_KiB(4);
        // The usage of the buffer.
        Required<VkBufferUsageFlags> usage;
        // Queue families this buffer is going to be used across. Either this size is 1 or is empty will
        // make the buffer created in exclusive sharing mode, otherwise it will be created in concurrent
        // sharing mode.
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
            }
            else {
                usage = p_usage;
            }
            return *this;
        }

        /**
         * @brief Set the queue families that may use this buffer.
         *
         * @param p_queue_family_indices The queue family indices that might use this buffer.
         * @return *this.
         */
        Builder& set_queue_family_indices(const std::set<uint32_t>& p_queue_family_indices) {
            queue_family_indices = p_queue_family_indices;
            return *this;
        }

        /**
         * @brief Set the queue families that may use this buffer.
         *
         * @param p_queue_family_indices The queue family indices that might use this buffer.
         * @return *this.
         */
        Builder& set_queue_family_indices(std::set<uint32_t>&& p_queue_family_indices) {
            queue_family_indices = std::move(p_queue_family_indices);
            return *this;
        }
    private:
        VkBuffer build(const BaseRenderDataVK* p_render_data) const;
    };

    /**
     * @brief Constructor.
     *
     * @param p_builder The builder instance.
     */
    ROBufferVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);
    virtual ~ROBufferVK() override;

    /**
     * @brief Get the buffer instance.
     *
     * @return The buffer instance.
     */
    VkBuffer get_buffer() const {
        return buffer;
    }

    /**
     * @brief Update the buffer.
     *
     * @param p_data The data to copy from.
     * @param p_dst_offset Offset in the buffer that the beginning of the data would be copied to.
     * @param p_size The size of the data in bytes that would be copied to the buffer.
     */
    void update_buffer(const void* p_data, size_t p_dst_offset, size_t p_size);

    /**
     * @brief Update the buffer.
     *
     * @param p_data The data to be updated to.
     * @param p_size The size of data that will be transmitted.
     */
    void update_buffer(const void* p_data, size_t p_size);

    /**
     * @brief Update the buffer from another buffer object.
     *
     * @param p_data The buffer object to copy data from.
     * @param p_src_offset The offset to copy data from in p_data.
     * @param p_dst_offset The offset to copy data to in the buffer.
     * @param p_size The size to copy (in bytes).
     */
    void update_buffer(Ref<ROBufferVK> p_data, size_t p_src_offset, size_t p_dst_offset, size_t p_size);

    /**
     * @brief Update the buffer from another buffer object.
     *
     * @param p_data The buffer object to copy data from.
     * @param p_dst_offset Offset in the buffer that the beginning of the data would be copied to. This has to be a multiple of 4.
     * @param p_size The size of the data in bytes that would be copied to the buffer. This has to be a multiple of 4.
     */
    void update_buffer(Ref<ROBufferVK> p_data, size_t p_dst_offset, size_t p_size);

    /**
     * @brief Update the buffer from another buffer object.
     *
     * @param p_data The buffer object to copy data from.
     * @param p_size The size of the data in bytes that would be copied to the buffer. This has to be a multiple of 4.
     */
    void update_buffer(Ref<ROBufferVK> p_data, size_t p_size);

    /**
     * @brief Update the buffer from another buffer object.
     *
     * @param p_data The buffer object to copy data from.
     */
    void update_buffer(Ref<ROBufferVK> p_data);

    const ExposedFields& fields() const {
        return exposed_fields;
    }

private:
    void* mapped_memory = nullptr;
    VkBuffer buffer;
    VkDeviceMemory buffer_memory;
    ExposedFields exposed_fields;
};
}

#endif
