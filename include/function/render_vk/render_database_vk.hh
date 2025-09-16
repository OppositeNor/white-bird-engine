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

#include "core/memory/reference_weak.hh"
#include "core/surface/3d_primitive.hh"
#include "core/surface/primitive_slots.hh"
#include "function/render/material.hh"
#include "function/render/texture.hh"
#include "global/stl_allocator.hh"
#include <cstddef>
#include <cstdint>
#include <map>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RenderDatabaseVK
 * @brief Vulkan renderer render database.
 *
 * @todo Test
 */
template <typename AllocType>
class RenderDatabaseVK {
private:
    template <typename T>
    using vector = ::WhiteBirdEngine::vector<T, AllocType>;
    template <typename T>
    using set = ::WhiteBirdEngine::set<T, AllocType>;
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
    RenderDatabaseVK(AllocType* p_allocator);


    /**
     * @brief Set the static opaque triangles.
     *
     * @param p_vertices The vertices of the triangles. Note that this vector have to be at the same memory pool
     * with this render database.
     * @param p_indices The indices of the triangles, with respect to its material.
     */
    void set_static_opaque_triangles(vector<Vertex3DTextured>&& p_vertices,
                                     vector<std::pair<MaterialID, Triangle3DIndxTextured>>&& p_indices);

private:

    AllocType* allocator;
    std::map<MaterialID, std::pair<size_t, size_t>> static_opaque_triangle_map;
    vector<Vertex3DTextured> static_opaque_triangle_vertices;
    vector<uint32_t> static_opaque_triangle_indices;

    std::map<MaterialID, vector<RefWeak<SubMesh<SlotTextured>>*>> dynamic_opaque_sub_meshes;

    std::unordered_map<TextureID, VkImage> texture_map;

};

template <typename AllocType>
RenderDatabaseVK<AllocType>::RenderDatabaseVK(AllocType* p_allocator)
    : static_opaque_triangle_vertices(p_allocator), static_opaque_triangle_indices(p_allocator) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Failed to create renderer database: allocator is null.");
    }
    allocator = p_allocator;
}

template <typename AllocType>
void RenderDatabaseVK<AllocType>::set_static_opaque_triangles(vector<Vertex3DTextured>&& p_vertices,
                                                              vector<std::pair<MaterialID, Triangle3DIndxTextured>>&& p_indices) {
    static_opaque_triangle_map.clear();
    static_opaque_triangle_vertices = std::move(p_vertices);
    static_opaque_triangle_vertices.swap(static_opaque_triangle_vertices);  // Make the vector memory space compact
    static_opaque_triangle_indices = vector<uint32_t>(allocator);

    using TriType = std::pair<MaterialID, Triangle3DIndxTextured>;
    std::sort(p_indices.begin(), p_indices.end(), [](
        const TriType& p_tri1, const TriType& p_tri2) -> bool {
              return p_tri1.first < p_tri2.first;
              });
    MaterialID current_id = MATERIAL_INVALID;
    for (auto& tri : p_indices) {
        if (tri.first != current_id) {
            // Asign the next material ID's indices.
            static_opaque_triangle_map[tri.first].first = 0;
            static_opaque_triangle_map[tri.first].second = static_opaque_triangle_indices.size();
            current_id = tri.first;
        }
        // Asign indices.
        static_opaque_triangle_indices.insert(static_opaque_triangle_indices.end(),
                                              tri.second.vertex_indices, tri.second.vertex_indices + 3);
        static_opaque_triangle_map[tri.first].first += 3;
    }
    p_indices.clear();
    p_indices.swap(p_indices); // Clear resources allocated by indices.
}

}

#endif
