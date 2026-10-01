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
#include "function/render_vk/static_mesh_compiler_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_raw.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/geometry_draw_data_vk.hh"
#include "function/render_vk/i_mesh_compiler_vk.hh"
#include "function/render_vk/mesh_buffer_compiler_vk.hh"
#include "function/render_vk/mesh_draw_batch_planner_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/static_mesh_resources_vk.hh"
#include "glm/common.hpp"
#include "glm/matrix.hpp"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_inverse.hpp>
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

struct StaticInstanceBufferData {
    std::vector<GeometrySubmeshGPU> submeshes;
    std::vector<GeometryNodeGPU> nodes;
    std::vector<GeometryInstanceGPU> instances;
    std::vector<uint32_t> first_instances;
    std::vector<RefRes<RUMaterial>> ordered_materials;
};

struct CombinedStaticInstanceBufferData {
    StaticInstanceBufferData data;
    std::vector<uint32_t> opaque_first_instances;
    std::vector<uint32_t> opaque_double_sided_first_instances;
    std::vector<uint32_t> masked_first_instances;
};

bool has_draws(RefRes<RUStaticMesh> p_mesh) {
    return p_mesh != MEM_NULL && p_mesh->submesh_count != 0 && p_mesh->instance_count != 0;
}

template <typename TMaterialIndexer>
StaticInstanceBufferData build_static_instance_buffer_data(RefRes<RUStaticMesh> p_mesh, const TMaterialIndexer& p_material_indexer) {
    StaticInstanceBufferData result;
    result.submeshes.reserve(p_mesh->submesh_count);
    result.nodes.reserve(p_mesh->instance_count);
    result.instances.reserve(p_mesh->instance_count);
    result.first_instances.reserve(p_mesh->submesh_count);
    result.ordered_materials.reserve(p_mesh->submesh_count);
    for (size_t i = 0; i < p_mesh->instance_count; ++i) {
        const glm::mat4& global_transform = p_mesh->instances.get()[i].global_transform;
        const glm::mat3 transform_3x3 = glm::mat3(global_transform);
        glm::mat4 normal_transform{1.0F};
        if (glm::abs(glm::determinant(transform_3x3)) >= 0.000001F) {
            normal_transform = glm::mat4(glm::inverseTranspose(transform_3x3));
        }
        result.nodes.push_back(GeometryNodeGPU{.transform = global_transform, .normal_transform = normal_transform});
    }

    for (size_t submesh_index = 0; submesh_index < p_mesh->submesh_count; ++submesh_index) {
        const RUStaticGeometrySubmesh& submesh = p_mesh->submeshes.get()[submesh_index];
        if (result.instances.size() > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to compile static-mesh resources: instance table exceeds uint32.");
        }
        result.first_instances.push_back(static_cast<uint32_t>(result.instances.size()));
        if (submesh.indices.index_count == 0 || submesh.instance_count == 0) {
            continue;
        }
        const uint32_t material_index = p_material_indexer(result.ordered_materials, submesh.material);
        result.submeshes.push_back(GeometrySubmeshGPU{.material_id = material_index});
        const uint32_t drawable_submesh_index = static_cast<uint32_t>(result.submeshes.size() - 1);
        for (size_t instance_index = 0; instance_index < submesh.instance_count; ++instance_index) {
            const size_t node_index = submesh.first_instance + instance_index;
            if (node_index > std::numeric_limits<uint32_t>::max()) {
                throw std::runtime_error("Failed to compile static-mesh resources: node index exceeds uint32.");
            }
            result.instances.push_back(GeometryInstanceGPU{
                .submesh_index = drawable_submesh_index,
                .node_index = static_cast<uint32_t>(node_index),
            });
        }
    }
    return result;
}

template <typename TMaterialIndexer>
void append_static_instance_buffer_data(StaticInstanceBufferData& p_destination,
    std::vector<uint32_t>& p_destination_first_instances,
    const StaticInstanceBufferData& p_source,
    const TMaterialIndexer& p_material_indexer) {
    const size_t node_offset = p_destination.nodes.size();
    const size_t submesh_offset = p_destination.submeshes.size();
    const size_t instance_offset = p_destination.instances.size();
    if (node_offset > std::numeric_limits<uint32_t>::max() || submesh_offset > std::numeric_limits<uint32_t>::max() ||
        instance_offset > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to compile static-mesh resources: combined table offset exceeds uint32.");
    }
    p_destination.nodes.insert(p_destination.nodes.end(), p_source.nodes.begin(), p_source.nodes.end());
    p_destination_first_instances.reserve(p_source.first_instances.size());
    for (uint32_t first_instance : p_source.first_instances) {
        if (first_instance > std::numeric_limits<uint32_t>::max() - instance_offset) {
            throw std::runtime_error("Failed to compile static-mesh resources: combined first-instance index exceeds uint32.");
        }
        p_destination_first_instances.push_back(static_cast<uint32_t>(instance_offset) + first_instance);
    }
    for (const GeometrySubmeshGPU& source_submesh : p_source.submeshes) {
        if (source_submesh.material_id >= p_source.ordered_materials.size()) {
            throw std::runtime_error("Failed to compile static-mesh resources: source material index is out of bounds.");
        }
        p_destination.submeshes.push_back(GeometrySubmeshGPU{
            .material_id = p_material_indexer(p_destination.ordered_materials, p_source.ordered_materials[source_submesh.material_id]),
        });
    }
    for (const GeometryInstanceGPU& source_instance : p_source.instances) {
        if (source_instance.submesh_index > std::numeric_limits<uint32_t>::max() - submesh_offset ||
            source_instance.node_index > std::numeric_limits<uint32_t>::max() - node_offset) {
            throw std::runtime_error("Failed to compile static-mesh resources: combined instance index exceeds uint32.");
        }
        p_destination.instances.push_back(GeometryInstanceGPU{
            .submesh_index = static_cast<uint32_t>(submesh_offset) + source_instance.submesh_index,
            .node_index = static_cast<uint32_t>(node_offset) + source_instance.node_index,
        });
    }
}

