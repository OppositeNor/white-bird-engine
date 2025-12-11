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
#include "core/surface/3d_primitive.hh"
#include "function/render/material.hh"

#include <algorithm>
#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>

namespace WhiteBirdEngine {

RenderDatabase::RenderDatabase(AllocType* p_allocator)
    : static_opaque_triangle_vertices(p_allocator), static_opaque_triangle_indices(p_allocator), static_opaque_triangle_obj_id(p_allocator),
    static_opaque_transform_mats(p_allocator),
    dynamic_opaque_triangle_vertices(p_allocator), dynamic_opaque_triangle_indices(p_allocator), dynamic_opaque_triangle_obj_id(p_allocator) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Failed to create renderer database: allocator is null.");
    }
    allocator = p_allocator;
}

void RenderDatabase::set_opaque_triangles_linear(
    std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
    Vector<Vertex3DTextured>& p_opaque_triangle_vertices,
    Vector<uint32_t>& p_opaque_triangle_indices,
    Vector<uint32_t>& p_opaque_triangle_obj_id,
    Vector<Vertex3DTextured>&& p_vertices,
    Vector<std::pair<MaterialID, Triangle3DIndxTextured>>&& p_indices) {

    p_opaque_triangle_map.clear();
    p_opaque_triangle_vertices = std::move(p_vertices);
    p_opaque_triangle_indices = Vector<uint32_t>(allocator);
    p_opaque_triangle_obj_id = Vector<uint32_t>(allocator);

    using TriType = std::pair<MaterialID, Triangle3DIndxTextured>;
    std::sort(p_indices.begin(), p_indices.end(), [](
        const TriType& p_tri1, const TriType& p_tri2) -> bool {
              // Sort according to the material first, then sort according to the object ID.
              // This makes sure that all the triangles with the same materials will be stored together,
              // and for each material the triangles within the same object will be stored together.
              return p_tri1.first == p_tri2.first ? p_tri1.second.obj_id < p_tri2.second.obj_id : p_tri1.first < p_tri2.first;
              });
    MaterialID current_id = MATERIAL_INVALID;
    p_opaque_triangle_obj_id.reserve(p_indices.size());
    for (auto& tri : p_indices) {
        if (tri.first != current_id) {
            // Asign the next material ID's indices.
            p_opaque_triangle_map[tri.first].first = 0;
            p_opaque_triangle_map[tri.first].second = p_opaque_triangle_indices.size();
            current_id = tri.first;
        }
        // Asign indices.
        p_opaque_triangle_indices.insert(p_opaque_triangle_indices.end(),
                                         tri.second.vertex_indices, tri.second.vertex_indices + 3);
        p_opaque_triangle_map[tri.first].first += 3;
        p_opaque_triangle_obj_id.push_back(tri.second.obj_id);
    }
    p_indices.clear();
}

void RenderDatabase::set_opaque_triangles_linear(
    std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
    Vector<Vertex3DTextured>& p_opaque_triangle_vertices,
    Vector<uint32_t>& p_opaque_triangle_indices,
    Vector<uint32_t>& p_opaque_triangle_obj_id,
    const Vector<Vertex3DTextured>& p_vertices,
    Vector<std::pair<MaterialID, Triangle3DIndxTextured>>& p_indices) {

    p_opaque_triangle_map.clear();
    p_opaque_triangle_vertices = p_vertices;
    p_opaque_triangle_indices = Vector<uint32_t>(allocator);
    p_opaque_triangle_obj_id = Vector<uint32_t>(allocator);

    using TriType = std::pair<MaterialID, Triangle3DIndxTextured>;
    std::sort(p_indices.begin(), p_indices.end(), [](
        const TriType& p_tri1, const TriType& p_tri2) -> bool {
              // Sort according to the material first, then sort according to the object ID.
              // This makes sure that all the triangles with the same materials will be stored together,
              // and for each material the triangles within the same object will be stored together.
              return p_tri1.first == p_tri2.first ? p_tri1.second.obj_id < p_tri2.second.obj_id : p_tri1.first < p_tri2.first;
              });
    MaterialID current_id = MATERIAL_INVALID;
    p_opaque_triangle_obj_id.reserve(p_indices.size());
    for (auto& tri : p_indices) {
        if (tri.first != current_id) {
            // Asign the next material ID's indices.
            p_opaque_triangle_map[tri.first].first = 0;
            p_opaque_triangle_map[tri.first].second = p_opaque_triangle_indices.size();
            current_id = tri.first;
        }
        // Asign indices.
        p_opaque_triangle_indices.insert(p_opaque_triangle_indices.end(),
                                         tri.second.vertex_indices, tri.second.vertex_indices + 3);
        p_opaque_triangle_map[tri.first].first += 3;
        p_opaque_triangle_obj_id.push_back(tri.second.obj_id);
    }
}


} // namespace WhiteBirdEngine
