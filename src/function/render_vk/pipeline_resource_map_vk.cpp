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
#include "function/render_vk/pipeline_resource_map_vk.hh"
#include <stdexcept>

namespace WhiteBirdEngine {

PipelineResourceMapVK::PipelineResourceMapVK(RenderAllocator* p_allocator) : entries(p_allocator) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("PipelineResourceMapVK: allocator is null.");
    }
}

PipelineResourceMapVK& PipelineResourceMapVK::bind(std::string_view p_shader_resource_id, std::string_view p_graph_resource_id) {
    if (p_shader_resource_id.empty() || p_graph_resource_id.empty()) {
        throw std::runtime_error("PipelineResourceMapVK: resource identifiers must not be empty.");
    }
    for (const Entry& entry : entries) {
        if (entry.source == p_shader_resource_id) {
            throw std::runtime_error("PipelineResourceMapVK: duplicate source identifier.");
        }
    }
    entries.push_back({std::string(p_shader_resource_id), std::string(p_graph_resource_id)});
    return *this;
}

} // namespace WhiteBirdEngine
