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
#include "function/render/render_database.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "core/surface/3d_primitive.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace WhiteBirdEngine {
namespace {

struct MaterialMeshInfo {
    std::string_view graphics_pipeline_key;
    std::string_view material_key;
    RefRes<RUMaterial> material;
    Vector<RUSubmesh3D*, RenderAllocator> submeshes;
};

// Group submeshes by material before flattening them. This lets the render
// database record one contiguous index subregion per material.
void register_material_submesh(Vector<MaterialMeshInfo, RenderAllocator>& p_material_mesh_infos,
    std::unordered_map<std::string_view, size_t>& p_registered_material_index,
    RUSubmesh3D& p_submesh,
    RenderAllocator* p_allocator) {
    auto& material = p_submesh.material;
    if (material == nullptr) {
        throw std::runtime_error("Failed to set opaque triangles: submesh material should not be NULL.");
    }
    if (material->resource_key == nullptr) {
        throw std::runtime_error("Failed to set opaque triangles: submesh material resource key should not be NULL.");
    }
    if (material->graphics_pipeline == nullptr || material->graphics_pipeline->resource_key == nullptr) {
        throw std::runtime_error("Failed to set opaque triangles: submesh material graphics pipeline should not be NULL.");
    }
    std::string_view material_key = material->resource_key;
    auto material_entry = p_registered_material_index.find(material_key);
    if (material_entry == p_registered_material_index.end()) {
        size_t material_index = p_material_mesh_infos.size();
        p_registered_material_index.insert({material_key, material_index});
        p_material_mesh_infos.push_back(MaterialMeshInfo{
            .graphics_pipeline_key = material->graphics_pipeline->resource_key,
            .material_key = material_key,
            .material = material,
            .submeshes = Vector<RUSubmesh3D*, RenderAllocator>(p_allocator)});
        material_entry = p_registered_material_index.find(material_key);
    }
    p_material_mesh_infos[material_entry->second].submeshes.push_back(&p_submesh);
}

// Append one submesh into the linear static-opaque buffers while preserving its
// local triangle indices as offsets into the merged vertex buffer.
void append_submesh_triangles(RenderDatabase::OpaqueTrianglesInfo& p_triangles_info, RUSubmesh3D& p_submesh, uint32_t p_obj_id) {
    uint32_t max_index = 0;
    size_t curr_index = p_triangles_info.vertices.size();
    for (size_t i = 0; i < p_submesh.triangles_count; ++i) {
        auto& triangle = p_submesh.triangles.get()[i];
        p_triangles_info.indices.push_back(curr_index + triangle.indices.vert_1);
        p_triangles_info.indices.push_back(curr_index + triangle.indices.vert_2);
        p_triangles_info.indices.push_back(curr_index + triangle.indices.vert_3);
        max_index = std::max({max_index, triangle.indices.vert_1, triangle.indices.vert_2, triangle.indices.vert_3});
    }
    p_triangles_info.obj_id.push_back(p_obj_id);
    p_triangles_info.vertices.resize(curr_index + max_index + 1);
    memcpy(&(p_triangles_info.vertices[curr_index]), p_submesh.triangles->vert_array.get(), (max_index + 1) * sizeof(Vertex3DTextured));
}

// Append all submeshes for one material and record the material's index range.
void append_material_mesh_info(RenderDatabase::OpaqueTrianglesInfo& p_triangles_info, const MaterialMeshInfo& p_mesh_info) {
    static std::atomic<uint32_t> obj_id_tracker = 0;
    uint32_t curr_obj_id = obj_id_tracker.fetch_add(1, std::memory_order_acq_rel);
    size_t material_mesh_start = p_triangles_info.indices.size();
    for (auto* submesh : p_mesh_info.submeshes) {
        append_submesh_triangles(p_triangles_info, *submesh, curr_obj_id);
    }
    p_triangles_info.materials.push_back(RenderDatabase::OpaqueTrianglesInfo::MaterialUnit{
        .material = p_mesh_info.material,
        .sub_region = {.starting_index = material_mesh_start, .size = p_triangles_info.indices.size() - material_mesh_start},
    });
}

const RenderDatabase::OpaqueTrianglesInfo::MaterialUnit* find_material_unit(
    const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info, std::string_view p_material_key) {
    for (const auto& material_unit : p_triangles_info.materials) {
        if (material_unit.material != nullptr && material_unit.material->resource_key != nullptr &&
            std::string_view(material_unit.material->resource_key) == p_material_key) {
            return &material_unit;
        }
    }
    return nullptr;
}

} // namespace

