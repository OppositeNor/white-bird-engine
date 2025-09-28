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
#ifndef __WBE_RO_RENDER_PASS_VK_HH__
#define __WBE_RO_RENDER_PASS_VK_HH__

#include "function/render/render_object.hh"
#include "function/render_vk/renderer_vk.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RORenderPassVK
 * @brief Render pass.
 *
 */
class RORenderPassVK : public RenderObject {
public:
    // Render pass configurations.
    struct Config {
        // Attachment descriptions.
        std::vector<VkAttachmentDescription> attachments;
        // Subpass descriptions.
        std::vector<VkSubpassDescription> subpasses;
    };

    RORenderPassVK(const RendererVK::BaseRenderDataVK* p_render_data, const Config& p_config);
    virtual ~RORenderPassVK() override;

    /**
     * @brief Get the render pass instance.
     *
     * @return The render pass instance.
     */
    VkRenderPass get_render_pass() const {
        return render_pass;
    }

protected:
    void create_renderable_image_2d(const RendererVK::BaseRenderDataVK* p_input, VkFormat p_format, VkImage* p_image);
    void create_renderable_image_view_2d(const RendererVK::BaseRenderDataVK* p_input, VkImage p_image, VkFormat p_format, VkImageView* p_image_view);

private:
    const RendererVK::BaseRenderDataVK* render_data;
    VkRenderPass render_pass;

    void init(const RendererVK::BaseRenderDataVK* p_render_data, const Config& p_config);
};

}

#endif
