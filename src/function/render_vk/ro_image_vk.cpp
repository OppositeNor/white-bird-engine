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
#include "function/render_vk/render_vk_utils.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
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
    vkDestroyImage(render_data->logic_device, image, render_data->allocation_callbacks);
}

void ROImageVK::init(const Builder& p_builder) {
    image = p_builder.build(render_data);
    // TODO: allocate memory for image.
}

} // namespace WhiteBirdEngine
