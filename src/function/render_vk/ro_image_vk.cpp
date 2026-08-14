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
#include "function/render_vk/ro_image_vk.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/ecs/components.hh"
#include "function/function_utils.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "glm/ext/vector_uint2.hpp"
#include "glm/ext/vector_uint3.hpp"
#include <cstdint>
#include <format>
#include <glm/glm.hpp>
#include <set>
#include <stdexcept>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROImageVK::Builder& ROImageVK::Builder::set_image_type(VkImageType p_image_type) {
    image_type = p_image_type;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::set_format(VkFormat p_format) {
    format = p_format;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::set_extent(VkExtent3D p_extent) {
    extent = p_extent;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::set_mipmap_level(uint32_t p_mipmap_level) {
    mipmap_level = p_mipmap_level;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::set_layers(uint32_t p_layers_count) {
    layers_count = p_layers_count;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::set_usage(VkImageUsageFlags p_usage) {
    usage = p_usage;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::set_queue_families(const std::set<uint32_t>& p_queue_families) {
    queue_families = p_queue_families;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::image_2d(glm::uvec2 p_size) {
    image_type = VK_IMAGE_TYPE_2D;
    extent = VkExtent3D{.width = p_size.x, .height = p_size.y, .depth = 1};
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::image_2d(VkExtent2D p_extent) {
    image_type = VK_IMAGE_TYPE_2D;
    extent = VkExtent3D{.width = p_extent.width, .height = p_extent.height, .depth = 1};
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::image_3d(glm::uvec3 p_size) {
    image_type = VK_IMAGE_TYPE_3D;
    extent = VkExtent3D{.width = p_size.x, .height = p_size.y, .depth = p_size.z};
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::image_3d(VkExtent3D p_extent) {
    image_type = VK_IMAGE_TYPE_3D;
    extent = p_extent;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::shadow_map_depth(const ComponentSpotLight& p_spot_light) {
    image_type = VK_IMAGE_TYPE_2D;
    format = SHADOW_MAP_DEPTH_FORMAT;
    extent = VkExtent3D{.width = p_spot_light.shadow_map_resolution, .height = p_spot_light.shadow_map_resolution, .depth = 1};
    usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::shadow_map_color(const ComponentSpotLight& p_spot_light) {
    image_type = VK_IMAGE_TYPE_2D;
    format = SHADOW_MAP_COLOR_FORMAT;
    extent = VkExtent3D{.width = p_spot_light.shadow_map_resolution, .height = p_spot_light.shadow_map_resolution, .depth = 1};
    usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::rgb() {
    format = VK_FORMAT_R8G8B8_UNORM;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::srgb() {
    format = VK_FORMAT_R8G8B8_SRGB;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::rgba() {
    format = VK_FORMAT_R8G8B8A8_UNORM;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::srgba() {
    format = VK_FORMAT_R8G8B8A8_SRGB;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::rgb_hdr() {
    format = VK_FORMAT_R32G32B32_SFLOAT;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::rgba_hdr() {
    format = VK_FORMAT_R32G32B32A32_SFLOAT;
    return *this;
}

void ROImageVK::MemoryBarrierBuilder::apply_barrier(VkCommandBuffer p_command_buffer) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = src_stage_mask;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstStageMask = dst_stage_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = WBE_REQUIRED(image).get_image();
    barrier.subresourceRange = subresource_range;
    VkDependencyInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    info.imageMemoryBarrierCount = 1;
    info.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(p_command_buffer, &info);
    // Keep later graph barriers aligned with the layout this command buffer will establish.
    image->exposed_fileds.image_layout = new_layout;
}

ROImageVK::ROImageVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder) : RenderObjectVK(p_render_data) {
    init(p_builder);
}

ROImageVK::ROImageVK(const BaseRenderDataVK* p_render_data, VkImage p_image, VkImageLayout p_initial_image_layout)
    : RenderObjectVK(p_render_data) {
    exposed_fileds.image = p_image;
    p_image = VK_NULL_HANDLE;
    exposed_fileds.image_memory = VK_NULL_HANDLE;
    allocation = VK_NULL_HANDLE;
    exposed_fileds.image_layout = p_initial_image_layout;
    exposed_fileds.is_swap_chain_image = true;
}

ROImageVK::~ROImageVK() {
    // Deallocate resource after frames in flight.
    if (!exposed_fileds.is_swap_chain_image && (exposed_fileds.image != VK_NULL_HANDLE || allocation != VK_NULL_HANDLE)) {
        render_data->register_delayed_call(
            [render_data = render_data, image = exposed_fileds.image, allocation = allocation]() {
                vmaDestroyImage(render_data->vma_allocator, image, allocation);
            },
            render_data->frames_in_flight);
    }
}

void ROImageVK::init(const Builder& p_builder) {
    exposed_fileds.queue_families = p_builder.queue_families;
    VkImageCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.imageType = WBE_REQUIRED(p_builder.image_type);
    create_info.format = WBE_REQUIRED(p_builder.format);
    create_info.extent = WBE_REQUIRED(p_builder.extent);
    create_info.mipLevels = p_builder.mipmap_level;
    create_info.arrayLayers = p_builder.layers_count;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.usage = WBE_REQUIRED(p_builder.usage);
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    Vector<uint32_t, RenderAllocator> queue_families(render_data->allocator);
    if (p_builder.queue_families.empty()) {
        queue_families = Vector<uint32_t, RenderAllocator>(
            render_data->queue_family_indices.begin(), render_data->queue_family_indices.end(), render_data->allocator);
    } else {
        queue_families = Vector<uint32_t, RenderAllocator>(
            p_builder.queue_families.begin(), p_builder.queue_families.end(), render_data->allocator);
    }
    if (queue_families.size() > 1) {
        create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.pQueueFamilyIndices = queue_families.data();
        create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_families.size());
    } else {
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo allocation_create_info{
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    VmaAllocationInfo allocation_info{};
    VkResult result = vmaCreateImage(
        render_data->vma_allocator, &create_info, &allocation_create_info, &exposed_fileds.image, &allocation, &allocation_info);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to create Vulkan image: {}.", string_VkResult(result)));
    }
    exposed_fileds.image_memory = allocation_info.deviceMemory;
}

void ROImageVK::set_image_data_immediate(VkCommandBuffer p_command_buffer, Ref<ROBufferVK> p_data, VkBufferImageCopy p_region_info) {
    VkImageSubresourceRange subresource_range{.aspectMask = p_region_info.imageSubresource.aspectMask,
        .baseMipLevel = p_region_info.imageSubresource.mipLevel,
        .levelCount = 1,
        .baseArrayLayer = p_region_info.imageSubresource.baseArrayLayer,
        .layerCount = p_region_info.imageSubresource.layerCount};
    auto apply_upload_barrier = [this, p_command_buffer, subresource_range](VkAccessFlags2 p_src_access, VkAccessFlags2 p_dst_access) {
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.srcAccessMask = p_src_access;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.dstAccessMask = p_dst_access;
        barrier.oldLayout = exposed_fileds.image_layout;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = exposed_fileds.image;
        barrier.subresourceRange = subresource_range;
        VkDependencyInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        info.imageMemoryBarrierCount = 1;
        info.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(p_command_buffer, &info);
        exposed_fileds.image_layout = VK_IMAGE_LAYOUT_GENERAL;
    };
    apply_upload_barrier(VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    vkCmdCopyBufferToImage(p_command_buffer, p_data->get_buffer(), exposed_fileds.image, VK_IMAGE_LAYOUT_GENERAL, 1, &p_region_info);
    // Make the uploaded texels visible to shader sampling before the image is used by a draw.
    apply_upload_barrier(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

} // namespace WhiteBirdEngine
