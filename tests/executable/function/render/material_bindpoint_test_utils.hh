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
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {

inline constexpr int32_t MATERIAL_TEXTURE_DEFAULT_ALBEDO_INDEX = 0;
inline constexpr int32_t MATERIAL_TEXTURE_DEFAULT_NORMAL_INDEX = 1;
inline constexpr int32_t MATERIAL_TEXTURE_DEFAULT_ROUGHNESS_METALLIC_AO_INDEX = 2;
inline constexpr int32_t MATERIAL_TEXTURE_BOUND_ALBEDO_INDEX = 3;
inline constexpr int32_t MATERIAL_TEXTURE_BOUND_NORMAL_INDEX = 4;
inline constexpr int32_t MATERIAL_TEXTURE_BOUND_ROUGHNESS_METALLIC_AO_INDEX = 5;

struct MaterialParamsUBO {
    int32_t albedo_index = MATERIAL_TEXTURE_DEFAULT_ALBEDO_INDEX;
    int32_t normal_index = MATERIAL_TEXTURE_DEFAULT_NORMAL_INDEX;
    int32_t roughness_metallic_ao_index = MATERIAL_TEXTURE_DEFAULT_ROUGHNESS_METALLIC_AO_INDEX;
};

inline RefRes<RUImage> require_material_texture(RefRes<RUImage> p_texture, std::string_view p_texture_name) {
    RefRes<RUImage> texture = p_texture;
    if (texture == nullptr) {
        throw std::runtime_error(
            std::format("Failed to bind test material textures: default material is missing '{}' texture.", p_texture_name));
    }
    return texture;
}

inline RefRes<RUImage> find_material_texture(RefRes<RUMaterial> p_material, HashCode p_texture_type) {
    if (p_material == nullptr) {
        return MEM_NULL;
    }
    std::span<const RUMaterial::TextureSlot> texture_slots(p_material->textures.get(), p_material->texture_count);
    for (const RUMaterial::TextureSlot& texture_slot : texture_slots) {
        if (texture_slot.texture_type == p_texture_type) {
            return texture_slot.texture;
        }
    }
    return MEM_NULL;
}

inline void add_material_texture_binding(RenderBindpointDeclarationsBuilder& p_builder,
    int32_t p_default_index,
    int32_t p_bound_index,
    RefRes<RUImage> p_default_texture,
    RefRes<RUImage> p_material_texture) {
    p_builder.add_image_decleration(std::format("material_textures.textures[{}]", p_default_index), p_default_texture);
    if (p_material_texture != nullptr) {
        p_builder.add_image_decleration(std::format("material_textures.textures[{}]", p_bound_index), p_material_texture);
    }
}

inline std::vector<uint8_t> add_material_texture_bindings(RenderBindpointDeclarationsBuilder& p_builder,
    RefRes<RUMaterial> p_default_material,
    RefRes<RUMaterial> p_material,
    Ref<ROSamplerVK> p_sampler) {
    MaterialParamsUBO material_params;

    RefRes<RUImage> default_albedo =
        require_material_texture(find_material_texture(p_default_material, dynam_hash("albedo")), "albedo");
    RefRes<RUImage> default_normal =
        require_material_texture(find_material_texture(p_default_material, dynam_hash("normal")), "normal");
    RefRes<RUImage> default_rma = require_material_texture(find_material_texture(p_default_material, dynam_hash("rma")), "rma");

    RefRes<RUImage> albedo_texture = find_material_texture(p_material, dynam_hash("albedo"));
    RefRes<RUImage> normal_texture = find_material_texture(p_material, dynam_hash("normal"));
    RefRes<RUImage> rma_texture = find_material_texture(p_material, dynam_hash("rma"));

    p_builder.add_sampler_decleration("material_textures.material_sampler", p_sampler);
    add_material_texture_binding(
        p_builder, MATERIAL_TEXTURE_DEFAULT_ALBEDO_INDEX, MATERIAL_TEXTURE_BOUND_ALBEDO_INDEX, default_albedo, albedo_texture);
    add_material_texture_binding(
        p_builder, MATERIAL_TEXTURE_DEFAULT_NORMAL_INDEX, MATERIAL_TEXTURE_BOUND_NORMAL_INDEX, default_normal, normal_texture);
    add_material_texture_binding(
        p_builder, MATERIAL_TEXTURE_DEFAULT_ROUGHNESS_METALLIC_AO_INDEX, MATERIAL_TEXTURE_BOUND_ROUGHNESS_METALLIC_AO_INDEX, default_rma, rma_texture);

    material_params.albedo_index = albedo_texture != nullptr ? MATERIAL_TEXTURE_BOUND_ALBEDO_INDEX : MATERIAL_TEXTURE_DEFAULT_ALBEDO_INDEX;
    material_params.normal_index = normal_texture != nullptr ? MATERIAL_TEXTURE_BOUND_NORMAL_INDEX : MATERIAL_TEXTURE_DEFAULT_NORMAL_INDEX;
    material_params.roughness_metallic_ao_index = rma_texture != nullptr ? MATERIAL_TEXTURE_BOUND_ROUGHNESS_METALLIC_AO_INDEX
                                                                         : MATERIAL_TEXTURE_DEFAULT_ROUGHNESS_METALLIC_AO_INDEX;

    std::vector<uint8_t> result(sizeof(MaterialParamsUBO));
    std::memcpy(result.data(), &material_params, result.size());
    return result;
}

} // namespace WhiteBirdEngine

#endif
