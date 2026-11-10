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
#ifndef WBE_FILE_SHADOWED_MESH_RECIPE_CONTEXT_HH
#define WBE_FILE_SHADOWED_MESH_RECIPE_CONTEXT_HH

#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/rend_recipe_context_vk.hh"
#include "function/render_vk/rend_recipe_extent_2d_vk.hh"
#include "function/render_vk/vulkan_enums_vk.hh"

namespace WhiteBirdEngine {

/**
 * @class ShadowedMeshRecipeContext
 * @brief The context of the forward-shaded recipes with one shadow-casting spot light, render_graphs.draw_cube and
 * render_graphs.draw_sponza. Copyable and movable.
 */
struct WBE_META(WBE_REFLECT_SCRIPT) ShadowedMeshRecipeContext : public RendRecipeContextVK {
    /**
     * @brief The extent of the spot light's shadow map.
     */
    WBE_META(WBE_REFLECT)
    RendRecipeExtent2DVK shadow_extent;
    /**
     * @brief The format of the shadow map's color target.
     */
    WBE_META(WBE_REFLECT)
    ImageFormatVK shadow_color_format = ImageFormatVK::UNDEFINED;
    /**
     * @brief The format of the shadow map's depth target.
     */
    WBE_META(WBE_REFLECT)
    ImageFormatVK shadow_depth_format = ImageFormatVK::UNDEFINED;
    /**
     * @brief The format of the main pass's depth target, for recipes that depth-test the main pass.
     */
    WBE_META(WBE_REFLECT)
    ImageFormatVK depth_format = ImageFormatVK::UNDEFINED;
};

} // namespace WhiteBirdEngine

#endif
