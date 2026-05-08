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
#ifndef WBE_FILE_RENDER_GRAPH_DESCRIPTOR_VK_HH
#define WBE_FILE_RENDER_GRAPH_DESCRIPTOR_VK_HH

#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "global/stl_allocator.hh"
#include <functional>

namespace WhiteBirdEngine {

/**
 * @class RendTaskDescVK
 * @brief A high-level Vulkan render task descriptor. A single std::function
 * defers the concrete RenderTaskVK instantiation until the graph is built.
 */
struct RendTaskDescVK {
    RendTaskDescVK(RenderAllocator* p_allocator) : prev(p_allocator) {
    }
    // The id of this task within the pipeline.
    RendTaskID task_id = 0;
    // The ids of tasks that must complete before this task starts. Used by the
    // graph to insert execution edges.
    Vector<RendTaskID, RenderAllocator> prev;
    // Factory invoked at graph-build time to construct the concrete task. The
    // resulting Unique<RenderTaskVK> is owned by the graph.
    std::function<Unique<RenderTaskVK>()> create;
};

/**
 * @class RendPipelineDescVK
 * @brief A high-level Vulkan render pipeline descriptor. Resources are
 * concrete RendResVK (containing Ref<RenderObjectVK>), and tasks reference
 * each other by id.
 */
struct RendPipelineDescVK {
    RendPipelineDescVK(RenderAllocator* p_allocator) : tasks(p_allocator), resources(p_allocator) {
    }
    // The set of tasks in the pipeline. Order is irrelevant; the graph topo-
    // sorts them based on `prev` edges.
    Vector<RendTaskDescVK, RenderAllocator> tasks;
    // The set of resources used by the pipeline. Looked up by res_id.
    Vector<RendResVK, RenderAllocator> resources;
};

} // namespace WhiteBirdEngine

#endif
