#include "function/render_vk/render_pass_vk.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

void RenderPassVK::create_renderable_image_2d(RenderDataVK* p_input, VkFormat p_format, VkImage* p_image) {
    VkImageCreateInfo create_info{};
    BaseRenderDataVK* base_render_data = p_input->base_render_data;
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    // 2D image
    create_info.imageType = VK_IMAGE_TYPE_2D;
    // 8 bits rgba
    create_info.format = p_format;
    // Set size to the size of the swap chain's images.
    create_info.extent.width = base_render_data->swap_chain_extent.width;
    create_info.extent.height = base_render_data->swap_chain_extent.height;
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
    if (base_render_data->queue_family_index_graphics != base_render_data->queue_family_index_compute) {
        // Shared with the graphics queue and the compute queue.
        create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        uint32_t shared_queue_families[] = {
            base_render_data->queue_family_index_graphics,
            base_render_data->queue_family_index_compute
        };
        create_info.pQueueFamilyIndices = shared_queue_families;
    }
    else {
        // Exclusive to one queue.
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    // Image is going to be rendered by a graphics pipeline.
    create_info.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    if (vkCreateImage(base_render_data->logic_device, &create_info, base_render_data->allocation_callbacks, p_image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image.");
    }
}

void RenderPassVK::create_renderable_image_view_2d(RenderDataVK* p_input, VkImage p_image, VkFormat p_format, VkImageView* p_image_view) {
    VkImageViewCreateInfo create_info;
    BaseRenderDataVK* base_render_data = p_input->base_render_data;
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
    // Specify the mipmap level. Should be 0 since it is only rendered to the 
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(base_render_data->logic_device, &create_info, base_render_data->allocation_callbacks, p_image_view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view.");
    }
}

}
