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
#ifndef WBE_FILE_RO_IMAGE_VK_HH
#define WBE_FILE_RO_IMAGE_VK_HH

#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <glm/glm.hpp>
#include <set>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ROImageVK
 * @brief Image.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) ROImageVK final : public RenderObjectVK, ThisRef<ROImageVK> {
    friend struct MemoryBarrierBuilder;
public:
    // Builder for ROImageVK
    struct Builder {
        friend ROImageVK;
        // Image type.
        Required<VkImageType> image_type;
        // Image format.
        Required<VkFormat> format;
        // Image extent.
        Required<VkExtent3D> extent;
        // Mipmap level.
        uint32_t mipmap_level = 1;
        // Number of layers.
        uint32_t layers_count = 1;
        // Image usages.
        Required<VkImageUsageFlags> usage;
        // Queue family indices. Either this size is 1 or is empty will make the imagae
        // created in exclusive sharing mode, otherwise it will be created in concurrent
        // sharing mode.
        std::set<uint32_t> queue_family_indices;
        // The initial image layout. This could be changed by setting image barriers.
        VkImageLayout initial_image_layout = VK_IMAGE_LAYOUT_GENERAL;

        // Set the image type.
        Builder& set_image_type(VkImageType p_image_type);
        // Set the format.
        Builder& set_format(VkFormat p_format);
        // Set the image extent.
        Builder& set_extent(VkExtent3D p_extent);
        // Set the mipmap level.
        Builder& set_mipmap_level(uint32_t p_mipmap_level);
        // Set the number of layers of the image.
        Builder& set_layers(uint32_t p_layers_count);
        // Set the image usage.
        Builder& set_usage(VkImageUsageFlags p_usage);
        // Set the queue family indices.
        Builder& set_queue_families(const std::set<uint32_t>& p_queue_family_indices);
        // Make a 2D image.
        Builder& image_2d(glm::uvec2 p_size);
        // Make a 2D image.
        Builder& image_2d(VkExtent2D p_extent);
        // Make a 3D image.
        Builder& image_3d(glm::uvec3 p_size);
        // Make a 3D image.
        Builder& image_3d(VkExtent3D p_extent);
        // Make an RGB image with each element 8 bit wide.
        Builder& rgb();
        // Make an SRGB image with each element 8 bit wide.
        Builder& srgb();
        // Make an RGBA image with each element 8 bit wide.
        Builder& rgba();
        // Make an SRGBA image with each element 8 bit wide.
        Builder& srgba();
        // Make an RGB HDR image with each element 32 bit wide.
        Builder& rgb_hdr();
        // Make an RGBA HDR image with each element 32 bit wide.
        Builder& rgba_hdr();
        // Set the initial image layout.
        Builder& set_initial_layout(VkImageLayout p_initial_layout);

    private:
        VkImage build(const BaseRenderDataVK* p_render_data) const;
    };

    ROImageVK() = delete;
    virtual ~ROImageVK() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(ROImageVK)

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    ROImageVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Get image instance.
     *
     * @return The image instance.
     */
    VkImage get_image() const {
        return exposed_fileds.image;
    }

    /**
     * @brief Set the data of the image immediately. This function has to be called
     * within a scope of recording command buffer.
     *
     * @param p_command_buffer The command buffer to record the image transfer.
     * @param p_data The buffer to get data from.
     * @param p_region_info The region info of the src buffer and the dst image.
     */
    void set_image_data_immediate(VkCommandBuffer p_command_buffer, Ref<ROBufferVK> p_data, VkBufferImageCopy p_region_info);

    struct ExposedFields {
        /**
         * @brief The image instance.
         */
        VkImage image = VK_NULL_HANDLE;
        /**
         * @brief The image's memory.
         */
        VkDeviceMemory image_memory = VK_NULL_HANDLE;
        /**
         * @brief The image's current layout after all pending transitions. Notice that this field does not
         * represent the actual current layout of the image, but it represent the image layout after all pending
         * transitions of the barriers that was pushed.
         */
        VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    /**
     * @brief Get the exposed fields.
     *
     * @return The exposed fields.
     */
    const ExposedFields& fields() const {
        return exposed_fileds;
    }

    /**
     * @class BufferMemoryBarrierBuilder
     * @brief Builder for buffer memory barriers.
     *
     */
    struct MemoryBarrierBuilder {
        friend ROImageVK;
        // The source access.
        VkAccessFlags2 src_access_mask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        // The source stage.
        VkPipelineStageFlags2 src_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        // The destination access.
        VkAccessFlags2 dst_access_mask = VK_ACCESS_2_MEMORY_READ_BIT;
        // The destination stage.
        VkPipelineStageFlags2 dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        // The old layout of the image. You can set this to VK_IMAGE_LAYOUT_UNDEFINED if you don't care about the
        // existing image data.
        VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        // The new layout that the image is going to transition to.
        Required<VkImageLayout> new_layout;
        // The source queue family to tranfer ownership. If all the ownerships in ROBufferVK is set
        // in queue_family_indices, this field could be set to VK_QUEUE_FAMILY_IGNORED.
        uint32_t src_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
        // The destination queue family to tranfer ownership. If all the ownerships in ROBufferVK is
        // set in queue_family_indices, this field could be set to VK_QUEUE_FAMILY_IGNORED.
        uint32_t dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
        // The image to set barrier on.
        Ref<ROImageVK> image;
        VkImageSubresourceRange subresource_range;
        // TODO: sub-resource range

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
         * @param p_drop_old Should drop the old data. The old data of the image won't be kept if true.
         * If false, old_layout information would be retrieved from the image.
         * @return *this.
         */
        MemoryBarrierBuilder& set_image(Ref<ROImageVK> p_image, bool p_drop_old = false) {
            image = p_image;
            if (p_drop_old) {
                // If the old data will be dropped, set the old layout to VK_IMAGE_LAYOUT_UNDEFINED
                old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
            else {
                old_layout = p_image->fields().image_layout;
            }
            if (!new_layout.has_value()) {
                // If new layout not specified, use the old layout, which the layout will not be changed.
                new_layout = p_image->fields().image_layout;
            }
            return *this;
        }

        /**
         * @brief Set the layout that this image will be transitioned to after triggering this barrier.
         *
         * @param p_new_layout The new layout to transition to.
         * @return *this.
         */
        MemoryBarrierBuilder& set_new_layout(VkImageLayout p_new_layout) {
            new_layout = p_new_layout;
            return *this;
        }

        /**
         * @brief Apply this barrier to the command buffer.
         *
         * @param p_command_buffer The command buffer to push to.
         */
        void apply_barrier(VkCommandBuffer p_command_buffer);
    };

private:
    ExposedFields exposed_fileds{};

    void init(const Builder& p_builder);
};

} // namespace WhiteBirdEngine

#endif
