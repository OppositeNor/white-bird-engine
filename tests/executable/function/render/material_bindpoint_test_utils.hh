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
#ifndef WBE_FILE_MATERIAL_BINDPOINT_TEST_UTILS_HH
#define WBE_FILE_MATERIAL_BINDPOINT_TEST_UTILS_HH

#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {

inline constexpr uint32_t MATERIAL_TEXTURE_DESCRIPTOR_COUNT = 32;
inline constexpr int32_t MATERIAL_TEXTURE_DEFAULT_INDEX = 0;
inline constexpr int32_t MATERIAL_TEXTURE_BOUND_INDEX = 1;

struct MaterialParamsUBO {
    int32_t albedo_index = -1;
    int32_t normal_index = -1;
    int32_t roughness_metallic_ao_index = -1;
};

inline RefRes<RUImage> find_material_texture(RefRes<RUMaterial> p_material, std::string_view p_texture_key) {
    if (p_material == nullptr) {
        return MEM_NULL;
    }
    std::span<RUMaterial::TextureSlot> texture_slots(p_material->textures.get(), p_material->texture_count);
    for (const auto& texture_slot : texture_slots) {
        if (texture_slot.texture_key == nullptr || texture_slot.texture_key->text_data == nullptr) {
            continue;
        }
        if (std::string_view(texture_slot.texture_key->text_data.get()) == p_texture_key) {
            return texture_slot.texture;
        }
    }
    return MEM_NULL;
}

inline RefRes<RUImage> require_material_texture(RefRes<RUMaterial> p_material, std::string_view p_texture_key) {
    RefRes<RUImage> texture = find_material_texture(p_material, p_texture_key);
    if (texture == nullptr) {
        throw std::runtime_error(std::format("Failed to bind test material textures: default material is missing '{}' texture.", p_texture_key));
    }
    return texture;
}

inline void add_material_texture_family_bindings(RenderBindpointDeclarationsBuilder& p_builder,
    std::string_view p_texture_name,
    RefRes<RUImage> p_default_texture,
    RefRes<RUImage> p_material_texture,
    Ref<ROSamplerVK> p_sampler) {
    p_builder.add_image_decleration(std::format("material_textures.{}[{}]", p_texture_name, MATERIAL_TEXTURE_DEFAULT_INDEX), p_default_texture)
        .add_sampler_decleration(std::format("material_textures.{}_sampler[{}]", p_texture_name, MATERIAL_TEXTURE_DEFAULT_INDEX), p_sampler);
    if (p_material_texture != nullptr) {
        p_builder.add_image_decleration(std::format("material_textures.{}[{}]", p_texture_name, MATERIAL_TEXTURE_BOUND_INDEX), p_material_texture)
            .add_sampler_decleration(std::format("material_textures.{}_sampler[{}]", p_texture_name, MATERIAL_TEXTURE_BOUND_INDEX), p_sampler);
    }
}

inline std::vector<uint8_t> add_material_texture_bindings(RenderBindpointDeclarationsBuilder& p_builder,
    RefRes<RUMaterial> p_default_material,
    RefRes<RUMaterial> p_material,
    Ref<ROSamplerVK> p_sampler) {
    MaterialParamsUBO material_params;

    RefRes<RUImage> default_albedo = require_material_texture(p_default_material, "albedo");
    RefRes<RUImage> default_normal = require_material_texture(p_default_material, "normal");
    RefRes<RUImage> default_rma = require_material_texture(p_default_material, "roughness_metallic_ao");

    RefRes<RUImage> albedo_texture = find_material_texture(p_material, "albedo");
    RefRes<RUImage> normal_texture = find_material_texture(p_material, "normal");
    RefRes<RUImage> rma_texture = find_material_texture(p_material, "roughness_metallic_ao");

    add_material_texture_family_bindings(p_builder, "albedo", default_albedo, albedo_texture, p_sampler);
    add_material_texture_family_bindings(p_builder, "normal", default_normal, normal_texture, p_sampler);
    add_material_texture_family_bindings(p_builder, "roughness_metallic_ao", default_rma, rma_texture, p_sampler);

    material_params.albedo_index = albedo_texture != nullptr ? MATERIAL_TEXTURE_BOUND_INDEX : -1;
    material_params.normal_index = normal_texture != nullptr ? MATERIAL_TEXTURE_BOUND_INDEX : -1;
    material_params.roughness_metallic_ao_index = rma_texture != nullptr ? MATERIAL_TEXTURE_BOUND_INDEX : -1;

    std::vector<uint8_t> result(sizeof(MaterialParamsUBO));
    std::memcpy(result.data(), &material_params, result.size());
    return result;
}

} // namespace WhiteBirdEngine

#endif
