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

#include "core/reflection/reflection_defs.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"

namespace WhiteBirdEngine {

using RendResID = HashCode;
using RendResTID = HashCode;
using RendTaskID = HashCode;

WBE_LABEL(RT_UNDEFINED, WBE_RENDER_TASK);

/**
 * @class RenderTask
 * @brief The abstract struct for render task.
 */
struct RenderTask {
    RenderTask() = default;
    virtual ~RenderTask() = default;
    RenderTask(const RenderTask&) = default;
    RenderTask(RenderTask&&) noexcept = default;
    RenderTask& operator=(const RenderTask&) = default;
    RenderTask& operator=(RenderTask&&) noexcept = default;

    /**
     * @brief Initialize the render task.
     */
    virtual void init() = 0;

    /**
     * @brief Perform the render task.
     */
    virtual void perform(double p_delta) = 0;
};

} // namespace WhiteBirdEngine

#endif
