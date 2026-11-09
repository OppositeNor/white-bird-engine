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
#include "function/render_vk/texture_compiler_vk.hh"
#include "core/memory/reference_raw.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/global_render_cache.hh"
#include "function/render_vk/ktx_vulkan_device_info_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "ktxvulkan.h"
#include "resource/resource_unit/resource_unit.hh"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TextureCompilerVK::TextureCompilerVK(const RenderContextVK* p_render_data, GlobalRenderCache* p_render_cache)
    : render_data(p_render_data), render_cache(p_render_cache) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Failed to create texture compiler: the render data is null.");
    }
    if (p_render_cache == nullptr) {
        throw std::runtime_error("Failed to create texture compiler: the global render cache is null.");
    }
}

Ref<ROImageViewVK> TextureCompilerVK::compile_texture_view(std::string_view p_resource_key, RefRes<RUTexture> p_texture_resource) const {
    const std::string view_key = std::format("{}#image_view", p_resource_key);
    return render_cache->acquire<ROImageViewVK>(view_key, [this, p_resource_key, p_texture_resource] {
        Ref<ROImageVK> image = compile_texture(p_resource_key, p_texture_resource);
        const ROImageVK::ExposedFields& image_fields = image->fields();
        return render_data->create_image_view(ROImageViewVK::Builder()
                .set_image(image)
                .set_view_type(image_fields.view_type)
                .set_format(image_fields.format)
                .set_subresource_range(VkImageSubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = image_fields.mipmap_levels,
                    .baseArrayLayer = 0,
                    .layerCount = image_fields.layers_count}));
    });
}

Ref<ROImageVK> TextureCompilerVK::compile_texture(std::string_view p_resource_key, RefRes<RUTexture> p_texture_resource) const {
    return render_cache->acquire<ROImageVK>(p_resource_key, [this, p_resource_key, p_texture_resource] {
        if (p_texture_resource == nullptr) {
            throw std::runtime_error(std::format("Failed to compile texture '{}': the texture resource is null.", p_resource_key));
        }
        if (render_data->ktx_vulkan_device_info == nullptr) {
            throw std::runtime_error("Failed to compile texture: libktx Vulkan device info is unavailable.");
        }
        ktxVulkanTexture uploaded_texture = render_data->ktx_vulkan_device_info->upload(p_texture_resource->texture.get());
        return make_ref<ROImageVK>(render_data->allocator, render_data, uploaded_texture);
    });
}

} // namespace WhiteBirdEngine
