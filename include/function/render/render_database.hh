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
#include "render_defs.hh"
#include "global/stl_allocator.hh"
#include "utils/defs.hh"

#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>
#include <map>

namespace WhiteBirdEngine {

/**
 * @class RenderDatabase
 * @brief Renderer render database.
 *
 * @todo Test
 */
class RenderDatabase {
private:
    using AllocType = RenderAllocator;
    template <typename T, typename Allocator = AllocType>
    using Vector = ::WhiteBirdEngine::Vector<T, AllocType>;
    template <typename T>
    using Set = ::WhiteBirdEngine::Set<T, AllocType>;
public:

    ~RenderDatabase() = default;
    WBE_R6_NDCD_DELETE_COPY_MOVE(RenderDatabase);

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
     * @brief Set the dynamic opaque triangles. Note that dynamic mean those objects could be transformed within
     * the scene by transformations handled in the GPU side.
     *
     * @param p_vertices The vertices of the triangles. Note that this vector have to be at the same memory pool
     * with this render database.
     * @param p_indices The indices of the triangles, with respect to its material.
     */
    template <typename VertType, typename IndxType>
    void set_dynamic_opaque_triangles(VertType&& p_vertices, IndxType&& p_indices) {
        set_opaque_triangles_linear(dynamic_opaque_triangle_map, dynamic_opaque_triangle_vertices, dynamic_opaque_triangle_indices,
                                    dynamic_opaque_triangle_obj_id, std::forward(p_vertices), std::forward(p_indices));
    }

    /**
     * @brief Get the static opaque triangle vertices.
     *
     * @return The static opaque triangle vertices.
     */
    const Vector<Vertex3DTextured>& get_static_opaque_triangle_vertices() const {
        return static_opaque_triangle_vertices;
    }

    /**
     * @brief Get the static opaque triangle indices.
     *
     * @return The static opaque triangle indices.
     */
    const Vector<uint32_t>& get_static_opaque_triangle_indices() const {
        return static_opaque_triangle_indices;
    }

    /**
     * @brief Get the static opaque triangle map, which maps a material ID, to the index of indices in static_opaque_triangle_indices,
     * and the number of triangles of this object with the specified material.
     */
    const std::map<MaterialID, std::pair<size_t, size_t>>& get_static_opaque_triangle_map() const {
        return static_opaque_triangle_map;
    }

    /**
     * @brief Get the object ID each static opaque triangle's vertices is associated to.
     *
     * @return The object ID each index is associated to.
     */
    const Vector<uint32_t>& get_static_opaque_triangle_obj_id() const {
        return static_opaque_triangle_obj_id;
    }

    /**
     * @brief Get the dynamic opaque triangle vertices.
     *
     * @return The dynamic opaque triangle vertices.
     */
    const Vector<Vertex3DTextured>& get_dynamic_opaque_triangle_vertices() const {
        return dynamic_opaque_triangle_vertices;
    }

    /**
     * @brief Get the dynamic opaque triangle indices.
     *
     * @return The dynamic opaque triangle indices.
     */
    const Vector<uint32_t>& get_dynamic_opaque_triangle_indices() const {
        return dynamic_opaque_triangle_indices;
    }

    /**
     * @brief Get the dynamic opaque triangle map, which maps a material ID, to the index of indices in dynamic_opaque_triangle_indices,
     * and the number of triangles of this object with the specified material.
     */
    const std::map<MaterialID, std::pair<size_t, size_t>>& get_dynamic_opaque_triangle_map() const {
        return dynamic_opaque_triangle_map;
    }

    /**
     * @brief Get the object ID each dynamic opaque triangle's vertices is associated to.
     *
     * @return The object ID each index is associated to.
     */
    const Vector<uint32_t>& get_dynamic_opaque_triangle_obj_id() const {
        return dynamic_opaque_triangle_obj_id;
    }

private:

    AllocType* allocator;
    std::map<MaterialID, std::pair<size_t, size_t>> static_opaque_triangle_map;
    Vector<Vertex3DTextured> static_opaque_triangle_vertices;
    Vector<uint32_t> static_opaque_triangle_indices;
    Vector<uint32_t> static_opaque_triangle_obj_id;
    Vector<glm::mat4> static_opaque_transform_mats;

    // TODO: change dynamic opaque triangles implementation
    std::map<MaterialID, std::pair<size_t, size_t>> dynamic_opaque_triangle_map;
    Vector<Vertex3DTextured> dynamic_opaque_triangle_vertices;
    Vector<uint32_t> dynamic_opaque_triangle_indices;
    Vector<uint32_t> dynamic_opaque_triangle_obj_id;

    void set_opaque_triangles_linear(
        std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
        Vector<Vertex3DTextured>& p_opaque_triangle_vertices,
        Vector<uint32_t>& p_opaque_triangle_indices,
        Vector<uint32_t>& p_opaque_triangle_obj_id,
        Vector<Vertex3DTextured>&& p_vertices,
        Vector<std::pair<MaterialID, Triangle3DIndxTextured>>&& p_indices);

    void set_opaque_triangles_linear(
        std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
        Vector<Vertex3DTextured>& p_opaque_triangle_vertices,
        Vector<uint32_t>& p_opaque_triangle_indices,
        Vector<uint32_t>& p_opaque_triangle_obj_id,
        const Vector<Vertex3DTextured>& p_vertices,
        Vector<std::pair<MaterialID, Triangle3DIndxTextured>>& p_indices);

};


} // namespace WhiteBirdEngine

#endif
