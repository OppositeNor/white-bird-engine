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
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/descriptor_heap_allocator_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/utils.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

DebugData make_descriptor_row(std::string_view p_descriptor_type, uint32_t p_heap_index) {
    DebugData row;
    row.set_value("resource_type", "ROImageViewVK");
    row.set_value("descriptor_type", std::string(p_descriptor_type));
    row.set_value("heap", "resource");
    row.set_value("heap_index", p_heap_index);
    row.set_value("device_address", nullptr);
    return row;
}

} // namespace

TypeID ROImageViewVK::get_type_uuid() const {
    return TypeUUID<ROImageViewVK>::UUID;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_image(Ref<ROImageVK> p_image) {
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

ROImageViewVK::Builder& ROImageViewVK::Builder::set_components(
    VkComponentSwizzle p_r, VkComponentSwizzle p_g, VkComponentSwizzle p_b, VkComponentSwizzle p_a) {
    components = {.r = p_r, .g = p_g, .b = p_b, .a = p_a};
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_subresource_range(const VkImageSubresourceRange& p_subresource_range) {
    subresource_range = p_subresource_range;
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_aspect_mask(VkImageAspectFlags p_aspect_mask) {
    subresource_range.aspectMask = p_aspect_mask;
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_mip_level_count(uint32_t p_level_count) {
    subresource_range.levelCount = p_level_count;
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

ROImageViewVK::Builder& ROImageViewVK::Builder::set_sampled_view_index(uint32_t p_index) {
    sampled_view_index = p_index;
    return *this;
}

ROImageViewVK::Builder& ROImageViewVK::Builder::set_storage_view_index(uint32_t p_index) {
    storage_view_index = p_index;
    return *this;
}

VkImageView ROImageViewVK::Builder::create_vk_object(const BaseRenderDataVK* p_render_data) const {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = WBE_REQUIRED(image).get_image();
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
    : IRenderObjectVK(p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Render data is nullptr.");
    }
    init(p_builder);
}

ROImageViewVK::~ROImageViewVK() {
    // Deallocate resource after frames in flight.
    render_data->register_delayed_call(
        [render_data = render_data, image_view = image_view, sampled_view_index = sampled_view_index, storage_view_index = storage_view_index]() {
            if (sampled_view_index.has_value()) {
                render_data->descriptor_heap_allocator->deallocate_sampled_image(*sampled_view_index);
            }
            if (storage_view_index.has_value()) {
                render_data->descriptor_heap_allocator->deallocate_storage_image(*storage_view_index);
            }
            vkDestroyImageView(render_data->logic_device, image_view, render_data->allocation_callbacks);
        },
        render_data->frames_in_flight);
}

void ROImageViewVK::init(const Builder& p_builder) {
    exposed_fields.image = p_builder.image;
    exposed_fields.subresource_range = p_builder.subresource_range;
    create_info = VkImageViewCreateInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = WBE_REQUIRED(p_builder.image).get_image(),
        .viewType = WBE_REQUIRED(p_builder.view_type),
        .format = WBE_REQUIRED(p_builder.format),
        .components = p_builder.components,
        .subresourceRange = p_builder.subresource_range};
    if (p_builder.sampled_view_index.has_value()) {
        if ((exposed_fields.image->fields().usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0) {
            throw std::runtime_error("Failed to adopt sampled-image descriptor index: image does not have sampled usage.");
        }
        if (render_data->descriptor_heap_allocator == nullptr ||
            !render_data->descriptor_heap_allocator->owns_sampled_image_range(*p_builder.sampled_view_index)) {
            throw std::runtime_error("Failed to adopt sampled-image descriptor index: the index is not allocated by the sampled-image arena.");
        }
    }
    if (p_builder.storage_view_index.has_value()) {
        if ((exposed_fields.image->fields().usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0) {
            throw std::runtime_error("Failed to adopt storage-image descriptor index: image does not have storage usage.");
        }
        if (render_data->descriptor_heap_allocator == nullptr ||
            !render_data->descriptor_heap_allocator->owns_storage_image_range(*p_builder.storage_view_index)) {
            throw std::runtime_error("Failed to adopt storage-image descriptor index: the index is not allocated by the storage-image arena.");
        }
    }
    image_view = p_builder.create_vk_object(render_data);
    if (p_builder.sampled_view_index.has_value()) {
        sampled_view_index = p_builder.sampled_view_index;
        render_data->descriptor_heap_allocator->write_sampled_image_descriptor(*sampled_view_index, create_info, VK_IMAGE_LAYOUT_GENERAL);
    }
    if (p_builder.storage_view_index.has_value()) {
        storage_view_index = p_builder.storage_view_index;
        render_data->descriptor_heap_allocator->write_storage_image_descriptor(*storage_view_index, create_info, VK_IMAGE_LAYOUT_GENERAL);
    }
}

uint32_t ROImageViewVK::get_sampled_view_index() const {
    if ((exposed_fields.image->fields().usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0) {
        throw std::runtime_error("Failed to create sampled-image descriptor: image does not have sampled usage.");
    }
    if (!sampled_view_index.has_value()) {
        DescriptorHeapAllocatorVK* allocator = render_data->descriptor_heap_allocator;
        if (allocator == nullptr) {
            throw std::runtime_error("Failed to create sampled-image descriptor: descriptor heap allocator is unavailable.");
        }
        sampled_view_index = allocator->allocate_sampled_image();
        allocator->write_sampled_image_descriptor(*sampled_view_index, create_info, VK_IMAGE_LAYOUT_GENERAL);
    }
    return *sampled_view_index;
}

uint32_t ROImageViewVK::get_storage_view_index() const {
    if ((exposed_fields.image->fields().usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0) {
        throw std::runtime_error("Failed to create storage-image descriptor: image does not have storage usage.");
    }
    if (!storage_view_index.has_value()) {
        DescriptorHeapAllocatorVK* allocator = render_data->descriptor_heap_allocator;
        if (allocator == nullptr) {
            throw std::runtime_error("Failed to create storage-image descriptor: descriptor heap allocator is unavailable.");
        }
        storage_view_index = allocator->allocate_storage_image();
        allocator->write_storage_image_descriptor(*storage_view_index, create_info, VK_IMAGE_LAYOUT_GENERAL);
    }
    return *storage_view_index;
}

DebugDataRows ROImageViewVK::descriptor_debug_data() const {
    DebugDataRows rows(render_data->allocator);
    if (sampled_view_index.has_value()) {
        rows.emplace_back(make_descriptor_row("sampled_image", *sampled_view_index));
    }
    if (storage_view_index.has_value()) {
        rows.emplace_back(make_descriptor_row("storage_image", *storage_view_index));
    }
    return rows;
}

} // namespace WhiteBirdEngine
