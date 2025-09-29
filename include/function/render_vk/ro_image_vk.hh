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

#include "function/render/render_object.hh"
#include "function/render_vk/renderer_vk.hh"
#include <cstdint>
#include <optional>
#include <set>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ROImageVK
 * @brief Image.
 *
 */
class ROImageVK final : public RenderObject {
public:
    // Builder for ROImageVK
    struct Builder {
        friend ROImageVK;
        // Image type.
        std::optional<VkImageType> image_type;
        // Image format.
        std::optional<VkFormat> format;
        // Image extent.
        std::optional<VkExtent3D> extent;
        // Mipmap level.
        uint32_t mipmap_level = 1;
        // Number of layers.
        uint32_t layers_count = 1;
        // Image usages.
        std::optional<VkImageUsageFlags> usage;
        // Queue family indices.
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

    private:
        VkImage build(const RendererVK::BaseRenderDataVK* p_render_data) const;
    };

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_config Configuration.
     */
    ROImageVK(const RendererVK::BaseRenderDataVK* p_render_data, const Builder& p_builder);
    virtual ~ROImageVK() override;

    virtual std::string get_ro_type() const override {
        return "image_vk";
    }

    /**
     * @brief Get image instance.
     *
     * @return The image instance.
     */
    VkImage get_image() const {
        return image;
    }

private:
    VkImage image;
    const RendererVK::BaseRenderDataVK* render_data;

    void init(const Builder& p_config);
};

}

#endif
