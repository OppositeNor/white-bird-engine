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
#ifndef __WBE_RENDER_PIPELINE_HH__
#define __WBE_RENDER_PIPELINE_HH__

#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class RenderPipeline
 * @brief Graphics pipeline class.
 *
 */
class RenderPipeline {
public:

    RenderPipeline() = default;
    ~RenderPipeline() {}
    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline(RenderPipeline&&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;
    RenderPipeline& operator=(RenderPipeline&&) = delete;

    /**
     * @brief Involke the tasks.
     *
     * @param p_delta The time difference from the last frame.
     */
    void involke(double p_delta) {
        for (auto& task : render_tasks) {
            task->perform(p_delta);
        }
    }
protected:

    /**
     * @brief Add a render task. The render tasks will be involked in the
     * sequence they are added.
     *
     * @param p_render_task The render task to be added.
     */
    void add_render_task(Ref<RenderTask> p_render_task) {
        render_tasks.push_back(p_render_task);
    }

private:
    std::vector<Ref<RenderTask>> render_tasks;
};

}

#endif
