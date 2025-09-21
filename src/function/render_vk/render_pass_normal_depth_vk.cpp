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
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

Ref<RenderData> RenderPassNormalDepthVK::setup(Ref<RenderData> p_input) {
    RenderPassNormalDepthVKInput* input = std::bit_cast<RenderPassNormalDepthVKInput*>(p_input.get());
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

    if (vkCreateRenderPass(input->logic_device, &render_pass_info, input->allocation_callbacks, &render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }
    return p_input;
}

Ref<RenderData> RenderPassNormalDepthVK::involke(Ref<RenderData> p_input) {
    return MEM_NULL; // TODO
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

}
