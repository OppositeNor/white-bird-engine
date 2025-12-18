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
#include "function/function_utils.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <vulkan/vulkan_raii.hpp>
#include <cstdint>
#include <stdexcept>
#include <vector>
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

ROImageVK::Builder& ROImageVK::Builder::set_queue_families(const std::set<uint32_t>& p_queue_family_indices) {
    queue_family_indices = p_queue_family_indices;
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::image_2d(glm::uvec2 p_size) {
    image_type = VK_IMAGE_TYPE_2D;
    extent = VkExtent3D{ .width = p_size.x, .height = p_size.y };
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::image_2d(VkExtent2D p_extent) {
    image_type = VK_IMAGE_TYPE_2D;
    extent = {.width = p_extent.width, .height = p_extent.height};
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::image_3d(glm::uvec3 p_size) {
    image_type = VK_IMAGE_TYPE_3D;
    extent = VkExtent3D{ .width = p_size.x, .height = p_size.y, .depth = p_size.z };
    return *this;
}

ROImageVK::Builder& ROImageVK::Builder::image_3d(VkExtent3D p_extent) {
    image_type = VK_IMAGE_TYPE_3D;
    extent = p_extent;
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

ROImageVK::Builder& ROImageVK::Builder::set_initial_layout(VkImageLayout p_initial_layout) {
    initial_image_layout = p_initial_layout;
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
    barrier.newLayout = WBE_REQUIRED(new_layout);
    barrier.srcQueueFamilyIndex = src_queue_family_index;
    barrier.dstQueueFamilyIndex = dst_queue_family_index;
    barrier.image = WBE_REQUIRED(image).get_image();
    barrier.subresourceRange = subresource_range;
    VkDependencyInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    info.imageMemoryBarrierCount = 1;
    info.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(p_command_buffer, &info);
    // Update the image's layout to new layout.
    image->exposed_fileds.image_layout = *new_layout;
}

VkImage ROImageVK::Builder::build(const BaseRenderDataVK* p_render_data) const {
    VkImageCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.imageType = WBE_REQUIRED(image_type);
    create_info.format = WBE_REQUIRED(format);
    create_info.extent = WBE_REQUIRED(extent);
    create_info.mipLevels = mipmap_level;
    create_info.arrayLayers = layers_count;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.usage = WBE_REQUIRED(usage);
    create_info.initialLayout = initial_image_layout;
    WBE_DEBUG_ASSERT(queue_family_indices.size() >= 1);
    if (queue_family_indices.size() == 1) {
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else {
        create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = queue_family_indices.size();
        std::vector<uint32_t> queue_family_indices_array(queue_family_indices.begin(), queue_family_indices.end());
        create_info.pQueueFamilyIndices = queue_family_indices_array.data();
    }
    VkImage result;
    if (vkCreateImage(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan image.");
    }
    return result;
}

ROImageVK::ROImageVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Render data is nullptr.");
    }
    init(p_builder);
}

ROImageVK::~ROImageVK() {
    // Deallocate resource after frames in flight.
    render_data->register_delayed_call([render_data = render_data, image = exposed_fileds.image, image_memory = exposed_fileds.image_memory]() {
        vkDestroyImage(render_data->logic_device, image, render_data->allocation_callbacks);
        vkFreeMemory(render_data->logic_device, image_memory, render_data->allocation_callbacks);
    }, render_data->frames_in_flight);
}

void ROImageVK::init(const Builder& p_builder) {
    exposed_fileds.image = p_builder.build(render_data);
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(render_data->logic_device, exposed_fileds.image, &memory_requirements);
    // Images are only allowed to be allocated on device.
    exposed_fileds.image_memory = allocate_device_memory(memory_requirements, DeviceDomain::DEVICE);
    exposed_fileds.image_layout = p_builder.initial_image_layout;
    if (vkBindImageMemory(render_data->logic_device, exposed_fileds.image, exposed_fileds.image_memory, 0) != VK_SUCCESS) {
        throw std::runtime_error("Failed to bind image memory.");
    }
}

void ROImageVK::set_image_data_immediate(VkCommandBuffer p_command_buffer, Ref<ROBufferVK> p_data, VkBufferImageCopy p_region_info) {
    MemoryBarrierBuilder barrier_builder;
    VkImageLayout old_layout = exposed_fileds.image_layout;
    barrier_builder.set_image(this_ref.lock()).set_access(VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT)
        .set_new_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL).apply_barrier(p_command_buffer);
    vkCmdCopyBufferToImage(p_command_buffer, p_data->get_buffer(), exposed_fileds.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &p_region_info);
    // After transfering the data, retrieve the original layout.
    barrier_builder.set_image(this_ref.lock()).set_access(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT)
        .set_new_layout(old_layout).apply_barrier(p_command_buffer);
}

} // namespace WhiteBirdEngine
