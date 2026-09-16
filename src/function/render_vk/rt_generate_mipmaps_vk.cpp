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
#include "function/render_vk/rt_generate_mipmaps_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "generated/type_uuid.gen.hh"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTGenerateMipmapsVK::RTGenerateMipmapsVK(
    const BaseRenderDataVK* p_render_data, std::string_view p_image_view_id, VkExtent2D p_base_extent, uint32_t p_mipmap_level_count, VkFormat p_format)
    : render_data(p_render_data), image_view_id(p_image_view_id), base_extent(p_base_extent),
      mipmap_level_count(p_mipmap_level_count) {
    if (render_data == nullptr || render_data->physical_device == VK_NULL_HANDLE) {
        // Format capabilities cannot be queried without a valid physical device.
        throw std::runtime_error("RTGenerateMipmapsVK: physical device is missing.");
    }
    if (base_extent.width == 0 || base_extent.height == 0 || mipmap_level_count == 0) {
        // Empty images and mip chains cannot produce a valid blit sequence.
        throw std::runtime_error("RTGenerateMipmapsVK: image extent and mip level count must be non-zero.");
    }
    // Limit the mip chain to the number of levels supported by the base extent.
    const uint32_t maximum_mipmap_level_count = std::bit_width(std::max(base_extent.width, base_extent.height));
    if (mipmap_level_count > maximum_mipmap_level_count) {
        // A longer chain would address mip levels that do not exist for this extent.
        throw std::runtime_error("RTGenerateMipmapsVK: mip level count exceeds the levels supported by the base extent.");
    }
    // Require every format feature used by the linear blit mip-generation path.
    VkFormatProperties format_properties{};
    vkGetPhysicalDeviceFormatProperties(render_data->physical_device, p_format, &format_properties);
    constexpr VkFormatFeatureFlags REQUIRED_FEATURES =
        VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if ((format_properties.optimalTilingFeatures & REQUIRED_FEATURES) != REQUIRED_FEATURES) {
        // Linear mip generation requires the format to support both blit directions and filtering.
        throw std::runtime_error("RTGenerateMipmapsVK: image format does not support filtered mipmap blits.");
    }
}

Vector<ResAccessVK, RenderAllocator> RTGenerateMipmapsVK::read_resources() const {
    // Declare transfer reads from source mip levels to the render graph.
    Vector<ResAccessVK, RenderAllocator> resources(render_data->allocator);
    resources.push_back({.res_id = image_view_id, .usage = RendResUsageVK::TRANSFER_SRC});
    return resources;
}

Vector<ResAccessVK, RenderAllocator> RTGenerateMipmapsVK::write_resources() const {
    // Declare transfer writes to destination mip levels to the render graph.
    Vector<ResAccessVK, RenderAllocator> resources(render_data->allocator);
    resources.push_back({.res_id = image_view_id, .usage = RendResUsageVK::TRANSFER_DST});
    return resources;
}

void RTGenerateMipmapsVK::bind_resources(const RendResViewVK& p_resources) {
    image_view_container = p_resources.get(image_view_id).container;
    if (image_view_container == MEM_NULL || image_view_container->get_resource_type_id() != TypeUUID<ROImageViewVK>::UUID) {
        throw std::runtime_error("RTGenerateMipmapsVK: image-view resource has the wrong type.");
    }
}

void RTGenerateMipmapsVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) {
    // Resolve the graph image view and its underlying image for this frame.
    auto image_view = image_view_container->get().dynamic_cast_ref<ROImageViewVK>();
    if (image_view == MEM_NULL || image_view->fields().image == MEM_NULL) {
        // Commands cannot be recorded without a bound image view and image.
        throw std::runtime_error("RTGenerateMipmapsVK: image-view resource is missing.");
    }
    // Generate each destination mip from the preceding level at half resolution.
    Ref<ROImageVK> image = image_view->fields().image;
    int32_t source_width = static_cast<int32_t>(base_extent.width);
    int32_t source_height = static_cast<int32_t>(base_extent.height);
    for (uint32_t mip_level = 1; mip_level < mipmap_level_count; ++mip_level) {
        if (mip_level > 1) {
            // Make a previously generated source mip visible before reading it in the next blit.
            ROImageVK::MemoryBarrierBuilder()
                .set_src_access(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT)
                .set_dst_access(VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT)
                .set_subresource_range(VkImageSubresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = mip_level - 1, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1})
                .set_new_layout(VK_IMAGE_LAYOUT_GENERAL)
                .set_image(image)
                .apply_barrier(p_command_buffer);
        }
        // Discard and transition the destination mip before completely overwriting it.
        ROImageVK::MemoryBarrierBuilder()
            .set_src_access(VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE)
            .set_dst_access(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT)
            .set_subresource_range(VkImageSubresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = mip_level, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1})
            .set_new_layout(VK_IMAGE_LAYOUT_GENERAL)
            .set_image(image, true)
            .apply_barrier(p_command_buffer);

        // Describe the linear blit from the source mip into the next smaller level.
        const int32_t destination_width = std::max(source_width / 2, 1);
        const int32_t destination_height = std::max(source_height / 2, 1);
        VkImageBlit2 image_blit{};
        image_blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        image_blit.srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = mip_level - 1, .baseArrayLayer = 0, .layerCount = 1};
        image_blit.srcOffsets[1] = {.x = source_width, .y = source_height, .z = 1};
        image_blit.dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = mip_level, .baseArrayLayer = 0, .layerCount = 1};
        image_blit.dstOffsets[1] = {.x = destination_width, .y = destination_height, .z = 1};
        // Record the filtered blit with both subresources in the general layout.
        VkBlitImageInfo2 blit_info{};
        blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blit_info.srcImage = image->get_image();
        blit_info.srcImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        blit_info.dstImage = image->get_image();
        blit_info.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        blit_info.regionCount = 1;
        blit_info.pRegions = &image_blit;
        blit_info.filter = VK_FILTER_LINEAR;
        vkCmdBlitImage2(p_command_buffer, &blit_info);
        // Advance the source dimensions for the next mip-generation iteration.
        source_width = destination_width;
        source_height = destination_height;
    }
}

} // namespace WhiteBirdEngine
