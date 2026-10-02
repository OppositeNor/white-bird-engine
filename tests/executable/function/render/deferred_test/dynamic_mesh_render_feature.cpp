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
#include "dynamic_mesh_render_feature.hh"
#include "../render_test.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/i_bindable_render_resource_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

DynamicMeshRenderFeature::DynamicMeshRenderFeature(
    RendererVK* p_renderer, RefRes<RUMesh3D> p_mesh, RefRes<RUGraphicsPipeline> p_material_pipeline, size_t p_entity_count)
    : render_data(p_renderer->get_base_render_data()) {
    if (p_entity_count == 0) {
        throw std::runtime_error("Dynamic mesh feature requires at least one entity.");
    }
    resources = create_mesh_resources(p_renderer, p_mesh, p_material_pipeline, MEM_NULL, p_material_pipeline);
    entities.reserve(p_entity_count);
    entity_keys.reserve(p_entity_count);
    for (size_t i = 0; i < p_entity_count; ++i) {
        entities.push_back(make_unique<EntityDrawData>(render_data->allocator, render_data, resources));
        entity_keys.push_back(std::format("{}_{}", DYNAMIC_MESH_ENTITY_KEY, i));
    }
}

void DynamicMeshRenderFeature::register_resources(RendGraphDescVK& p_graph) const {
    p_graph.add_resource(DYNAMIC_MESH_KEY, *resources);
    for (size_t i = 0; i < entities.size(); ++i) {
        p_graph.add_resource(entity_keys[i], *entities[i]);
    }
}

void DynamicMeshRenderFeature::set_transform(size_t p_entity_index, const glm::mat4& p_transform) {
    if (p_entity_index >= entities.size()) {
        throw std::runtime_error("Dynamic mesh entity index is out of range.");
    }
    entities[p_entity_index]->set_transform(p_transform);
}

RendTaskDescVK DynamicMeshRenderFeature::make_task(std::string_view p_task_id,
    size_t p_entity_index,
    Ref<ROGraphicsPipelineVK> p_pipeline,
    VkExtent2D p_extent,
    std::span<const std::string_view> p_colors,
    std::optional<std::string_view> p_depth) const {
    if (p_entity_index >= entities.size()) {
        throw std::runtime_error("Dynamic mesh entity index is out of range.");
    }
    return make_mesh_task_desc(render_data->allocator,
        render_data,
        p_task_id,
        resources,
        std::move(p_pipeline),
        false,
        {},
        p_extent,
        p_colors,
        p_depth,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        false,
        DYNAMIC_MESH_KEY,
        IBindableRenderResourceVK::make_member_resource_id(entity_keys[p_entity_index], EntityDrawData::TRANSFORM));
}

} // namespace WhiteBirdEngine
