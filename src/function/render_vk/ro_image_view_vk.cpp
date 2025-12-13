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
#include "function/render_vk/ro_image_view_vk.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROImageViewVK::Builder& ROImageViewVK::Builder::set_image(Ref<ROImageVK> p_image) {
    image_obj = p_image;
    image = WBE_REQUIRES_VALID(p_image)->get_image();
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_image(VkImage p_image) {
    image = p_image;
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_view_type(VkImageViewType p_view_type) {
    view_type = p_view_type;
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_format(VkFormat p_format) {
    format = p_format;
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_components(const VkComponentMapping& p_components) {
    components = p_components;
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_components(VkComponentSwizzle p_r, VkComponentSwizzle p_g, VkComponentSwizzle p_b, VkComponentSwizzle p_a) {
    components = {.r = p_r, .g = p_g, .b = p_b, .a = p_a};
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_subresource_range(const VkImageSubresourceRange& p_subresource_range) {
    subresource_range = p_subresource_range;
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_from_image_config(const ROImageVK::Builder& p_image_config) {
    switch (WBE_REQUIRED(p_image_config.image_type)) {
        case VK_IMAGE_TYPE_1D:
            view_type = VK_IMAGE_VIEW_TYPE_1D;
            break;
        case VK_IMAGE_TYPE_2D:
            view_type = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case VK_IMAGE_TYPE_3D:
            view_type = VK_IMAGE_VIEW_TYPE_3D;
            break;
        default:
            break;
    };
    format = WBE_REQUIRED(p_image_config.format);
    return *this;
}

VkImageView ROImageViewVK::Builder::build(const BaseRenderDataVK* p_render_data) const {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = WBE_REQUIRED(image);
    create_info.viewType = WBE_REQUIRED(view_type);
    create_info.format = WBE_REQUIRED(format);
    create_info.components = components;
    create_info.subresourceRange = subresource_range;
    
    VkImageView result;
    if (vkCreateImageView(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view.");
    }
    return result;
}

ROImageViewVK::ROImageViewVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Render data is nullptr.");
    }
    init(p_builder);
}

ROImageViewVK::~ROImageViewVK() {
    vkDestroyImageView(render_data->logic_device, image_view, render_data->allocation_callbacks);
}

void ROImageViewVK::init(const Builder& p_builder) {
    image_view = p_builder.build(render_data);
}

} // namespace WhiteBirdEngine


