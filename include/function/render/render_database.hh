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
#ifndef __WBE_RENDER_DATABASE_HH__
#define __WBE_RENDER_DATABASE_HH__

#include "core/surface/3d_primitive.hh"
#include "function/render/material.hh"
#include "global/stl_allocator.hh"
#include <algorithm>
#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>

namespace WhiteBirdEngine {

/**
 * @class RenderDatabase
 * @brief Renderer render database.
 *
 * @todo Test
 */
template <typename AllocType>
class RenderDatabase {
private:
    template <typename T>
    using vector = ::WhiteBirdEngine::vector<T, AllocType>;
    template <typename T>
    using set = ::WhiteBirdEngine::set<T, AllocType>;
public:

    ~RenderDatabase() {}
    RenderDatabase(const RenderDatabase&) = delete;
    RenderDatabase(RenderDatabase&&) = delete;
    RenderDatabase& operator=(const RenderDatabase&) = delete;
    RenderDatabase& operator=(RenderDatabase&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator for resource allocations.
     */
    RenderDatabase(AllocType* p_allocator);


    /**
     * @brief Set the static opaque triangles.
     *
     * @param p_vertices The vertices of the triangles. Note that this vector have to be at the same memory pool
     * with this render database.
     * @param p_indices The indices of the triangles, with respect to its material.
     */
    template <typename VertType, typename IndxType>
    void set_static_opaque_triangles(VertType&& p_vertices, IndxType&& p_indices) {
        set_opaque_triangles_linear(static_opaque_triangle_map, static_opaque_triangle_vertices, static_opaque_triangle_indices,
                                    dynamic_opaque_triangle_obj_id, std::forward<VertType>(p_vertices), std::forward<IndxType>(p_indices));
    }

    /**
     * @brief Set the dynamic opaque triangles.
     *
     * @param p_vertices The vertices of the triangles. Note that this vector have to be at the same memory pool
     * with this render database.
     * @param p_indices The indices of the triangles, with respect to its material.
     */
    template <typename VertType, typename IndxType>
    void set_dynamic_opaque_triangles(VertType&& p_vertices, IndxType&& p_indices) {
        set_opaque_triangles_linear(dynamic_opaque_triangle_map, dynamic_opaque_triangle_vertices, dynamic_opaque_triangle_indices,
                                    dynamic_opaque_triangle_obj_id, std::move(p_vertices), std::move(p_indices));
    }

private:

    AllocType* allocator;
    std::map<MaterialID, std::pair<size_t, size_t>> static_opaque_triangle_map;
    vector<Vertex3DTextured> static_opaque_triangle_vertices;
    vector<uint32_t> static_opaque_triangle_indices;
    vector<uint32_t> static_opaque_triangle_obj_id;
    vector<glm::mat4> static_opaque_transform_mats;

    // TODO: change dynamic opaque triangles implementation
    std::map<MaterialID, std::pair<size_t, size_t>> dynamic_opaque_triangle_map;
    vector<Vertex3DTextured> dynamic_opaque_triangle_vertices;
    vector<uint32_t> dynamic_opaque_triangle_indices;
    vector<uint32_t> dynamic_opaque_triangle_obj_id;

    void set_opaque_triangles_linear(
        std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
        vector<Vertex3DTextured>& p_opaque_triangle_vertices,
        vector<uint32_t>& p_opaque_triangle_indices,
        vector<uint32_t>& p_opaque_triangle_obj_id,
        vector<Vertex3DTextured>&& p_vertices,
        vector<std::pair<MaterialID, Triangle3DIndxTextured>>&& p_indices);

    void set_opaque_triangles_linear(
        std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
        vector<Vertex3DTextured>& p_opaque_triangle_vertices,
        vector<uint32_t>& p_opaque_triangle_indices,
        vector<uint32_t>& p_opaque_triangle_obj_id,
        const vector<Vertex3DTextured>& p_vertices,
        const vector<std::pair<MaterialID, Triangle3DIndxTextured>>& p_indices);

};

template <typename AllocType>
RenderDatabase<AllocType>::RenderDatabase(AllocType* p_allocator)
    : static_opaque_triangle_vertices(p_allocator), static_opaque_triangle_indices(p_allocator), static_opaque_triangle_obj_id(p_allocator),
    static_opaque_transform_mats(p_allocator),
    dynamic_opaque_triangle_vertices(p_allocator), dynamic_opaque_triangle_indices(p_allocator), dynamic_opaque_triangle_obj_id(p_allocator) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Failed to create renderer database: allocator is null.");
    }
    allocator = p_allocator;
}

template <typename AllocType>
void RenderDatabase<AllocType>::set_opaque_triangles_linear(
    std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
    vector<Vertex3DTextured>& p_opaque_triangle_vertices,
    vector<uint32_t>& p_opaque_triangle_indices,
    vector<uint32_t>& p_opaque_triangle_obj_id,
    vector<Vertex3DTextured>&& p_vertices,
    vector<std::pair<MaterialID, Triangle3DIndxTextured>>&& p_indices) {

    p_opaque_triangle_map.clear();
    vector(allocator, std::move(p_vertices)).swap(p_opaque_triangle_vertices);
    p_opaque_triangle_indices = vector<uint32_t>(allocator);
    p_opaque_triangle_obj_id = vector<uint32_t>(allocator);

    using TriType = std::pair<MaterialID, Triangle3DIndxTextured>;
    std::sort(p_indices.begin(), p_indices.end(), [](
        const TriType& p_tri1, const TriType& p_tri2) -> bool {
              // Sort according to the material first, then sort according to the object ID.
              // This makes sure that all the triangles with the same materials will be stored together,
              // and for each material the triangles within the same object will be stored together.
              return p_tri1.first == p_tri2.first ? p_tri1.second.obj_id < p_tri2.second.obj_id : p_tri1.first < p_tri2.first;
              });
    MaterialID current_id = MATERIAL_INVALID;
    p_opaque_triangle_obj_id.reserve(p_indices);
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
    std::vector<uint32_t>().swap(p_indices);
}

template <typename AllocType>
void RenderDatabase<AllocType>::set_opaque_triangles_linear(
    std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
    vector<Vertex3DTextured>& p_opaque_triangle_vertices,
    vector<uint32_t>& p_opaque_triangle_indices,
    vector<uint32_t>& p_opaque_triangle_obj_id,
    const vector<Vertex3DTextured>& p_vertices,
    const vector<std::pair<MaterialID, Triangle3DIndxTextured>>& p_indices) {

    p_opaque_triangle_map.clear();
    p_opaque_triangle_vertices = p_vertices;
    p_opaque_triangle_indices = vector<uint32_t>(allocator);
    p_opaque_triangle_obj_id = vector<uint32_t>(allocator);

    using TriType = std::pair<MaterialID, Triangle3DIndxTextured>;
    std::sort(p_indices.begin(), p_indices.end(), [](
        const TriType& p_tri1, const TriType& p_tri2) -> bool {
              // Sort according to the material first, then sort according to the object ID.
              // This makes sure that all the triangles with the same materials will be stored together,
              // and for each material the triangles within the same object will be stored together.
              return p_tri1.first == p_tri2.first ? p_tri1.second.obj_id < p_tri2.second.obj_id : p_tri1.first < p_tri2.first;
              });
    MaterialID current_id = MATERIAL_INVALID;
    p_opaque_triangle_obj_id.reserve(p_indices);
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


}

#endif
