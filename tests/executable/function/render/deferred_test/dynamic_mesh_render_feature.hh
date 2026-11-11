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
#ifndef WBE_FILE_DYNAMIC_MESH_RENDER_FEATURE_HH
#define WBE_FILE_DYNAMIC_MESH_RENDER_FEATURE_HH

#include "../entity_draw_data.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/mesh_resources_vk.hh"
#include "function/render_vk/rend_graph_recipe_builder_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
class RendererVK;

/**
 * @class DynamicMeshRenderFeature
 * @brief Draws several moving entities of one mesh compiled once by MeshCompilerVK. Neither copyable nor movable.
 *
 * Recipes register the mesh, named by get_mesh_key(), on each render graph under DYNAMIC_MESH_KEY, which their
 * graphics_draw_mesh tasks import it under; entity i is imported under `<DYNAMIC_MESH_ENTITY_KEY>_<i>` (get_entity_key(),
 * supplied by add_imports()) and needs its own draw task per pass. All methods run on the rendering thread. Geometry
 * requires reconstruction to change.
 */
class DynamicMeshRenderFeature {
public:
    DynamicMeshRenderFeature() = delete;
    /**
     * @brief Compile the mesh resource p_mesh_key, placing p_entity_count entities at identity.
     * @throws std::runtime_error If p_entity_count is zero or compilation fails.
     */
    DynamicMeshRenderFeature(RendererVK* p_renderer, std::string_view p_mesh_key, size_t p_entity_count);
    ~DynamicMeshRenderFeature() = default;
    DynamicMeshRenderFeature(const DynamicMeshRenderFeature&) = delete;
    DynamicMeshRenderFeature(DynamicMeshRenderFeature&&) = delete;
    DynamicMeshRenderFeature& operator=(const DynamicMeshRenderFeature&) = delete;
    DynamicMeshRenderFeature& operator=(DynamicMeshRenderFeature&&) = delete;

    /** @brief Return the number of drawn entities. */
    size_t get_entity_count() const {
        return entities.size();
    }

    /** @brief Return the resource key of the drawn mesh. */
    std::string_view get_mesh_key() const {
        return mesh_key;
    }

    /**
     * @brief Supply every entity to a recipe, under its get_entity_key(); the draw tasks import the mesh.
     */
    void add_imports(RendGraphRecipeImportsVK& p_imports) const;

    /**
     * @brief Get the import name of an entity.
     * @throws std::runtime_error If p_entity_index is out of range.
     */
    std::string_view get_entity_key(size_t p_entity_index) const;

    /**
     * @brief Place one entity; every frame slot applies the latest transform.
     * @throws std::runtime_error If p_entity_index is out of range or the transform is invalid.
     */
    void set_transform(size_t p_entity_index, const glm::mat4& p_transform);

private:
    const RenderContextVK* render_data;
    std::string mesh_key;
    Ref<MeshResourcesVK> resources;
    std::vector<Unique<EntityDrawData>> entities;
    std::vector<std::string> entity_keys;
};

} // namespace WhiteBirdEngine
#endif
