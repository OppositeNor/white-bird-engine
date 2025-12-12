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
#ifndef __WBE_RO_IMAGE_VK_HH__
#define __WBE_RO_IMAGE_VK_HH__

#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
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
class WBE_META(WBE_RENDER_OBJECT) ROImageVK final : public RenderObjectVK {
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
        return image;
    }

private:
    VkImage image = VK_NULL_HANDLE;

    void init(const Builder& p_config);
};

} // namespace WhiteBirdEngine

#endif
