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
#include "core/memory/reference_strong.hh"
#include "function/function_utils.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <glm/glm.hpp>
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

ROImageVK::Builder& ROImageVK::Builder::set_queue_families(const std::set<uint32_t>& p_queue_families) {
    queue_families = p_queue_families;
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

void ROImageVK::MemoryBarrierBuilder::apply_barrier(VkCommandBuffer p_command_buffer) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = src_stage_mask;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstStageMask = dst_stage_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = WBE_REQUIRED(image).get_image();
    barrier.subresourceRange = subresource_range;
    VkDependencyInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    info.imageMemoryBarrierCount = 1;
    info.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(p_command_buffer, &info);
    // Update image's internal state.
    image->exposed_fileds.image_layout = VK_IMAGE_LAYOUT_GENERAL;
}

VkImage ROImageVK::Builder::create_vk_object(const BaseRenderDataVK* p_render_data) const {
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
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    std::vector<uint32_t> initial_queues(queue_families.begin(), queue_families.end());
    if (initial_queues.size() > 1) {
        create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.pQueueFamilyIndices = initial_queues.data();
        create_info.queueFamilyIndexCount = static_cast<uint32_t>(initial_queues.size());
    } else {
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    VkImage result;
    if (vkCreateImage(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan image.");
    }
    return result;
}

ROImageVK::ROImageVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data) {
    init(p_builder);
}

ROImageVK::ROImageVK(const BaseRenderDataVK* p_render_data, VkImage p_image, VkImageLayout p_initial_image_layout)
    : RenderObjectVK(p_render_data) {
    exposed_fileds.image = p_image;
    p_image = VK_NULL_HANDLE;
    exposed_fileds.image_memory = VK_NULL_HANDLE;
    exposed_fileds.image_layout = p_initial_image_layout;
    exposed_fileds.is_swap_chain_image = true;
}

ROImageVK::~ROImageVK() {
    // Deallocate resource after frames in flight.
    if (!exposed_fileds.is_swap_chain_image) {
        render_data->register_delayed_call([render_data = render_data, image = exposed_fileds.image, image_memory = exposed_fileds.image_memory]() {
            vkDestroyImage(render_data->logic_device, image, render_data->allocation_callbacks);
            vkFreeMemory(render_data->logic_device, image_memory, render_data->allocation_callbacks);
        }, render_data->frames_in_flight);
    }
}

void ROImageVK::init(const Builder& p_builder) {
    exposed_fileds.image = p_builder.create_vk_object(render_data);
    exposed_fileds.queue_families = p_builder.queue_families;
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(render_data->logic_device, exposed_fileds.image, &memory_requirements);
    // Images are only allowed to be allocated on device.
    exposed_fileds.image_memory = allocate_device_memory(memory_requirements, DeviceDomain::DEVICE);
}

void ROImageVK::set_image_data_immediate(VkCommandBuffer p_command_buffer, Ref<ROBufferVK> p_data, VkBufferImageCopy p_region_info) {
    MemoryBarrierBuilder barrier_builder;
    barrier_builder.set_image(this_ref.lock()).set_access(VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT)
        .apply_barrier(p_command_buffer);
    vkCmdCopyBufferToImage(p_command_buffer, p_data->get_buffer(), exposed_fileds.image, VK_IMAGE_LAYOUT_GENERAL, 1, &p_region_info);
    // After transfering the data, retrieve the original layout.
    barrier_builder.set_image(this_ref.lock()).set_access(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT)
        .apply_barrier(p_command_buffer);
}

} // namespace WhiteBirdEngine