template <typename TMaterialIndexer>
CombinedStaticInstanceBufferData build_combined_static_instance_buffer_data(RefRes<RUStaticOpaqueMesh> p_opaque_mesh,
    RefRes<RUStaticMaskedMesh> p_masked_mesh,
    RefRes<RUStaticOpaqueDoubleSidedMesh> p_opaque_double_sided_mesh,
    const TMaterialIndexer& p_material_indexer) {
    CombinedStaticInstanceBufferData result;
    if (p_opaque_mesh != MEM_NULL) {
        append_static_instance_buffer_data(result.data,
            result.opaque_first_instances,
            build_static_instance_buffer_data(p_opaque_mesh, p_material_indexer),
            p_material_indexer);
    }
    if (p_opaque_double_sided_mesh != MEM_NULL) {
        append_static_instance_buffer_data(result.data,
            result.opaque_double_sided_first_instances,
            build_static_instance_buffer_data(p_opaque_double_sided_mesh, p_material_indexer),
            p_material_indexer);
    }
    if (p_masked_mesh != MEM_NULL) {
        append_static_instance_buffer_data(result.data,
            result.masked_first_instances,
            build_static_instance_buffer_data(p_masked_mesh, p_material_indexer),
            p_material_indexer);
    }
    return result;
}

Vector<MeshDrawBatchPlannerVK::Draw, RenderAllocator> make_static_draws(
    RenderAllocator* p_allocator, RefRes<RUStaticMesh> p_mesh, std::span<const uint32_t> p_first_instances) {
    if (p_mesh->submesh_count != p_first_instances.size()) {
        throw std::runtime_error("Failed to compile static-mesh resources: first-instance and submesh counts differ.");
    }
    Vector<MeshDrawBatchPlannerVK::Draw, RenderAllocator> result(p_allocator);
    result.reserve(p_mesh->submesh_count);
    for (size_t i = 0; i < p_mesh->submesh_count; ++i) {
        const RUStaticGeometrySubmesh& submesh = p_mesh->submeshes.get()[i];
        if (submesh.instance_count > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to compile static-mesh resources: draw instance count exceeds uint32.");
        }
        MeshDrawBatchPlannerVK::Draw draw;
        draw.vertices = submesh.vertices;
        draw.indices = submesh.indices;
        draw.instance_count = static_cast<uint32_t>(submesh.instance_count);
        draw.first_instance = p_first_instances[i];
        result.push_back(std::move(draw));
    }
    return result;
}

void merge_geometry_buffers(MeshBufferCompilerVK::BuildResult& p_shared, MeshBufferCompilerVK::BuildResult p_category) {
    if (p_category.vertex_buffer == MEM_NULL) {
        return;
    }
    if (p_shared.vertex_buffer != MEM_NULL &&
        (p_shared.vertex_buffer != p_category.vertex_buffer || p_shared.index_buffer != p_category.index_buffer)) {
        throw std::runtime_error("Failed to compile static-mesh resources: all categories must share vertex and index buffers.");
    }
    p_shared = std::move(p_category);
}

} // namespace

StaticMeshCompilerVK::StaticMeshCompilerVK(RendererVK* p_renderer) : IMeshCompilerVK(p_renderer) {
}

