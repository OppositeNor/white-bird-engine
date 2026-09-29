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
#include "function/render/render_task_resource_map.hh"
#include "function/render/render_defs.hh"
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

RenderTaskResourceMap::RenderTaskResourceMap(RenderAllocator* p_allocator) : entries(p_allocator), used(p_allocator) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("RenderTaskResourceMap: allocator is null.");
    }
}

RenderTaskResourceMap& RenderTaskResourceMap::bind(std::string_view p_task_resource_id, std::string_view p_graph_resource_id) {
    if (p_task_resource_id.empty() || p_graph_resource_id.empty()) {
        throw std::runtime_error("RenderTaskResourceMap: resource identifiers must not be empty.");
    }
    for (const Entry& entry : entries) {
        if (entry.source == p_task_resource_id) {
            throw std::runtime_error(std::format("RenderTaskResourceMap: resource '{}' is mapped more than once.", p_task_resource_id));
        }
    }
    entries.emplace_back(std::string(p_task_resource_id), std::string(p_graph_resource_id));
    used.push_back(0);
    return *this;
}

std::string_view RenderTaskResourceMap::resolve(std::string_view p_task_resource_id) const {
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].source == p_task_resource_id) {
            used[i] = 1;
            return entries[i].target;
        }
    }
    return p_task_resource_id;
}

void RenderTaskResourceMap::validate_all_used(std::string_view p_task_id) const {
    for (size_t i = 0; i < entries.size(); ++i) {
        if (used[i] == 0) {
            throw std::runtime_error(std::format(
                "RenderTaskResourceMap: task '{}' maps resource '{}', which the task never uses.", p_task_id, entries[i].source));
        }
    }
}

} // namespace WhiteBirdEngine
