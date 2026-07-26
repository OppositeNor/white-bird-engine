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
#ifndef WBE_FILE_RO_BUFFER_VK_HH
#define WBE_FILE_RO_BUFFER_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/function_utils.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_staging_buffer_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>
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
        // The size of the buffer.
        size_t buffer_size;
        // The domain where the buffer's memory is located.
        DeviceDomain domain;
    };

    /**
     * @class Builder
     * @brief The builder object for ROBufferVK.
     *
     */
    struct Builder {
        friend ROBufferVK;

        // The size of the buffer.
        size_t size = WBE_KI_B(4UL);
        // The usage of the buffer.
        Required<VkBufferUsageFlags> usage;
        // Queue families this buffer is going to be used across. Either this
        // size is 1 or is empty will make the buffer created in exclusive
        // sharing mode, otherwise it will be created in concurrent sharing
        // mode.
        std::set<uint32_t> queue_family_indices;
        // The domain where this buffer's memory will be allocated.
        Required<DeviceDomain> domain;

        /**
         * @brief The domain where the memory of this buffer will be allocated.
         *
         * @param p_domain The domain.
         * @return *this.
         */
        Builder& set_domain(DeviceDomain p_domain) {
            domain = p_domain;
            return *this;
        }

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

        Ref<ROBufferVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<ROBufferVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<ROBufferVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<ROBufferVK>(p_render_data->allocator, p_render_data, *this);
        }

    private:
        VkBuffer create_vk_object(const BaseRenderDataVK* p_render_data) const;
    };

    /**
     * @class BufferMemoryBarrierBuilder
     * @brief Builder for buffer memory barriers.
     *
     */
    struct MemoryBarrierBuilder {
        friend ROBufferVK;
        // The source access.
        VkAccessFlags2 src_access_mask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        // The source stage.
        VkPipelineStageFlags2 src_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        // The destination access.
        VkAccessFlags2 dst_access_mask = VK_ACCESS_2_MEMORY_READ_BIT;
        // The destination stage.
        VkPipelineStageFlags2 dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        // The source queue family to tranfer ownership. If all the ownerships
        // in ROBufferVK is set in queue_family_indices, this field could be set
        // to VK_QUEUE_FAMILY_IGNORED.
        uint32_t src_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
        // The destination queue family to tranfer ownership. If all the
        // ownerships in ROBufferVK is set in queue_family_indices, this field
        // could be set to VK_QUEUE_FAMILY_IGNORED.
        uint32_t dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
        // The buffer to set barrier on.
        Ref<ROBufferVK> buffer;
        // The offset of the data to apply barrier on in the buffer.
        VkDeviceSize offset = 0;
        // The size of the data to apply barrier on in the buffer. If set to 0,
        // the barrier is going to be applied from offset to the end of barrier.
        VkDeviceSize size = 0;

        /**
         * @brief Set the source access.
         *
         * @param p_src_access_mask The access bit mask.
         * @return *this.
         */
        MemoryBarrierBuilder& set_src_access(VkAccessFlags p_src_access_mask) {
            src_access_mask = p_src_access_mask;
            return *this;
        }

        /**
         * @brief Set the source access.
         *
         * @param p_src_access_mask The access bit mask.
         * @param p_src_stage The source pipeline stage.
         * @return *this.
         */
        MemoryBarrierBuilder& set_src_access(VkAccessFlags p_src_access_mask, VkPipelineStageFlags2 p_src_stage_mask) {
            src_access_mask = p_src_access_mask;
            src_stage_mask = p_src_stage_mask;
            return *this;
        }

        /**
         * @brief Set the destination access.
         *
         * @param p_dst_access_mask The access bit mask.
         * @return *this.
         */
        MemoryBarrierBuilder& set_dst_access(VkAccessFlags p_dst_access_mask) {
            dst_access_mask = p_dst_access_mask;
            return *this;
        }

        /**
         * @brief Set the destination access.
         *
         * @param p_dst_access_mask The access bit mask.
         * @param p_dst_stage_mask The destination pipeline stage.
         * @return *this.
         */
        MemoryBarrierBuilder& set_dst_access(VkAccessFlags p_dst_access_mask, VkPipelineStageFlags2 p_dst_stage_mask) {
            dst_access_mask = p_dst_access_mask;
            dst_stage_mask = p_dst_stage_mask;
            return *this;
        }

        /**
         * @brief Set access.
         *
         * @param p_src_access_mask The source access bit mask.
         * @param p_dst_access_mask The destination access bit mask.
         * @return
         */
        MemoryBarrierBuilder& set_access(VkAccessFlags p_src_access_mask, VkAccessFlags p_dst_access_mask) {
            src_access_mask = p_src_access_mask;
            dst_access_mask = p_dst_access_mask;
            return *this;
        }

        /**
         * @brief Set the buffer to apply the barrier on.
         *
         * @param p_buffer The buffer to apply the barrier on.
         * @return *this.
         */
        MemoryBarrierBuilder& set_buffer(Ref<ROBufferVK> p_buffer) {
            buffer = p_buffer;
            return *this;
        }

        /**
         * @brief Set the offset of the data region in the buffer that this
         * barrier is going to be applied on.
         *
         * @param p_offset The offset.
         * @return *this.
         */
        MemoryBarrierBuilder& set_offset(VkDeviceSize p_offset) {
            offset = p_offset;
            return *this;
        }

        /**
         * @brief Set the size of the data region in the buffer that this
         * barrier is going to be applied on.
         *
         * @param p_size The size. If set to 0, map the region from p_offset to
         * the end of the buffer.
         * @return *this.
         */
        MemoryBarrierBuilder& set_size(VkDeviceSize p_size) {
            size = p_size;
            return *this;
        }

        /**
         * @brief Set the data region in the buffer that this barrier is going
         * to be applied on.
         *
         * @param p_offset The offset.
         * @param p_size The size. If set to 0, map the region from p_offset to
         * the end of the buffer.
         * @return *this.
         */
        MemoryBarrierBuilder& set_data_region(VkDeviceSize p_offset, VkDeviceSize p_size) {
            offset = p_offset;
            size = p_size;
            return *this;
        }

        /**
         * @brief Build the barrier.
         *
         * @return The barrier instance.
         */
        VkBufferMemoryBarrier2 build() const;
    };

    /**
     * @brief Constructor.
     *
     * @param p_builder The builder instance.
     */
    ROBufferVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);
    virtual ~ROBufferVK() override;
    ROBufferVK(const ROBufferVK&) = delete;
    ROBufferVK(ROBufferVK&&) noexcept = delete;
    ROBufferVK& operator=(const ROBufferVK&) = delete;
    ROBufferVK& operator=(ROBufferVK&&) noexcept = delete;

    /**
     * @brief Get the buffer instance.
     *
     * @return The buffer instance.
     */
    VkBuffer get_buffer() const {
        return buffer;
    }

    /**
     * @brief Update the buffer. Updating from main memory must specify the
     * device domain as host.
     *
     * @param p_data The data to copy from.
     * @param p_dst_offset Offset in the buffer that the beginning of the data
     * would be copied to.
     * @param p_size The size of the data in bytes that would be copied to the
     * buffer.
     */
    void update_buffer(const void* p_data, size_t p_dst_offset, size_t p_size);

    /**
     * @brief Update the buffer. Updating from main memory must specify the
     * device domain as host.
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
     * @param p_dst_offset Offset in the buffer that the beginning of the data
     * would be copied to. This has to be a multiple of 4.
     * @param p_size The size of the data in bytes that would be copied to the
     * buffer. This has to be a multiple of 4.
     */
    void update_buffer(Ref<ROBufferVK> p_data, size_t p_dst_offset, size_t p_size);

    /**
     * @brief Update the buffer from another buffer object.
     *
     * @param p_data The buffer object to copy data from.
     * @param p_size The size of the data in bytes that would be copied to the
     * buffer. This has to be a multiple of 4.
     */
    void update_buffer(Ref<ROBufferVK> p_data, size_t p_size);

    /**
     * @brief Update the buffer from another buffer object.
     *
     * @param p_data The buffer object to copy data from.
     */
    void update_buffer(Ref<ROBufferVK> p_data);

    /**
     * @brief Update the buffer from a staging buffer.
     *
     * @param p_buffer The staging buffer to retrieve data.
     * @param p_data The data to fill in.
     */
    void update_buffer(Ref<ROStagingBufferVK> p_buffer, void* p_data, size_t p_size);

    const ExposedFields& fields() const {
        return exposed_fields;
    }

private:
    void* mapped_memory = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
    ExposedFields exposed_fields{};
};
} // namespace WhiteBirdEngine

#endif
