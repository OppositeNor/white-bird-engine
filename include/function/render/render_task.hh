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
#ifndef __WBE_RENDER_TASK_HH__
#define __WBE_RENDER_TASK_HH__

#include "core/allocator/allocator.hh"
#include "core/memory/reference_raw.hh"
#include "utils/utils.hh"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

using RendResID = HashCode;
using RendResTID = HashCode;
using RendTaskID = HashCode;

struct RendResInfo {
    RendResID res_id;
    RendResTID type_id;
};

/**
 * @class RenderTask
 * @brief The abstract struct for render task.
 *
 */
struct RenderTask {
    RenderTask() = default;
    virtual ~RenderTask() {};
    /**
     * @brief Perform the render task.
     */
    virtual void operator()(double p_delta) = 0;
};

struct RenderTaskInfo {
    // The ID of this render task.
    RendTaskID render_task_id;

    // Previous render task.
    std::vector<RendTaskID> prev;
    // Next render tasks.
    std::vector<RendTaskID> next;

    // Required resources.
    std::vector<RendResID> req_res;
    // Produced resources.
    std::vector<RendResInfo> prod_res;

    // The render task instance.
    RefRaw<RenderTask> render_task = MEM_NULL;
};


}

#endif
