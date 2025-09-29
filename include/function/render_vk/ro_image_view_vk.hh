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
#ifndef __WBE_RO_IMAGE_VIEW_VK_HH__
#define __WBE_RO_IMAGE_VIEW_VK_HH__

#include "function/render/render_object.hh"
#include "function/render_vk/renderer_vk.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class ROImageViewVK final : public RenderObject {
public:
    // Builder for ROImageViewVK
    struct Builder {
        friend ROImageViewVK;
        // Source image.
        std::optional<VkImage> image;
        // Image view type.
        std::optional<VkImageViewType> view_type;
        // Image format.
        std::optional<VkFormat> format;
        // Component mapping.
        VkComponentMapping components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        };
        // Subresource range.
        std::optional<VkImageSubresourceRange> subresource_range;

        // Set the source image.
        Builder& set_image(VkImage p_image);
        // Set the image view type.
        Builder& set_view_type(VkImageViewType p_view_type);
        // Set the image format.
        Builder& set_format(VkFormat p_format);
        // Set component mapping.
        Builder& set_components(const VkComponentMapping& p_components);
        // Set subresource range.
        Builder& set_subresource_range(const VkImageSubresourceRange& p_subresource_range);

    private:
        VkImageView build(const RendererVK::BaseRenderDataVK* p_render_data) const;
    };

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    ROImageViewVK(const RendererVK::BaseRenderDataVK* p_render_data, const Builder& p_builder);
    virtual ~ROImageViewVK() override;

    virtual std::string get_ro_type() const override {
        return "image_view_vk";
    }

    /**
     * @brief Get the image view instance.
     *
     * @return The image view instance.
     */
    VkImageView get_image_view() const {
        return image_view;
    }

private:    
    VkImageView image_view;
    const RendererVK::BaseRenderDataVK* render_data;

    void init(const Builder& p_builder);
};

}

#endif
