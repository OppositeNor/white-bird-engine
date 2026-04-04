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
#include "core/async/worker.hh"
#include "core/memory/unique.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "core/surface/3d_primitive.hh"
#include "function/render/material.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace WhiteBirdEngine {

RenderDatabase::RenderDatabase(AllocType* p_allocator, Unique<IMaterialFactory> p_material_factory)
    : material_factory(std::move(p_material_factory)), static_opaque(p_allocator), dynamic_opaque(p_allocator),
    render_objects_resources(p_allocator) {
    WBE_DEBUG_ASSERT(material_factory != MEM_NULL);
    singleton = this;
    if (p_allocator == nullptr) {
        throw std::runtime_error("Failed to create renderer database: allocator is null.");
    }
    allocator = p_allocator;
}

bool RenderDatabase::contains_resource(HashCode p_key) {
    std::shared_lock lock(render_objects_resources_mutex);
    return render_objects_resources.contains(p_key);
}

void RenderDatabase::register_render_object_resource(HashCode p_key, Ref<IRenderObject> p_render_object) {
    // TODO: For render objects that could be reused, register them into the database, and during render object
    // creation try to retrieve them instead of re-creating them.
    std::unique_lock lock(render_objects_resources_mutex);
    if (render_objects_resources.contains(p_key)) {
        throw std::runtime_error("Failed to register render object resource: hashing collision detected!");
    }
    render_objects_resources.insert(std::pair(p_key, p_render_object));
    // If render_objects_resources's size are greater than a threashold, clean up the dead weak references.
    if (render_objects_resources.size() > CLEAN_DEAD_RO_WEAK_REFS_THREASHOLD) {
        Worker::get_singleton()->add_job([&](){
            std::unique_lock lock(render_objects_resources_mutex);
            std::erase_if(render_objects_resources, [](const auto& p_entry) {
                return p_entry.second.is_valid();
            });
        });
    }
}

void RenderDatabase::set_opaque_triangles_linear(
    OpaqueTrianglesInfo& p_triangles_info,
    RUMesh3D* p_mesh) {
    if (p_mesh == nullptr) {
        throw std::runtime_error("Failed to set opaque triangles: mesh resource should not be NULL.");
    }
    // The triangles are arranged by their materials, which triangles with same materials are put together.
    // This is designed this way so that we could use the least amount of draw calls for per-material drawing.
    // Triangles with the same material are drawn together in one render pass.

    p_triangles_info.reset();

    struct MaterialMeshInfo {
        Vector<RUSubmesh3D*> submeshes;
    };
    
    // Record the submeshes for each material
    Vector<MaterialMeshInfo> material_mesh_infos(allocator);
    std::unordered_map<RUMaterial*, size_t> registered_material_index;

    size_t total_triangles = 0;
    for (size_t i = 0; i < p_mesh->submesh_count; ++i) {
        auto& submesh = p_mesh->submeshes.get()[i];
        auto& material = submesh->material;
        if (!registered_material_index.contains(material.get())) {
            // If not registered before, register.
            p_triangles_info.materials.push_back({.material = material_factory->create(material)});
            MaterialMeshInfo info = {.submeshes = Vector<RUSubmesh3D*>(allocator)};
            registered_material_index.insert({material.get(), material_mesh_infos.size()});
            material_mesh_infos.push_back(info);
        }
        // Register the submeshes to the corresponding mesh.
        size_t material_index = registered_material_index[material.get()];
        auto& material_mesh_info = material_mesh_infos[material_index];
        material_mesh_info.submeshes.push_back(submesh.get());
        total_triangles += submesh->triangles_count;
    }
    p_triangles_info.obj_id.reserve(total_triangles);
    p_triangles_info.indices.reserve(total_triangles * 3);

    // Process the triangles in the mesh, and record them in the vertices.
    for (size_t i = 0; i < material_mesh_infos.size(); ++i) {
        // Assign a unique ID for each submesh so they could be distinguished from other submeshes.
        // This is useful for object edge detection or some per-object operations.
        static std::atomic<uint32_t> obj_id_tracker = 0;
        uint32_t curr_obj_id = obj_id_tracker.fetch_add(1, std::memory_order_acq_rel);

        auto& mesh_info = material_mesh_infos[i];
        size_t material_mesh_start = p_triangles_info.indices.size();
        for (auto& submesh : mesh_info.submeshes) {
            uint32_t max_index = 0;
            size_t curr_index = p_triangles_info.vertices.size();
            for (size_t i = 0; i < submesh->triangles_count; ++i) {
                auto& triangle = submesh->triangles.get()[i];
                // Because we are merging the vertices into one array, the original indices should add the offset where this
                // submesh's vertices is stored in the vertex array.
                p_triangles_info.indices.push_back(curr_index + triangle.indices.vert_1);
                p_triangles_info.indices.push_back(curr_index + triangle.indices.vert_2);
                p_triangles_info.indices.push_back(curr_index + triangle.indices.vert_3);
                // We also record the maximum index to determine how much vertices are copied.
                max_index = std::max({max_index, triangle.indices.vert_1, triangle.indices.vert_2, triangle.indices.vert_3});
            }
            // Assign an unique ID for each triangle to distinguish from other 
            p_triangles_info.obj_id.push_back(curr_obj_id);

            p_triangles_info.vertices.resize(curr_index + max_index + 1);
            memcpy(&(p_triangles_info.vertices[curr_index]), submesh->triangles->vert_array.get(), (max_index + 1) * sizeof(Vertex3DTextured));
        }
        // Setup subregion for the material.
        p_triangles_info.materials[i].sub_region.starting_index = material_mesh_start;
        p_triangles_info.materials[i].sub_region.size = p_triangles_info.indices.size() - material_mesh_start;
    }
}

} // namespace WhiteBirdEngine
