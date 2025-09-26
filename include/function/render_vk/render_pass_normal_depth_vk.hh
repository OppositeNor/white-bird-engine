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
#ifndef __WBE_RENDER_PASS_NORMAL_DEPTH_VK_HH__
#define __WBE_RENDER_PASS_NORMAL_DEPTH_VK_HH__

#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_pass.hh"
#include "function/render_vk/graphics_pipeline_vk.hh"
#include "function/render_vk/render_pass_vk.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RenderPassNormalDepthVK
 * @brief Render pass for depth and normal.
 *
 */
class RenderPassNormalDepthVK final : public RenderPassVK {
public:
    WBE_RENDER_PASS(RenderPassNormalDepthVK, normal_depth, VULKAN);

    RenderPassNormalDepthVK() = default;
    virtual ~RenderPassNormalDepthVK() override;
    RenderPassNormalDepthVK(const RenderPassNormalDepthVK&) = delete;
    RenderPassNormalDepthVK(RenderPassNormalDepthVK&&) = delete;
    RenderPassNormalDepthVK& operator=(const RenderPassNormalDepthVK&) = delete;
    RenderPassNormalDepthVK& operator=(RenderPassNormalDepthVK&&) = delete;

    virtual Ref<RenderData> setup(Ref<RenderData> p_input) override;
    virtual Ref<RenderData> involke(Ref<RenderData> p_input) override;


private:
    bool initialized = false;
    VkRenderPass render_pass;
    VkFramebuffer framebuffer;
    Ref<VkImage> normal_map;
    VkImageView normal_view;
    Ref<VkImage> depth_map;
    VkImageView depth_view;

    Unique<GraphicsPipelineVK> graphics_pipeline;

    void setup_render_pass(RenderDataVK* p_input);
    void setup_framebuffer(RenderDataVK* p_input);

    VkAttachmentDescription create_normal_attachment();
    VkAttachmentDescription create_depth_attachment();
};

}

#endif
