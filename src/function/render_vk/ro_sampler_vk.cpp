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
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROSamplerVK::Builder& ROSamplerVK::Builder::set_filter(VkFilter p_min_filter, VkFilter p_mag_filter) {
    min_filter = p_min_filter;
    mag_filter = p_mag_filter;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_mag_filter(VkFilter p_mag_filter) {
    mag_filter = p_mag_filter;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_min_filter(VkFilter p_min_filter) {
    min_filter = p_min_filter;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_mipmap_mode(VkSamplerMipmapMode p_mipmap_mode) {
    mipmap_mode = p_mipmap_mode;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_address_mode(VkSamplerAddressMode p_address_mode) {
    return set_address_mode(p_address_mode, p_address_mode, p_address_mode);
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_address_mode(
    VkSamplerAddressMode p_address_mode_u, VkSamplerAddressMode p_address_mode_v, VkSamplerAddressMode p_address_mode_w) {
    address_mode_u = p_address_mode_u;
    address_mode_v = p_address_mode_v;
    address_mode_w = p_address_mode_w;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_address_mode_u(VkSamplerAddressMode p_address_mode_u) {
    address_mode_u = p_address_mode_u;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_address_mode_v(VkSamplerAddressMode p_address_mode_v) {
    address_mode_v = p_address_mode_v;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_address_mode_w(VkSamplerAddressMode p_address_mode_w) {
    address_mode_w = p_address_mode_w;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_mip_lod_bias(float p_mip_lod_bias) {
    mip_lod_bias = p_mip_lod_bias;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_anisotropy(VkBool32 p_anisotropy_enable, float p_max_anisotropy) {
    anisotropy_enable = p_anisotropy_enable;
    max_anisotropy = p_max_anisotropy;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_anisotropy_enable(VkBool32 p_anisotropy_enable) {
    anisotropy_enable = p_anisotropy_enable;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_max_anisotropy(float p_max_anisotropy) {
    max_anisotropy = p_max_anisotropy;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_compare(VkBool32 p_compare_enable, VkCompareOp p_compare_op) {
    compare_enable = p_compare_enable;
    compare_op = p_compare_op;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_compare_enable(VkBool32 p_compare_enable) {
    compare_enable = p_compare_enable;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_compare_op(VkCompareOp p_compare_op) {
    compare_op = p_compare_op;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_lod_range(float p_min_lod, float p_max_lod) {
    min_lod = p_min_lod;
    max_lod = p_max_lod;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_min_lod(float p_min_lod) {
    min_lod = p_min_lod;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_max_lod(float p_max_lod) {
    max_lod = p_max_lod;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_border_color(VkBorderColor p_border_color) {
    border_color = p_border_color;
    return *this;
}

ROSamplerVK::Builder& ROSamplerVK::Builder::set_unnormalized_coordinates(VkBool32 p_unnormalized_coordinates) {
    unnormalized_coordinates = p_unnormalized_coordinates;
    return *this;
}

VkSampler ROSamplerVK::Builder::create_vk_object(const BaseRenderDataVK* p_render_data) const {
    VkSamplerCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    create_info.magFilter = mag_filter;
    create_info.minFilter = min_filter;
    create_info.mipmapMode = mipmap_mode;
    create_info.addressModeU = address_mode_u;
    create_info.addressModeV = address_mode_v;
    create_info.addressModeW = address_mode_w;
    create_info.mipLodBias = mip_lod_bias;
    create_info.anisotropyEnable = anisotropy_enable;
    create_info.maxAnisotropy = max_anisotropy;
    create_info.compareEnable = compare_enable;
    create_info.compareOp = compare_op;
    create_info.minLod = min_lod;
    create_info.maxLod = max_lod;
    create_info.borderColor = border_color;
    create_info.unnormalizedCoordinates = unnormalized_coordinates;

    VkSampler result = VK_NULL_HANDLE;
    if (vkCreateSampler(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan sampler.");
    }
    return result;
}

ROSamplerVK::ROSamplerVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder) : RenderObjectVK(p_render_data) {
    sampler = p_builder.create_vk_object(p_render_data);
}

ROSamplerVK::~ROSamplerVK() {
    render_data->register_delayed_call(
        [render_data = render_data, sampler = sampler]() { vkDestroySampler(render_data->logic_device, sampler, render_data->allocation_callbacks); },
        render_data->frames_in_flight);
}

} // namespace WhiteBirdEngine