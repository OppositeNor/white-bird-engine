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

#include "function/render_vk/render_database_vk.hh"
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/surface/3d_primitive.hh"
#include "function/render/material.hh"
#include "global/stl_allocator.hh"
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace WhiteBirdEngine {

RenderDatabaseVK::RenderDatabaseVK(HeapAllocatorAlignedPool* p_allocator)
    : static_opaque_triangle_vertices(p_allocator), static_opaque_triangle_indices(p_allocator) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Failed to create renderer database: allocator is null.");
    }
    allocator = p_allocator;
}

void RenderDatabaseVK::set_static_opaque_triangles(vectorp<Vertex3DTextured>&& p_vertices,
                                                   vectorp<std::pair<MaterialID, Triangle3DIndxTextured>>&& p_indices) {
    static_opaque_triangle_map.clear();
    static_opaque_triangle_vertices = std::move(p_vertices);
    static_opaque_triangle_vertices.swap(static_opaque_triangle_vertices);  // Make the vector memory space compact
    static_opaque_triangle_indices = vectorp<uint32_t>(allocator);

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
