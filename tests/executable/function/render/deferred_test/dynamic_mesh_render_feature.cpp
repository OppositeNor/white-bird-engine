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
#include "function/render_vk/dynamic_mesh_compiler_vk.hh"
#include "function/render_vk/dynamic_mesh_resources_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

DynamicMeshRenderFeature::DynamicMeshRenderFeature(
    RendererVK* p_renderer, RefRes<RUMesh3D> p_mesh, RefRes<RUGraphicsPipeline> p_material_pipeline, uint32_t p_capacity)
    : render_data(p_renderer->get_base_render_data()) {
    resources = DynamicMeshCompilerVK(p_renderer).compile(p_mesh,
        p_capacity,
        ShaderMetadataLoader().load(p_material_pipeline->vertex_stage),
        mesh_vertex_entry(p_material_pipeline),
        load_mesh_material_contract(p_material_pipeline),
        DynamicMeshCompilerVK::Config{});
}

void DynamicMeshRenderFeature::register_resources(RendGraphDescVK& p_graph) const {
    p_graph.add_dynamic_mesh(resources);
}

void DynamicMeshRenderFeature::publish_visible_transforms(std::span<const glm::mat4> p_transforms) {
    resources->publish_visible_transforms(p_transforms);
}

RendTaskDescVK DynamicMeshRenderFeature::make_task(std::string_view p_task_id,
    Ref<ROGraphicsPipelineVK> p_pipeline,
    VkExtent2D p_extent,
    std::span<const std::string_view> p_colors,
    std::optional<std::string_view> p_depth) const {
    RendTaskDescVK result(render_data->allocator);
    result.task_id = p_task_id;
    alias_mesh_resources(result, *p_pipeline, DYNAMIC_MESH_RESOURCE_KEYS);
    RTRunGraphicsPipelineIndirectVK::Builder builder(render_data->allocator);
    builder.set_render_extent(p_extent)
        .set_vertex_buffer(REND_RES_DYNAMIC_MESH_VERTICES)
        .set_index_buffer(REND_RES_DYNAMIC_MESH_INDICES)
        .set_indirect_buffer(REND_RES_DYNAMIC_MESH_DRAW_BATCHES)
        .set_color_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
        .set_depth_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    alias_render_targets(result, p_colors, p_depth);
    for (auto& batch : make_dynamic_mesh_pipeline_batches(render_data->allocator, *resources, p_pipeline)) {
        builder.add_pipeline_batch(std::move(batch));
    }
    result.task = builder.create_unique(render_data);
    return result;
}

} // namespace WhiteBirdEngine
