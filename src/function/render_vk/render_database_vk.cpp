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
#include <cstring>
#include <stdexcept>

namespace WhiteBirdEngine {

RenderDatabaseVK::RenderDatabaseVK(HeapAllocatorAlignedPool* p_allocator)
: static_opaque_triangles(p_allocator), dynamic_opaque_triangles(p_allocator) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Failed to create renderer database: allocator is null.");
    }
    allocator = p_allocator;
}

void RenderDatabaseVK::set_static_opaque_triangles(vectorp<std::pair<MaterialID, Triangle3DTextured>>&& p_triangles) {
    static_opaque_triangle_map.clear();
    static_opaque_triangles = vectorp<Triangle3DTextured>(allocator);
    static_opaque_triangles.reserve(p_triangles.size());

    using TriType = std::pair<MaterialID, Triangle3DTextured>;
    std::sort(p_triangles.begin(), p_triangles.end(), [](
        const TriType& p_tri1, const TriType& p_tri2) -> bool {
              return p_tri1.first < p_tri2.first;
              });
    MaterialID current_id = MATERIAL_INVALID;
    for (auto& tri : p_triangles) {
        if (tri.first != current_id) {
            static_opaque_triangle_map[tri.first].first = 0;
            static_opaque_triangle_map[tri.first].second = static_opaque_triangles.data() + static_opaque_triangles.size();
            current_id = tri.first;
        }
        static_opaque_triangles.push_back(tri.second);
        ++(static_opaque_triangle_map[tri.first].first);
    }
    p_triangles.clear();
}

void RenderDatabaseVK::set_dynamic_opaque_triangles(vectorp<std::pair<MaterialID, Triangle3DTextured>>&& p_triangles) {
    dynamic_opaque_triangle_map.clear();
    for (auto& tri : p_triangles) {
        auto iter = dynamic_opaque_triangle_map.find(tri.first);
        if (iter == dynamic_opaque_triangle_map.end()) {
            dynamic_opaque_triangle_map.insert(std::pair(tri.first, vectorp<Triangle3DTextured>(1, tri.second, allocator)));
        }
        else {
            iter->second.push_back(tri.second);
        }
    }
    p_triangles.clear();
    is_dynamic_opaque_triangles_dirty = true;
}

void RenderDatabaseVK::set_dynamic_opaque_triangles(MaterialID p_material_id, vectorp<Triangle3DTextured>&& p_triangles) {
    auto iter = dynamic_opaque_triangle_map.find(p_material_id);
    if (iter == dynamic_opaque_triangle_map.end()) {
        dynamic_opaque_triangle_map.insert(std::pair(p_material_id, std::move(p_triangles)));
    }
    else {
        iter->second = std::move(p_triangles);
    }
    is_dynamic_opaque_triangles_dirty = true;
}

void RenderDatabaseVK::update_cached_dynamic_opaque_triangles() const {
    if (!is_dynamic_opaque_triangles_dirty) {
        return;
    }
    size_t total_triangles = 0;
    for (auto& triangles : dynamic_opaque_triangle_map) {
        total_triangles += triangles.second.size();
    }
    dynamic_opaque_triangles = vectorp<Triangle3DTextured>(allocator);
    dynamic_opaque_triangles.reserve(total_triangles);
    for (auto& triangles : dynamic_opaque_triangle_map) {
        dynamic_opaque_triangles.insert(dynamic_opaque_triangles.end(), 
                                      triangles.second.begin(), 
                                      triangles.second.end());
    }
    is_dynamic_opaque_triangles_dirty = false;
}

void RenderDatabaseVK::add_dynamic_opaque_triangles(MaterialID p_mat_id, Triangle3DTextured* p_triangles, size_t p_num) {
    auto iter = dynamic_opaque_triangle_map.find(p_mat_id);
    if (iter == dynamic_opaque_triangle_map.end()) {
        throw std::runtime_error("Failed to add triangle");
    }
    is_dynamic_opaque_triangles_dirty = true;
    size_t old_size = iter->second.size();
    iter->second.resize(old_size + p_num);
    memcpy(iter->second.data() + old_size, p_triangles, p_num * sizeof(Triangle3DTextured));
}

}
