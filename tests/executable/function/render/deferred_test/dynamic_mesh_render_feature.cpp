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
#include "core/allocator/stack_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/geometry_draw_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/static_set_draw_batch_planner_vk.hh"
#include "function/render_vk/static_set_resources_vk.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {
constexpr std::string_view VERTICES = "dynamic_mesh_vertices";
constexpr std::string_view INDICES = "dynamic_mesh_indices";
constexpr std::string_view NODES = "dynamic_mesh_nodes";
constexpr std::string_view COMMANDS = "dynamic_mesh_commands";
constexpr std::string_view SUBMESHES = "dynamic_mesh_submeshes";
constexpr std::string_view INSTANCES = "dynamic_mesh_instances";
constexpr std::string_view MATERIAL_PARAMS = "dynamic_mesh_material_params";
constexpr std::string_view MATERIAL_SAMPLER = "dynamic_mesh_material_sampler";
constexpr std::string_view TEXTURES = "dynamic_mesh_textures";

Ref<ROCMultislotsVK> make_slots(const BaseRenderDataVK* p_data, size_t p_size, VkBufferUsageFlags p_usage) {
    return make_ref<ROCMultislotsVK>(
        p_data->allocator, p_data->allocator, p_data->frames_in_flight, [p_data, p_size, p_usage](uint32_t) -> Ref<IRenderObjectVK> {
            return ROBufferVK::Builder().set_properties(p_size, p_usage).create_ref(p_data);
        });
}
} // namespace

