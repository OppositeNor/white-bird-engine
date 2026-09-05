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
#ifndef WBE_FILE_STATIC_SET_MATERIAL_BINDING_BUILDER_VK_HH
#define WBE_FILE_STATIC_SET_MATERIAL_BINDING_BUILDER_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class StaticSetMaterialBindingBuilderVK
 * @brief Builds reflected static-set material parameter and texture bindpoints for one graphics pipeline.
 */
class StaticSetMaterialBindingBuilderVK final {
public:
    /**
     * @brief Construct a stateless builder.
     */
    StaticSetMaterialBindingBuilderVK() = default;
    /**
     * @brief Destroy a stateless builder.
     */
    ~StaticSetMaterialBindingBuilderVK() = default;
    /**
     * @brief Copy a stateless builder.
     */
    StaticSetMaterialBindingBuilderVK(const StaticSetMaterialBindingBuilderVK&) = default;
    /**
     * @brief Move a stateless builder.
     */
    StaticSetMaterialBindingBuilderVK(StaticSetMaterialBindingBuilderVK&&) noexcept = default;
    /**
     * @brief Copy a stateless builder.
     */
    StaticSetMaterialBindingBuilderVK& operator=(const StaticSetMaterialBindingBuilderVK&) = default;
    /**
     * @brief Move a stateless builder.
     */
    StaticSetMaterialBindingBuilderVK& operator=(StaticSetMaterialBindingBuilderVK&&) noexcept = default;

    /**
     * @brief Build bindpoints for one reflected material contract.
     *
     * Returns an empty declaration set when p_material_contract is std::nullopt.
     *
     * @param p_material_contract Reflected material contract for the selected pipeline.
     * @param p_materials Ordered deduplicated material rows referenced by static submeshes.
     * @param p_default_material Default material supplying fallback texture roles.
     * @param p_material_textures_variable_name Reflected global parameter name for texture descriptors.
     * @param p_material_params_variable_name Reflected global parameter name for packed material parameters.
     * @param p_max_texture_descriptor_count Maximum sampled-image descriptors supported for the runtime array.
     * @param p_material_sampler Optional sampler render object bound at material_textures.material_sampler.
     * @throws std::runtime_error When reflected limits are exceeded or material role data is malformed.
     */
    RenderBindpointDeclarations build(const std::optional<MaterialTextureContract>& p_material_contract,
        std::span<const RefRes<RUMaterial>> p_materials,
        RefRes<RUMaterial> p_default_material,
        std::string_view p_material_textures_variable_name,
        std::string_view p_material_params_variable_name,
        uint32_t p_max_texture_descriptor_count,
        Ref<IRenderObject> p_material_sampler = MEM_NULL) const;
};

} // namespace WhiteBirdEngine

#endif