Ref<StaticMeshResourcesVK> StaticMeshCompilerVK::compile(RefRes<RUStaticOpaqueMesh> p_opaque_mesh,
    RefRes<RUStaticMaskedMesh> p_masked_mesh,
    const ShaderMetadata& p_opaque_vertex_metadata,
    std::string_view p_opaque_vertex_entry,
    const ShaderMetadata& p_masked_vertex_metadata,
    std::string_view p_masked_vertex_entry,
    const std::optional<MaterialTextureContract>& p_material_contract,
    const Config& p_config,
    RefRes<RUStaticOpaqueDoubleSidedMesh> p_opaque_double_sided_mesh) const {
    const bool has_opaque_draws = has_draws(p_opaque_mesh);
    const bool has_opaque_double_sided_draws = has_draws(p_opaque_double_sided_mesh);
    const bool has_masked_draws = has_draws(p_masked_mesh);
    if (!has_opaque_draws && !has_masked_draws && !has_opaque_double_sided_draws) {
        throw std::runtime_error("Failed to compile static-mesh resources: no drawable static mesh category was supplied.");
    }
    const auto validate_category = [this](RefRes<RUStaticMesh> p_mesh, const ShaderMetadata& p_vertex_metadata) {
        for (size_t i = 0; i < p_mesh->submesh_count; ++i) {
            validate_material(p_mesh->submeshes.get()[i].material);
        }
        validate_node_layout(p_vertex_metadata);
    };
    if (has_opaque_draws) {
        validate_category(p_opaque_mesh, p_opaque_vertex_metadata);
    }
    if (has_opaque_double_sided_draws) {
        validate_category(p_opaque_double_sided_mesh, p_opaque_vertex_metadata);
    }
    if (has_masked_draws) {
        validate_category(p_masked_mesh, p_masked_vertex_metadata);
    }

    const BaseRenderDataVK* render_data = get_render_data();
    Ref<StaticMeshResourcesVK> result = make_ref<StaticMeshResourcesVK>(render_data->allocator, render_data->allocator);
    result->material_params_resource_id = p_config.material_params_variable_name;
    result->opaque_mesh = p_opaque_mesh;
    result->opaque_double_sided_mesh = p_opaque_double_sided_mesh;
    result->masked_mesh = p_masked_mesh;

    MeshBufferCompilerVK::BuildResult geometry_buffers;
    if (has_opaque_draws) {
        merge_geometry_buffers(geometry_buffers, compile_geometry_buffers(p_opaque_mesh, p_config));
    }
    if (has_masked_draws) {
        merge_geometry_buffers(geometry_buffers, compile_geometry_buffers(p_masked_mesh, p_config));
    }
    if (has_opaque_double_sided_draws) {
        merge_geometry_buffers(geometry_buffers, compile_geometry_buffers(p_opaque_double_sided_mesh, p_config));
    }
    if (geometry_buffers.vertex_buffer == MEM_NULL || geometry_buffers.index_buffer == MEM_NULL) {
        throw std::runtime_error("Failed to compile static-mesh resources: shared vertex or index buffer is missing.");
    }
    result->vertices = std::move(geometry_buffers.vertex_buffer);
    result->indices = std::move(geometry_buffers.index_buffer);

    const auto material_indexer = [](std::vector<RefRes<RUMaterial>>& p_materials, RefRes<RUMaterial> p_material) {
        return get_or_append_material_index(p_materials, p_material);
    };
    CombinedStaticInstanceBufferData combined_data =
        build_combined_static_instance_buffer_data(p_opaque_mesh, p_masked_mesh, p_opaque_double_sided_mesh, material_indexer);
    const StaticInstanceBufferData& instance_data = combined_data.data;
    result->submeshes =
        create_storage_buffer(instance_data.submeshes.data(), instance_data.submeshes.size() * sizeof(GeometrySubmeshGPU));
    result->nodes = create_storage_buffer(instance_data.nodes.data(), instance_data.nodes.size() * sizeof(GeometryNodeGPU));
    result->instances =
        create_storage_buffer(instance_data.instances.data(), instance_data.instances.size() * sizeof(GeometryInstanceGPU));

    const auto compile_category = [this, render_data](RefRes<RUStaticMesh> p_mesh,
                                      std::span<const uint32_t> p_first_instances,
                                      const ShaderMetadata& p_vertex_metadata,
                                      std::string_view p_vertex_entry,
                                      Ref<ROBufferVK>& p_draw_buffer,
                                      Vector<MeshDrawBatchVK, RenderAllocator>& p_batches) {
        auto draws = make_static_draws(render_data->allocator, p_mesh, p_first_instances);
        DrawPlan plan = plan_draws(draws, p_vertex_metadata, p_vertex_entry);
        p_draw_buffer = create_indirect_buffer(plan.commands);
        p_batches = std::move(plan.batches);
    };
    if (has_opaque_draws) {
        compile_category(p_opaque_mesh,
            combined_data.opaque_first_instances,
            p_opaque_vertex_metadata,
            p_opaque_vertex_entry,
            result->opaque_draw_batches,
            result->opaque_batches);
    }
    if (has_opaque_double_sided_draws) {
        compile_category(p_opaque_double_sided_mesh,
            combined_data.opaque_double_sided_first_instances,
            p_opaque_vertex_metadata,
            p_opaque_vertex_entry,
            result->opaque_double_sided_draw_batches,
            result->opaque_double_sided_batches);
    }
    if (has_masked_draws) {
        compile_category(p_masked_mesh,
            combined_data.masked_first_instances,
            p_masked_vertex_metadata,
            p_masked_vertex_entry,
            result->masked_draw_batches,
            result->masked_batches);
    }

    MaterialBuild material_build = compile_materials(p_material_contract,
        p_config,
        std::span<const RefRes<RUMaterial>>(instance_data.ordered_materials.data(), instance_data.ordered_materials.size()));
    result->material_params = std::move(material_build.material_params);
    result->material_sampler = std::move(material_build.material_sampler);
    result->textures = std::move(material_build.textures);
    return result;
}

} // namespace WhiteBirdEngine
