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
#ifndef WBE_FILE_RENDER_TASK_HH
#define WBE_FILE_RENDER_TASK_HH

#include "core/allocator/allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_object.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

using RendResID = HashCode;
using RendResTID = HashCode;
using RendTaskID = HashCode;

/**
 * @class RendResInfo
 * @brief Render resource info.
 *
 */
struct RendResInfo {
    // The ID of this render resource. Each resource should have a unique res_id.
    RendResID res_id;
    // The type of the render reesource. Each type should have a unique type_id.
    RendResTID type_id;
};

/**
 * @class RenderTask
 * @brief The abstract struct for render task.
 *
 */
struct RenderTask {
    WBE_R6_DEFAULT_VIRTUAL(RenderTask)

    /**
     * @brief Initialize the render task (since render task does not follow RAII, meaning
     * calling the constructor does not fully initialize a render task, this method could provide
     * a place where the full initialization could be performed).
     */
    virtual void init() = 0;

    /**
     * @brief Perform the render task.
     */
    virtual void perform(double p_delta) = 0;
};

/**
 * @class RendPipelineInfo
 * @brief The information of a render pipeline.
 *
 */
struct RendPipelineInfo {
    // The root task of the pipeline.
    Ref<struct RendTaskInfo> root_task;
    /**
     * @class Resource
     * @brief Represents a resource.
     *
     */
    struct Resource {
        // The ID of the resource.
        RendResID res_id;
        // The type ID of the resource.
        RendResTID type_id;
        // Is the resource a weak binding.
        bool is_weak = false;
        // The resource instance. If is_weak is false, the previously registered resource with
        // the same res_id will be replaced by this instance. If is_weak is true, it
        // will try to find resources that was registered before by other pipelines, and
        // use those as the resource here. If none reference was found, it will be replaced
        // with this one. When is_weak is true, or when is_weak is false and there is no previously
        // registered resource with the same res_id, res should not be MEM_NULL.
        Ref<RenderObject> res = MEM_NULL;
    };
    // All the resources used in the render pipeline.
    std::vector<Resource> resources;
};

/**
 * @class RendTaskInfo
 * @brief Render task info.
 *
 */
struct RendTaskInfo {
    // The ID of this render task. Each render task should have a unique task_id.
    RendTaskID task_id;

    // Previous render task.
    std::vector<RendTaskID> prev;
    // Next render tasks.
    std::vector<Ref<RendTaskInfo>> next;

    // Required resources.
    std::vector<RendResInfo> req_res;

    // The render task instance.
    Ref<class RenderTaskFactory> render_task_factory = MEM_NULL;
};

/**
 * @class RenderTaskFactory
 * @brief The factory to create RenderTask.
 *
 */
class RenderTaskFactory {
public:
    WBE_R6_DEFAULT_VIRTUAL(RenderTaskFactory)

    /**
     * @brief Create the render task instance.
     */
    virtual Ref<RenderTask> create() const = 0;
};

} // namespace WhiteBirdEngine

#endif
