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
#ifndef WBE_FILE_RO_IMAGE_VIEW_VK_HH
#define WBE_FILE_RO_IMAGE_VIEW_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ROImageViewVK
 * @brief Vulkan image view object.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) ROImageViewVK final : public RenderObjectVK {
public:
    // Builder for ROImageViewVK
    struct Builder {
        friend ROImageViewVK;
        // The image.
        Ref<ROImageVK> image;
        // Image view type.
        Required<VkImageViewType> view_type;
        // Image format.
        Required<VkFormat> format;
        // Component mapping.
        VkComponentMapping components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY};
        // Subresource range.
        VkImageSubresourceRange subresource_range{.baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1};

        // Set the source image.
        Builder& set_image(Ref<ROImageVK> p_image);
        // Set the image view type.
        Builder& set_view_type(VkImageViewType p_view_type);
        // Set the image format.
        Builder& set_format(VkFormat p_format);
        // Set component mapping.
        Builder& set_components(const VkComponentMapping& p_components);
        // Set component mapping.
        Builder& set_components(VkComponentSwizzle p_r, VkComponentSwizzle p_g, VkComponentSwizzle p_b, VkComponentSwizzle p_a);
        // Set subresource range.
        Builder& set_subresource_range(const VkImageSubresourceRange& p_subresource_range);
        // Set from image configurations. All features are default to match to
        // the image config.
        Builder& set_from_image_config(const ROImageVK::Builder& p_image_config);

        Ref<ROImageViewVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<ROImageViewVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<ROImageViewVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<ROImageViewVK>(p_render_data->allocator, p_render_data, *this);
        }

    private:
        VkImageView create_vk_object(const BaseRenderDataVK* p_render_data) const;
    };
    ROImageViewVK() = delete;
    virtual ~ROImageViewVK() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(ROImageViewVK)

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    ROImageViewVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Get the image view instance.
     *
     * @return The image view instance.
     */
    VkImageView get_image_view() const {
        return image_view;
    }

    /**
     * @class ExposedFields
     * @brief Exposed fields of ROImageViewVK.
     *
     */
    struct ExposedFields {
        /**
         * @brief The source image.
         */
        Ref<ROImageVK> image = MEM_NULL;
    };

    const ExposedFields& fields() const {
        return exposed_fields;
    }

private:
    VkImageView image_view = VK_NULL_HANDLE;

    ExposedFields exposed_fields{};

    void init(const Builder& p_builder);
};
} // namespace WhiteBirdEngine

#endif
