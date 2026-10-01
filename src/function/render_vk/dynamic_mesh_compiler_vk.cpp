/* Copyright 2026 OppositeNor

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
#include "function/render_vk/dynamic_mesh_compiler_vk.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/dynamic_mesh_resources_vk.hh"
#include "function/render_vk/geometry_draw_data_vk.hh"
#include "function/render_vk/i_mesh_compiler_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/mesh_buffer_compiler_vk.hh"
#include "function/render_vk/mesh_draw_batch_planner_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

Ref<ROCMultislotsVK> make_frame_slots(const BaseRenderDataVK* p_render_data, size_t p_size, VkBufferUsageFlags p_usage) {
    return make_ref<ROCMultislotsVK>(p_render_data->allocator,
        p_render_data->allocator,
        p_render_data->frames_in_flight,
        [p_render_data, p_size, p_usage](uint32_t) -> Ref<IRenderObjectVK> {
            return ROBufferVK::Builder().set_properties(p_size, p_usage).create_ref(p_render_data);
        });
}

} // namespace

DynamicMeshCompilerVK::DynamicMeshCompilerVK(RendererVK* p_renderer) : IMeshCompilerVK(p_renderer) {
}

Ref<DynamicMeshResourcesVK> DynamicMeshCompilerVK::compile(RefRes<RUMesh3D> p_mesh,
    uint32_t p_capacity,
    const ShaderMetadata& p_vertex_metadata,
    std::string_view p_vertex_entry,
    const std::optional<MaterialTextureContract>& p_material_contract,
    const Config& p_config) const {
    if (p_mesh == MEM_NULL || p_mesh->submesh_count == 0 || p_mesh->submeshes == MEM_NULL) {
        throw std::runtime_error("Failed to compile dynamic-mesh resources: mesh has no submeshes.");
    }
    if (p_capacity == 0 || p_mesh->submesh_count > std::numeric_limits<uint32_t>::max() / p_capacity) {
        throw std::runtime_error("Failed to compile dynamic-mesh resources: instance capacity is zero or overflows the instance table.");
    }
    validate_node_layout(p_vertex_metadata);

    const BaseRenderDataVK* render_data = get_render_data();
    std::vector<RefRes<RUMaterial>> ordered_materials;
    std::vector<GeometrySubmeshGPU> submesh_rows;
    std::vector<GeometryInstanceGPU> instance_links;
    Vector<MeshDrawBatchPlannerVK::Draw, RenderAllocator> draws(render_data->allocator);
    submesh_rows.reserve(p_mesh->submesh_count);
    instance_links.reserve(p_mesh->submesh_count * p_capacity);
    draws.reserve(p_mesh->submesh_count);
    for (size_t i = 0; i < p_mesh->submesh_count; ++i) {
        RefRes<RUSubmesh3D> submesh = p_mesh->submeshes.get()[i];
        if (submesh == MEM_NULL || submesh->indices.index_count == 0) {
            throw std::runtime_error("Failed to compile dynamic-mesh resources: mesh contains an empty submesh.");
        }
        validate_material(submesh->material);
        submesh_rows.push_back(GeometrySubmeshGPU{.material_id = get_or_append_material_index(ordered_materials, submesh->material)});
        // Instance k of every submesh is drawn with node transform k.
        for (uint32_t node_index = 0; node_index < p_capacity; ++node_index) {
            instance_links.push_back(GeometryInstanceGPU{.submesh_index = static_cast<uint32_t>(i), .node_index = node_index});
        }
        MeshDrawBatchPlannerVK::Draw draw;
        draw.vertices = submesh->vertices;
        draw.indices = submesh->indices;
        draw.instance_count = p_capacity;
        draw.first_instance = static_cast<uint32_t>(i) * p_capacity;
        draws.push_back(std::move(draw));
    }

    MeshBufferCompilerVK::BuildResult geometry_buffers = compile_geometry_buffers(p_mesh);
    if (geometry_buffers.vertex_buffer == MEM_NULL || geometry_buffers.index_buffer == MEM_NULL) {
        throw std::runtime_error("Failed to compile dynamic-mesh resources: shared vertex or index buffer is missing.");
    }

    Ref<DynamicMeshResourcesVK> result = make_ref<DynamicMeshResourcesVK>(render_data->allocator, render_data->allocator);
    result->capacity = p_capacity;
    result->vertices = std::move(geometry_buffers.vertex_buffer);
    result->indices = std::move(geometry_buffers.index_buffer);
    result->submeshes = create_storage_buffer(submesh_rows.data(), submesh_rows.size() * sizeof(GeometrySubmeshGPU));
    result->instances = create_storage_buffer(instance_links.data(), instance_links.size() * sizeof(GeometryInstanceGPU));

    DrawPlan plan = plan_draws(draws, p_vertex_metadata, p_vertex_entry);
    result->batches = std::move(plan.batches);
    result->draw_data = make_unique<GeometryDrawDataVK>(render_data->allocator, render_data->allocator, p_capacity, plan.commands);
    result->nodes = make_frame_slots(render_data, result->draw_data->get_nodes().size_bytes(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    result->draw_batches =
        make_frame_slots(render_data, result->draw_data->get_commands().size_bytes(), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

    MaterialBuild material_build = compile_materials(
        p_material_contract, p_config, std::span<const RefRes<RUMaterial>>(ordered_materials.data(), ordered_materials.size()));
    result->material_params = std::move(material_build.material_params);
    result->material_sampler = std::move(material_build.material_sampler);
    result->textures = std::move(material_build.textures);
    result->publish_visible_transforms({});
    return result;
}

} // namespace WhiteBirdEngine
