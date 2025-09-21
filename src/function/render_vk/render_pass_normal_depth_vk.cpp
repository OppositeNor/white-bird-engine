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
#include "function/render_vk/render_pass_normal_depth_vk.hh"
#include "core/allocator/allocator.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

Ref<RenderPass::RenderData> RenderPassNormalDepthVK::setup(Ref<RenderData> p_input) {
    RenderDataVK* input = std::bit_cast<RenderDataVK*>(p_input.get());
    setup_render_pass(input);
    setup_framebuffer(input);
    return p_input;
}

Ref<RenderPass::RenderData> RenderPassNormalDepthVK::involke(Ref<RenderData> p_input) {
    return MEM_NULL; // TODO
}

void RenderPassNormalDepthVK::setup_render_pass(RenderDataVK* p_input) {
    // Setup color attachment
    VkAttachmentDescription normal_attachment = create_normal_attachment();
    VkAttachmentDescription depth_attachment = create_depth_attachment();

    // Color attachment.
    VkAttachmentReference color_attachment_reference{};
    color_attachment_reference.attachment = 0;
    // Image going to be rendered using a graphics pipeline.
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Setup subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    // 2 attachments: normal and depth
    render_pass_info.attachmentCount = 2;
    VkAttachmentDescription attachments[] {
        normal_attachment, depth_attachment
    };
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(p_input->base_render_data->logic_device, &render_pass_info,
                           p_input->base_render_data->allocation_callbacks, &render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }
}


void RenderPassNormalDepthVK::setup_framebuffer(RenderDataVK* p_input) {
    create_renderable_image_2d(p_input, VK_FORMAT_R8G8B8_UNORM, &normal_map);
    create_renderable_image_2d(p_input, VK_FORMAT_R32_SFLOAT, &depth_map);
    create_renderable_image_view_2d(p_input, normal_map, VK_FORMAT_R8G8B8_UNORM, &normal_view);
    create_renderable_image_view_2d(p_input, depth_map, VK_FORMAT_R32_SFLOAT, &depth_view);

    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = render_pass;
    create_info.attachmentCount = 2;
    VkImageView attachments[] = { normal_view, depth_view };
    create_info.pAttachments = attachments;
    create_info.width = p_input->base_render_data->swap_chain_extent.width;
    create_info.height = p_input->base_render_data->swap_chain_extent.height;
    create_info.layers = 1;

    if (vkCreateFramebuffer(p_input->base_render_data->logic_device, &create_info,
                            p_input->base_render_data->allocation_callbacks, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create framebuffer.");
    }
}

VkAttachmentDescription RenderPassNormalDepthVK::create_normal_attachment() {
    // Setup color attachment
    VkAttachmentDescription color_attachment{};
    color_attachment.format = VK_FORMAT_R8G8B8_UNORM;
    // Disable multisample.
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // Clear the color before every render pass.
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // Store the color after every render pass.
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // Don't care about the stencil images.
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // The attachment's initial layout is not known.
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // The attachment's final layout should be able to present on screen.
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return color_attachment;

}

VkAttachmentDescription create_depth_attachment() {
    // Setup color attachment
    VkAttachmentDescription color_attachment{};
    color_attachment.format = VK_FORMAT_R32_SFLOAT;
    // Disable multisample.
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // Clear the color before every render pass.
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // Store the color after every render pass.
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // Don't care about the stencil images.
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // The attachment's initial layout is not known.
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // The attachment's final layout should be able to present on screen.
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return color_attachment;
}

}
