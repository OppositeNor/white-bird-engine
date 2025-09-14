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
#include "core/surface/3d_primitive.hh"
#include "function/render/material.hh"
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
    static_opaque_triangles.clear();
    static_opaque_triangles.swap(static_opaque_triangles); // Free the previously reserved memory.
    static_opaque_triangles.reserve(p_triangles.size());

    using TriType = std::pair<MaterialID, Triangle3DTextured>;
    std::sort(p_triangles.begin(), p_triangles.end(), [](
        const TriType& p_tri1, const TriType& p_tri2) -> bool {
              return p_tri1.first < p_tri2.first;
              });
    MaterialID track_id = 0;
    for (auto& tri : p_triangles) {
        if (tri.first != track_id) {
            static_opaque_triangle_map[tri.first].first = 0;
            static_opaque_triangle_map[tri.first].second = &(tri.second);
            track_id = tri.first;
        }
        static_opaque_triangles.push_back(tri.second);
        ++(static_opaque_triangle_map[tri.first].first);
    }
    p_triangles.clear();
}

void RenderDatabaseVK::set_dynamic_opaque_triangles(vectorp<std::pair<MaterialID, Triangle3DTextured>>&& p_triangles, bool p_update_immediate) {
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
    if (p_update_immediate) {
        update_dynamic_opaque_triangles();
    }
    else {
        is_dynamic_opaque_triangles_dirty = true;
    }
}

void RenderDatabaseVK::update_dynamic_opaque_triangles() const {
    size_t tracker = 0;
    for (auto& triangles : dynamic_opaque_triangle_map) {
        tracker += triangles.second.size();
    }
    dynamic_opaque_triangles.clear();
    dynamic_opaque_triangles.swap(dynamic_opaque_triangles); // Free the previously reserved memory.
    dynamic_opaque_triangles.resize(tracker);
    tracker = 0;
    for (auto& triangles : dynamic_opaque_triangle_map) {
        memcpy(dynamic_opaque_triangles.data() + tracker, triangles.second.data(), triangles.second.size() * sizeof(Triangle3DTextured));
        tracker += triangles.second.size() * sizeof(Triangle3DTextured);
    }
    is_dynamic_opaque_triangles_dirty = true;
}

}
