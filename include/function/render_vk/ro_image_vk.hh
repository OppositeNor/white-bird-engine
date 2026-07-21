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

#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/spot_light_info.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "glm/ext/vector_uint2.hpp"
#include "glm/ext/vector_uint3.hpp"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <set>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ROImageVK
 * @brief Image.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) ROImageVK final : public RenderObjectVK {
    friend struct MemoryBarrierBuilder;
    friend class RendererVK;

public:
    // Builder for ROImageVK
    struct Builder {
        friend ROImageVK;
        // Format used for the depth attachment of a spot light shadow map.
        static constexpr VkFormat SHADOW_MAP_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
        // Format used for the dummy color attachment that accompanies a
        // spot light shadow map (the Vulkan render pass requires at least
        // one color attachment, but its contents are ignored).
        static constexpr VkFormat SHADOW_MAP_COLOR_FORMAT = VK_FORMAT_R8_UNORM;
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
        // The queue families this image will be used in.
        std::set<uint32_t> queue_families;

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
        // Set the queue families.
        Builder& set_queue_families(const std::set<uint32_t>& p_queue_families);
        // Make a 2D image.
        Builder& image_2d(glm::uvec2 p_size);
        // Make a 2D image.
        Builder& image_2d(VkExtent2D p_extent);
        // Make a 3D image.
        Builder& image_3d(glm::uvec3 p_size);
        // Make a 3D image.
        Builder& image_3d(VkExtent3D p_extent);
        // Configure this image as the depth attachment of a spot light's
        // shadow map: 2D, square, sized from the light's
        // `shadow_map_resolution`, format `SHADOW_MAP_DEPTH_FORMAT`, usage
        // depth-stencil attachment + sampled.
        Builder& shadow_map_depth(const SpotLightInfo& p_spot_light);
        // Configure this image as the dummy color attachment that pairs
        // with `shadow_map_depth`: 2D, square, sized from the light's
        // `shadow_map_resolution`, format `SHADOW_MAP_COLOR_FORMAT`, usage
        // color attachment.
        Builder& shadow_map_color(const SpotLightInfo& p_spot_light);
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

        Ref<ROImageVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<ROImageVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<ROImageVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<ROImageVK>(p_render_data->allocator, p_render_data, *this);
        }

    private:
        VkImage create_vk_object(const BaseRenderDataVK* p_render_data) const;
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
     * @brief Constructor for swap chain images. This image will not manually
     * allocate memory, and will not deallocate memory when destroyed.
     *
     * @param p_render_data The base render data.
     * @param p_image The vulkan image instance.
     */
    ROImageVK(const BaseRenderDataVK* p_render_data, VkImage p_image, VkImageLayout p_initial_image_layout);

    /**
     * @brief Get image instance.
     *
     * @return The image instance.
     */
    VkImage get_image() const {
        return exposed_fileds.image;
    }

    /**
     * @brief Set the data of the image immediately. This function has to be
     * called within a scope of recording command buffer.
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
         * @brief Is this a image for the swap chain.
         */
        bool is_swap_chain_image = false;
        /**
         * @brief The image's memory.
         */
        VkDeviceMemory image_memory = VK_NULL_HANDLE;
        /**
         * @brief The queue families the image is shared between.
         */
        std::set<uint32_t> queue_families;
        /**
         * @brief The image's current layout after all pending transitions.
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
        // The old layout of the image. You can set this to
        // VK_IMAGE_LAYOUT_UNDEFINED if you don't care about the existing image
        // data.
        VkImageLayout old_layout = VK_IMAGE_LAYOUT_GENERAL;
        // The destination queue family to tranfer ownership.
        uint32_t dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
        // The image to set barrier on.
        Ref<ROImageVK> image;
        VkImageSubresourceRange subresource_range{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1};

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
         * @brief Set the image subresource range covered by this barrier.
         *
         * @param p_subresource_range The image subresource range.
         * @return *this.
         */
        MemoryBarrierBuilder& set_subresource_range(VkImageSubresourceRange p_subresource_range) {
            subresource_range = p_subresource_range;
            return *this;
        }

        /**
         * @brief Set the image subresource range from image copy layers.
         *
         * @param p_subresource_layers The image subresource layers.
         * @return *this.
         */
        MemoryBarrierBuilder& set_subresource_range(VkImageSubresourceLayers p_subresource_layers) {
            subresource_range = VkImageSubresourceRange{.aspectMask = p_subresource_layers.aspectMask,
                .baseMipLevel = p_subresource_layers.mipLevel,
                .levelCount = 1,
                .baseArrayLayer = p_subresource_layers.baseArrayLayer,
                .layerCount = p_subresource_layers.layerCount};
            return *this;
        }

        /**
         * @brief Set the buffer to apply the barrier on.
         *
         * @param p_buffer The buffer to apply the barrier on.
         * @param p_drop_old Should drop the old data. The old data of the image
         * won't be kept if true. If false, old_layout information would be
         * retrieved from the image.
         * @return *this.
         */
        MemoryBarrierBuilder& set_image(Ref<ROImageVK> p_image, bool p_drop_old = false) {
            image = p_image;
            if (p_drop_old) {
                // If the old data will be dropped, set the old layout to
                // VK_IMAGE_LAYOUT_UNDEFINED
                old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            } else {
                old_layout = p_image->fields().image_layout;
            }
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
