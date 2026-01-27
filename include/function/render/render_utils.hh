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
#ifndef WBE_FILE_RENDER_UTILS_HH
#define WBE_FILE_RENDER_UTILS_HH

#include "core/job/job.hh"
#include "function/render/renderer.hh"
#include "utils/defs.hh"

namespace WhiteBirdEngine {

/**
 * @class RenderJob
 * @brief A render job.
 *
 */
class JobRender : public Job<JobRender> {
public:
    WBE_R6_DEFAULT_OVERRIDE(JobRender)
    /**
     * @brief Perform the job.
     *
     * @param p_renderer The renderer instance.
     */
    virtual void perform(Ref<Renderer> p_renderer) = 0;
};

/**
 * @class JobRenderWrapper
 * @brief A job render wrapper for std::function.
 *
 */
class JobRenderWrapper final : public JobRender {
public:
    WBE_R6_DEFAULT_OVERRIDE(JobRenderWrapper)
    /**
     * @brief Constructor.
     *
     * @param p_job The job function.
     */
    JobRenderWrapper(const std::function<void(Ref<Renderer>)>& p_job)
    : job(p_job) {}

    JobRenderWrapper(std::function<void(Ref<Renderer>)>&& p_job)
    : job(std::move(p_job)) {}

    virtual void perform(Ref<Renderer> p_renderer) override {
        job(p_renderer);
    }
private:
    std::function<void(Ref<Renderer>)> job;
};

} // namespace WhiteBirdEngine

#endif
