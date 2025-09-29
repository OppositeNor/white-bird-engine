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
#include "function/render_vk/ro_render_pass_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RORenderPassVK::Builder& RORenderPassVK::Builder::set_attachments(const std::vector<VkAttachmentDescription>& p_attachments) {
    attachments = p_attachments;
    return *this;
}

RORenderPassVK::Builder& RORenderPassVK::Builder::add_attachment(const VkAttachmentDescription& p_attachment) {
    attachments.push_back(p_attachment);
    return *this;
}

RORenderPassVK::Builder& RORenderPassVK::Builder::set_subpasses(const std::vector<VkSubpassDescription>& p_subpasses) {
    subpasses = p_subpasses;
    return *this;
}

RORenderPassVK::Builder& RORenderPassVK::Builder::add_subpass(const VkSubpassDescription& p_subpass) {
    subpasses.push_back(p_subpass);
    return *this;
}

RORenderPassVK::Builder& RORenderPassVK::Builder::set_dependencies(const std::vector<VkSubpassDependency>& p_dependencies) {
    dependencies = p_dependencies;
    return *this;
}

RORenderPassVK::Builder& RORenderPassVK::Builder::add_dependency(const VkSubpassDependency& p_dependency) {
    dependencies.push_back(p_dependency);
    return *this;
}

VkRenderPass RORenderPassVK::Builder::build(const RendererVK::BaseRenderDataVK* p_render_data) const {
    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = attachments.size();
    create_info.pAttachments = attachments.data();
    create_info.subpassCount = subpasses.size();
    create_info.pSubpasses = subpasses.data();
    create_info.dependencyCount = dependencies.size();
    create_info.pDependencies = dependencies.data();
    
    VkRenderPass result;
    if (vkCreateRenderPass(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }
    return result;
}

RORenderPassVK::RORenderPassVK(const RendererVK::BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : render_data(p_render_data) {
    init(p_builder);
}

RORenderPassVK::~RORenderPassVK() {
    vkDestroyRenderPass(render_data->logic_device, render_pass, render_data->allocation_callbacks);
}

void RORenderPassVK::init(const Builder& p_builder) {
    render_pass = p_builder.build(render_data);
}

void RORenderPassVK::create_renderable_image_2d(const RendererVK::BaseRenderDataVK* p_input, VkFormat p_format, VkImage* p_image) {
    VkImageCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    // 2D image
    create_info.imageType = VK_IMAGE_TYPE_2D;
    // 8 bits rgba
    create_info.format = p_format;
    // Set size to the size of the swap chain's images.
    create_info.extent.width = p_input->swap_chain_extent.width;
    create_info.extent.height = p_input->swap_chain_extent.height;
    // No mipmap
    create_info.mipLevels = 1;
    // 1 layer image
    create_info.arrayLayers = 1;
    // 1 sample per pixel
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    // Optimal memory layout
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    // Could be rendered into.
    create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (p_input->queue_family_index_graphics != p_input->queue_family_index_compute) {
        // Shared with the graphics queue and the compute queue.
        create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        uint32_t shared_queue_families[] = {
            p_input->queue_family_index_graphics,
            p_input->queue_family_index_compute
        };
        create_info.pQueueFamilyIndices = shared_queue_families;
    }
    else {
        // Exclusive to one queue.
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    // Image is going to be rendered by a graphics pipeline.
    create_info.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    if (vkCreateImage(p_input->logic_device, &create_info, p_input->allocation_callbacks, p_image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image.");
    }
}

void RORenderPassVK::create_renderable_image_view_2d(const RendererVK::BaseRenderDataVK* p_input, VkImage p_image, VkFormat p_format, VkImageView* p_image_view) {
    VkImageViewCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    // Set the image of the image view.
    create_info.image = p_image;
    // The view type is 2D.
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    // The format of the image view.
    create_info.format = p_format;
    // Map the rgba directly to the result.
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    // Specify the mipmap level.
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(p_input->logic_device, &create_info, p_input->allocation_callbacks, p_image_view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view.");
    }
}

}