DynamicMeshRenderFeature::DynamicMeshRenderFeature(
    RendererVK* p_renderer, RefRes<RUMesh3D> p_mesh, RefRes<RUGraphicsPipeline> p_material_pipeline, uint32_t p_capacity)
    : render_data(p_renderer->get_base_render_data()) {
    if (p_mesh == MEM_NULL || p_mesh->submesh_count == 0 || p_mesh->submeshes == MEM_NULL || p_capacity == 0 ||
        p_mesh->submesh_count > std::numeric_limits<uint32_t>::max() / p_capacity) {
        throw std::runtime_error("Dynamic mesh requires geometry and a valid fixed instance capacity.");
    }
    // Adapt the existing mesh views only for compilation; all returned GPU resources own their data.
    static_assert(sizeof(RUStaticGeometryInstance) % WBE_DEFAULT_ALIGNMENT == 0);
    static_assert(sizeof(RUStaticGeometrySubmesh) % WBE_DEFAULT_ALIGNMENT == 0);
    StackAllocator storage(get_align_size(sizeof(RUStaticOpaqueSet), WBE_DEFAULT_ALIGNMENT) +
                           sizeof(RUStaticGeometryInstance) * p_capacity + sizeof(RUStaticGeometrySubmesh) * p_mesh->submesh_count);
    RefRes<RUStaticOpaqueSet> set(create_stack_obj<RUStaticOpaqueSet>(storage), &storage);
    for (uint32_t i = 0; i < p_capacity; ++i) {
        const MemID memory = create_stack_obj<RUStaticGeometryInstance>(storage);
        if (i == 0) {
            set->instances = RefRes<RUStaticGeometryInstance>(memory, &storage);
        }
    }
    set->instance_count = p_capacity;
    Vector<uint32_t, RenderAllocator> first_instances(render_data->allocator);
    for (size_t i = 0; i < p_mesh->submesh_count; ++i) {
        auto source = p_mesh->submeshes.get()[i];
        if (source == MEM_NULL || source->indices.index_count == 0) {
            throw std::runtime_error("Dynamic demonstration mesh contains an empty submesh.");
        }
        const MemID memory = create_stack_obj<RUStaticGeometrySubmesh>(storage);
        auto* submesh = static_cast<RUStaticGeometrySubmesh*>(storage.get(memory));
        submesh->vertices = source->vertices;
        submesh->indices = source->indices;
        submesh->material = source->material;
        submesh->instance_count = p_capacity;
        if (i == 0) {
            set->submeshes = RefRes<RUStaticGeometrySubmesh>(memory, &storage);
        }
        first_instances.push_back(static_cast<uint32_t>(i) * p_capacity);
    }
    set->submesh_count = p_mesh->submesh_count;
    geometry = create_static_set_resources(p_renderer, set, MEM_NULL, p_material_pipeline, MEM_NULL, p_material_pipeline);
    auto metadata = ShaderMetadataLoader().load(p_material_pipeline->vertex_stage);
    auto plan = StaticSetDrawBatchPlannerVK().build(render_data->allocator,
        {set->submeshes.get(), set->submesh_count},
        first_instances,
        metadata,
        static_set_vertex_entry(p_material_pipeline),
        render_data->max_draw_indirect_count);
    draw_data = make_unique<GeometryDrawDataVK>(render_data->allocator, render_data->allocator, p_capacity, plan.commands);
    for (size_t i = 0; i < p_mesh->submesh_count; ++i) {
        pop_stack_obj<RUStaticGeometrySubmesh>(storage);
    }
    for (uint32_t i = 0; i < p_capacity; ++i) {
        pop_stack_obj<RUStaticGeometryInstance>(storage);
    }
    pop_stack_obj<RUStaticOpaqueSet>(storage);
    nodes = make_slots(render_data, draw_data->get_nodes().size_bytes(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    commands = make_slots(render_data, draw_data->get_commands().size_bytes(), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    publish_visible_transforms({});
}

void DynamicMeshRenderFeature::register_resources(RendGraphDescVK& p_graph) const {
    p_graph.add_resource(VERTICES, geometry->acquire_vertices())
        .add_resource(INDICES, geometry->acquire_indices())
        .add_resource(SUBMESHES, geometry->acquire_submeshes())
        .add_resource(INSTANCES, geometry->acquire_instances())
        .add_resource(NODES, nodes)
        .add_resource(COMMANDS, commands)
        .add_resource(MATERIAL_PARAMS, geometry->acquire_material_params())
        .add_resource(MATERIAL_SAMPLER, geometry->acquire_material_sampler())
        .add_resource(TEXTURES, geometry->acquire_textures());
}

void DynamicMeshRenderFeature::publish_visible_transforms(std::span<const glm::mat4> p_transforms) {
    draw_data->update(p_transforms);
    nodes->publish_buffer_update(draw_data->get_nodes().data(), 0, draw_data->get_nodes().size_bytes());
    commands->publish_buffer_update(draw_data->get_commands().data(), 0, draw_data->get_commands().size_bytes());
}

RendTaskDescVK DynamicMeshRenderFeature::make_task(std::string_view p_task_id,
    Ref<ROGraphicsPipelineVK> p_pipeline,
    VkExtent2D p_extent,
    std::span<const std::string_view> p_colors,
    std::optional<std::string_view> p_depth) const {
    RendTaskDescVK result(render_data->allocator);
    result.task_id = p_task_id;
    // Some passes omit material sampling; only map identifiers present in that pipeline's reflection.
    const std::array<std::pair<std::string_view, std::string_view>, 5> mappings{{
        {REND_RES_STATIC_SET_NODES, NODES},
        {REND_RES_STATIC_SET_SUBMESHES, SUBMESHES},
        {REND_RES_STATIC_SET_INSTANCES, INSTANCES},
        {REND_RES_STATIC_SET_MATERIAL_PARAMS, MATERIAL_PARAMS},
        {REND_RES_STATIC_SET_MATERIAL_SAMPLER, MATERIAL_SAMPLER},
    }};
    for (const auto& [source, target] : mappings) {
        for (const auto& field : p_pipeline->get_descriptor_layout().fields) {
            if (field.resource_id == source) {
                result.alias(source, target);
                break;
            }
        }
    }
    RTRunGraphicsPipelineIndirectVK::Builder builder(render_data->allocator);
    builder.set_render_extent(p_extent)
        .set_vertex_buffer(VERTICES)
        .set_index_buffer(INDICES)
        .set_indirect_buffer(COMMANDS)
        .set_color_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
        .set_depth_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    alias_render_targets(result, p_colors, p_depth);
    auto batches = make_static_set_pipeline_batches(render_data->allocator, geometry, p_pipeline, false, false);
    for (auto& batch : batches) {
        builder.add_pipeline_batch(std::move(batch));
    }
    result.task = builder.create_unique(render_data);
    return result;
}

} // namespace WhiteBirdEngine
