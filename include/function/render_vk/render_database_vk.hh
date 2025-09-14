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
#ifndef __WBE_RENDER_DATABASE_VK_HH__
#define __WBE_RENDER_DATABASE_VK_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/surface/3d_primitive.hh"
#include "function/render/material.hh"
#include "global/stl_allocator.hh"
#include <map>
#include <stdexcept>
namespace WhiteBirdEngine {

/**
 * @class RenderDatabaseVK
 * @brief Vulkan renderer render database.
 *
 * @todo Test
 */
class RenderDatabaseVK {
public:
    ~RenderDatabaseVK() {}
    RenderDatabaseVK(const RenderDatabaseVK&) = delete;
    RenderDatabaseVK(RenderDatabaseVK&&) = delete;
    RenderDatabaseVK& operator=(const RenderDatabaseVK&) = delete;
    RenderDatabaseVK& operator=(RenderDatabaseVK&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator for resource allocations.
     */
    RenderDatabaseVK(HeapAllocatorAlignedPool* p_allocator);

    /**
     * @brief Set the static triangles in the database.
     *
     * @param p_triangles The static triangle data. For each element, it Should be paird
     * @todo Test
     * with its material ID and the triangle data.
     */
    void set_static_opaque_triangles(vectorp<std::pair<MaterialID, Triangle3DTextured>>&& p_triangles);

    /**
     * @brief Get all of the static opaque triangles in the database.
     *
     * @return The number of triangles and the array of triangles.
     */
    const vectorp<Triangle3DTextured>& get_static_opaque_triangles() const {
        return static_opaque_triangles;
    }

    /**
     * @brief Get the static opaque triangles in the database according to its material ID.
     *
     * @param p_mat_id The material ID of the triangles.
     * @return The number of triangles and the array of triangles.
     */
    std::pair<size_t, const Triangle3DTextured*> get_static_opaque_triangles(MaterialID p_mat_id) const {
        if (p_mat_id == MATERIAL_INVALID) {
            throw std::runtime_error("Invalid material.");
        }
        return static_opaque_triangle_map.at(p_mat_id);
    }
    
    /**
     * @brief Set the dynamic opaque triangle data.
     *
     * @param p_triangles The triangles to be set to.
     * @param p_update_immediate If true, all cached values will be updated immediatly after calling this function.
     * Otherwise they will be lazy-loaded.
     */
    void set_dynamic_opaque_triangles(vectorp<std::pair<MaterialID, Triangle3DTextured>>&& p_triangles, bool p_update_immediate = false);

    /**
     * @brief Get all of the dynamic opaque triangles in the database.
     *
     * @todo Test
     * @return The number of triangles and the array of triangles.
     */
    const vectorp<Triangle3DTextured>& get_dynamic_opaque_triangles() const {
        if (is_dynamic_opaque_triangles_dirty) {
            update_dynamic_opaque_triangles();
        }
        return dynamic_opaque_triangles;
    }

    /**
     * @brief Get the static dynamic triangles in the database according to its material ID.
     *
     * @param p_mat_id The material ID of the triangles.
     * @return The number of triangles and the array of triangles.
     */
    const vectorp<Triangle3DTextured>& get_dynamic_opaque_triangles(MaterialID p_mat_id) const {
        if (p_mat_id == MATERIAL_INVALID) {
            throw std::runtime_error("Invalid material.");
        }
        return dynamic_opaque_triangle_map.at(p_mat_id);
    }
private:
    HeapAllocatorAlignedPool* allocator;

    std::map<MaterialID, std::pair<size_t, Triangle3DTextured*>> static_opaque_triangle_map;
    vectorp<Triangle3DTextured> static_opaque_triangles;

    std::map<MaterialID, vectorp<Triangle3DTextured>> dynamic_opaque_triangle_map;
    mutable vectorp<Triangle3DTextured> dynamic_opaque_triangles;
    mutable bool is_dynamic_opaque_triangles_dirty = false;

    void update_dynamic_opaque_triangles() const;
};

}

#endif
