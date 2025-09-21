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
    VkAttachmentDescription color_attachment = create_result_color_attachment();

    // Color attachment.
    VkAttachmentReference color_attachment_reference{};
    color_attachment_reference.attachment = 0;
    // Image going to be rendered using a graphics pipeline.
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
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

    if (vkCreateRenderPass(p_input->logic_device, &render_pass_info, p_input->allocation_callbacks, &render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }
}


void RenderPassNormalDepthVK::setup_framebuffer(RenderDataVK* p_input) {
    setup_render_image(p_input);
    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = render_pass;
    create_info.attachmentCount = 1;
    VkImageView attachments[] = { render_image_view };
    create_info.pAttachments = attachments;
    create_info.width = p_input->swap_chain_extent.width;
    create_info.height = p_input->swap_chain_extent.height;
    create_info.layers = 1;

    if (vkCreateFramebuffer(p_input->logic_device, &create_info, p_input->allocation_callbacks, &framebuffer)) {
        throw std::runtime_error("Failed to create framebuffer.");
    }
}

VkAttachmentDescription RenderPassNormalDepthVK::create_result_color_attachment() {
    // Setup color attachment
    VkAttachmentDescription color_attachment{};
    color_attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
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

void RenderPassNormalDepthVK::setup_render_image(RenderDataVK* p_input) {
    VkImageCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    // 2D image
    create_info.imageType = VK_IMAGE_TYPE_2D;
    // 8 bits rgba
    create_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    // Set size to the size of the swap chain's images.
    create_info.extent.width = p_input->swap_chain_extent.width;
    create_info.extent.height = p_input->swap_chain_extent.height;
    // No mipmap
    create_info.mipLevels = 0;
    // 1 layer image
    create_info.arrayLayers = 1;
    // 1 sample per pixel
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    // Optimal memory layout
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    // Could be rendered into.
    create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // TODO
}


}
