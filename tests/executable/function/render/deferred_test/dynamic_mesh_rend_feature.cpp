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
#include "dynamic_mesh_rend_feature.hh"
#include "../rend_test.hh"
#include "../entity_draw_data.hh"
#include "core/alloc/i_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/rend_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/rend_vk/i_bindable_rend_resource_vk.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/mesh_compiler_vk.hh"
#include "function/rend_vk/mesh_resources_vk.hh"
#include "function/rend_vk/rend_graph_recipe_builder_vk.hh"
#include "function/rend_vk/renderer_vk.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

DynamicMeshRendFeature::DynamicMeshRendFeature(RendererVK* p_renderer, std::string_view p_mesh_key, size_t p_entity_count)
    : rend_data(p_renderer->get_rend_context()), mesh_key(p_mesh_key) {
    if (p_entity_count == 0) {
        throw std::runtime_error("Dynamic mesh feature requires at least one entity.");
    }
    // The entities need the mesh before the graphs exist; the recipes register the same cached compilation.
    resources = MeshCompilerVK(p_renderer).acquire(mesh_key);
    entities.reserve(p_entity_count);
    entity_keys.reserve(p_entity_count);
    for (size_t i = 0; i < p_entity_count; ++i) {
        entities.push_back(make_unique<EntityDrawData>(rend_data->alloc, rend_data, resources));
        entity_keys.push_back(std::format("{}_{}", DYNAMIC_MESH_ENTITY_KEY, i));
    }
}

void DynamicMeshRendFeature::add_imports(RendGraphRecipeImportsVK& p_imports) const {
    for (size_t i = 0; i < entities.size(); ++i) {
        p_imports.add(entity_keys[i], *entities[i]);
    }
}

std::string_view DynamicMeshRendFeature::get_entity_key(size_t p_entity_index) const {
    if (p_entity_index >= entities.size()) {
        throw std::runtime_error("Dynamic mesh entity index is out of range.");
    }
    return entity_keys[p_entity_index];
}

void DynamicMeshRendFeature::set_transform(size_t p_entity_index, const glm::mat4& p_transform) {
    if (p_entity_index >= entities.size()) {
        throw std::runtime_error("Dynamic mesh entity index is out of range.");
    }
    entities[p_entity_index]->set_transform(p_transform);
}

} // namespace WhiteBirdEngine
