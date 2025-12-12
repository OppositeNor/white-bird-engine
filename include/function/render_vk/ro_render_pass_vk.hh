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

#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RORenderPassVK
 * @brief Render pass.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) RORenderPassVK : public RenderObjectVK {
public:
    // Builder for RORenderPassVK
    struct Builder {
        friend RORenderPassVK;
        // Attachment descriptions.
        std::vector<VkAttachmentDescription> attachments;
        // Subpass descriptions.
        std::vector<VkSubpassDescription> subpasses;
        // Subpass dependencies.
        std::vector<VkSubpassDependency> dependencies;

        // Set attachment descriptions.
        Builder& set_attachments(const std::vector<VkAttachmentDescription>& p_attachments);
        // Add a single attachment description.
        Builder& add_attachment(const VkAttachmentDescription& p_attachment);
        // Set subpass descriptions.
        Builder& set_subpasses(const std::vector<VkSubpassDescription>& p_subpasses);
        // Add a single subpass description.
        Builder& add_subpass(const VkSubpassDescription& p_subpass);
        // Set subpass dependencies.
        Builder& set_dependencies(const std::vector<VkSubpassDependency>& p_dependencies);
        // Add a single subpass dependency.
        Builder& add_dependency(const VkSubpassDependency& p_dependency);

    private:
        VkRenderPass build(const BaseRenderDataVK* p_render_data) const;
    };

    RORenderPassVK() = delete;
    virtual ~RORenderPassVK() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(RORenderPassVK)

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    RORenderPassVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Get the render pass instance.
     *
     * @return The render pass instance.
     */
    VkRenderPass get_render_pass() const {
        return render_pass;
    }

private:
    VkRenderPass render_pass;

    void init(const Builder& p_builder);
};

} // namespace WhiteBirdEngine

#endif
