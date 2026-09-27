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
#ifndef WBE_FILE_PIPELINE_RESOURCE_MAP_VK_HH
#define WBE_FILE_PIPELINE_RESOURCE_MAP_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "function/render/render_defs.hh"
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class PipelineResourceMapVK
 * @brief Copyable and movable, allocator-owned configuration for one pipeline invocation.
 * The allocator must outlive every copy. Lookups are exact and single-step; shader accesses
 * must still be declared explicitly with the actual graph resource identifiers.
 */
class PipelineResourceMapVK {
public:
    PipelineResourceMapVK() = delete;
    /**
     * @brief Create an empty mapping using an allocator that outlives this configuration.
     */
    explicit PipelineResourceMapVK(RenderAllocator* p_allocator);
    ~PipelineResourceMapVK() = default;
    PipelineResourceMapVK(const PipelineResourceMapVK&) = default;
    PipelineResourceMapVK(PipelineResourceMapVK&&) noexcept = default;
    PipelineResourceMapVK& operator=(const PipelineResourceMapVK&) = default;
    PipelineResourceMapVK& operator=(PipelineResourceMapVK&&) noexcept = default;

    /**
     * @brief Map a reflected resource to a graph resource. Empty IDs and duplicate sources throw.
     * Tasks snapshot this configuration at construction; subsequent changes do not affect them.
     */
    PipelineResourceMapVK& bind(std::string_view p_shader_resource_id, std::string_view p_graph_resource_id);

private:
    friend class PipelineDescriptorBindingsVK;
    struct Entry {
        std::string source;
        std::string target;
    };
    Vector<Entry, RenderAllocator> entries;
};

} // namespace WhiteBirdEngine

#endif