RenderDatabase::RenderDatabase(AllocType* p_allocator)
    : compiled_render_resources(p_allocator), static_opaque(p_allocator), dynamic_opaque(p_allocator) {
    singleton = this;
    if (p_allocator == nullptr) {
        throw std::runtime_error("Failed to create renderer database: allocator is null.");
    }
    allocator = p_allocator;
}

void RenderDatabase::register_compiled_render_resource(
    std::string_view p_resource_key, HashCode p_render_object_type_id, RefWeak<IRenderObject> p_render_object) {
    if (p_resource_key.empty()) {
        throw std::runtime_error("Failed to register compiled render resource: resource key is empty.");
    }
    if (p_render_object.is_null()) {
        throw std::runtime_error("Failed to register compiled render resource: render object is null.");
    }
    compiled_render_resources.insert_or_assign(
        CompiledRenderResourceKey{.resource_key = std::string(p_resource_key), .render_object_type_id = p_render_object_type_id},
        p_render_object);
}

Ref<IRenderObject> RenderDatabase::get_compiled_render_resource(std::string_view p_resource_key, HashCode p_render_object_type_id) {
    auto entry = compiled_render_resources.find(
        CompiledRenderResourceKey{.resource_key = std::string(p_resource_key), .render_object_type_id = p_render_object_type_id});
    if (entry == compiled_render_resources.end() || entry->second.is_null()) {
        return MEM_NULL;
    }
    return entry->second.lock();
}

bool RenderDatabase::contains_compiled_render_resource(std::string_view p_resource_key, HashCode p_render_object_type_id) const {
    auto entry = compiled_render_resources.find(
        CompiledRenderResourceKey{.resource_key = std::string(p_resource_key), .render_object_type_id = p_render_object_type_id});
    return entry != compiled_render_resources.end() && entry->second.is_valid();
}

void RenderDatabase::clear_compiled_render_resources() {
    compiled_render_resources.clear();
}

const RenderDatabase::OpaqueTrianglesInfo::MaterialUnit* RenderDatabase::find_opaque_material(std::string_view p_material_key) const {
    if (const auto* static_material = find_material_unit(static_opaque, p_material_key); static_material != nullptr) {
        return static_material;
    }
    return find_material_unit(dynamic_opaque, p_material_key);
}

void RenderDatabase::set_opaque_triangles_linear(OpaqueTrianglesInfo& p_triangles_info, RUMesh3D* p_mesh) {
    if (p_mesh == nullptr) {
        throw std::runtime_error("Failed to set opaque triangles: mesh "
                                 "resource should not be NULL.");
    }
    // The triangles are arranged by their materials, which triangles with same
    // materials are put together. This is designed this way so that we could
    // use the least amount of draw calls for per-material drawing. Triangles
    // with the same material are drawn together in one render pass.

    p_triangles_info.reset();

    // Record the submeshes for each material
    Vector<MaterialMeshInfo> material_mesh_infos(allocator);
    std::unordered_map<std::string_view, size_t> registered_material_index;

    size_t total_triangles = 0;
    std::span<RefRes<RUSubmesh3D>> submeshes(p_mesh->submeshes.get(), p_mesh->submesh_count);
    for (auto& submesh : submeshes) {
        register_material_submesh(material_mesh_infos, registered_material_index, *submesh, allocator);
        total_triangles += submesh->triangles_count;
    }
    p_triangles_info.obj_id.reserve(total_triangles);
    p_triangles_info.indices.reserve(total_triangles * 3);

    std::sort(material_mesh_infos.begin(), material_mesh_infos.end(), [](const MaterialMeshInfo& p_left, const MaterialMeshInfo& p_right) {
        if (p_left.graphics_pipeline_key != p_right.graphics_pipeline_key) {
            return p_left.graphics_pipeline_key < p_right.graphics_pipeline_key;
        }
        return p_left.material_key < p_right.material_key;
    });

    for (const auto& mesh_info : material_mesh_infos) {
        if (p_triangles_info.graphics_pipelines.empty() ||
            p_triangles_info.graphics_pipelines.back().graphics_pipeline != mesh_info.material->graphics_pipeline) {
            p_triangles_info.graphics_pipelines.push_back(OpaqueTrianglesInfo::GraphicsPipelineUnit{
                .graphics_pipeline = mesh_info.material->graphics_pipeline,
                .material_sub_region = {.starting_index = p_triangles_info.materials.size(), .size = 0},
            });
        }
        append_material_mesh_info(p_triangles_info, mesh_info);
        ++p_triangles_info.graphics_pipelines.back().material_sub_region.size;
    }
}
} // namespace WhiteBirdEngine
