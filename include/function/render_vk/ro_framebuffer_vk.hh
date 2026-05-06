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
#ifndef WBE_FILE_RO_FRAMEBUFFER_VK_HH
#define WBE_FILE_RO_FRAMEBUFFER_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_render_pass_vk.hh"
#include "glm/fwd.hpp"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ROFramebufferVK
 * @brief Framebuffer render object.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) ROFramebufferVK final : public RenderObjectVK {
public:
    /**
     * @class Builder
     * @brief Builder class for framebuffer.
     *
     */
    struct Builder {
        friend ROFramebufferVK;
        // The attachment image views.
        // std::vector<VkImageView> attachments;
        std::vector<Ref<ROImageViewVK>> attachments;
        // The render pass this framebuffer will be compatable with.
        Ref<RORenderPassVK> render_pass;
        // The width of the framebuffer.
        Required<uint32_t> width;
        // The height of the framebuffer.
        Required<uint32_t> height;
        // The layers of the framebuffer.
        uint32_t layers = 1;

        // Set the attachment image views.
        Builder& set_attachments(const std::vector<Ref<ROImageViewVK>>& p_attachments) {
            for (const auto& attachment : p_attachments) {
                add_attachment(attachment);
            }
            return *this;
        }
        // Add an attachment image view.
        Builder& add_attachment(Ref<ROImageViewVK> p_image_view) {
            attachments.push_back(p_image_view);
            return *this;
        }
        // Set the extent of the framebuffer.
        Builder& set_extent(VkExtent2D p_extent) {
            width = p_extent.width;
            height = p_extent.height;
            return *this;
        }
        // Set the extent of the framebuffer.
        Builder& set_extent(glm::uvec2 p_extent) {
            width = p_extent.x;
            height = p_extent.y;
            return *this;
        }
        // Set the extent of the framebuffer.
        Builder& set_extent(VkExtent3D p_extent) {
            width = p_extent.width;
            height = p_extent.height;
            layers = p_extent.depth;
            return *this;
        }
        // Set the extent of the framebuffer.
        Builder& set_extent(glm::uvec3 p_extent) {
            width = p_extent.x;
            height = p_extent.y;
            layers = p_extent.z;
            return *this;
        }
        // Set the width of the framebuffer.
        Builder& set_width(uint32_t p_width) {
            width = p_width;
            return *this;
        }
        // Set the height of the framebuffer.
        Builder& set_height(uint32_t p_height) {
            height = p_height;
            return *this;
        }
        // Set the layers of the framebuffer.
        Builder& set_layers(uint32_t p_layers) {
            layers = p_layers;
            return *this;
        }
        // Set the render pass this framebuffer will be compatable with.
        Builder& set_render_pass(Ref<RORenderPassVK> p_render_pass) {
            render_pass = p_render_pass;
            return *this;
        }

        Ref<ROFramebufferVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<ROFramebufferVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<ROFramebufferVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<ROFramebufferVK>(p_render_data->allocator, p_render_data, *this);
        }

    private:
        VkFramebuffer create_vk_object(const BaseRenderDataVK* p_render_data) const;
    };
    ROFramebufferVK() = delete;
    virtual ~ROFramebufferVK() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(ROFramebufferVK)

    ROFramebufferVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Get the framebuffer instance.
     *
     * @return The framebuffer instance.
     */
    VkFramebuffer get_framebuffer() const {
        return framebuffer;
    }

    /**
     * @class ExposedFields
     * @brief Exposed fields of framebuffer.
     *
     */
    struct ExposedFields {
        // The render pass.
        Ref<RORenderPassVK> render_pass;
        // The width and height of the framebuffer.
        glm::uvec2 extent;
    };

    /**
     * @brief Get the exposed fields of framebuffer.
     *
     * @return The exposed fields of framebuffer.
     */
    const ExposedFields& fields() const {
        return exposed_fields;
    }

private:
    VkFramebuffer framebuffer;
    ExposedFields exposed_fields;
};
} // namespace WhiteBirdEngine

#endif
