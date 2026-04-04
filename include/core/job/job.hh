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
#ifndef WBE_FILE_JOB_HH
#define WBE_FILE_JOB_HH

#include "utils/defs.hh"
#include <functional>
#include <utility>

namespace WhiteBirdEngine {

/**
 * @class Job
 * @brief A job.
 *
 */
class Job final {
public:
    Job() = delete;
    ~Job() = default;
    WBE_R6_NDCD_DELETE_COPY_MOVE(Job)
    /**
     * @brief Constructor.
     *
     * @param p_job The job function.
     */
    Job(const std::function<void()>& p_job)
    : job(p_job) {}

    Job(std::function<void()>&& p_job)
    : job(std::move(p_job)) {}

    void perform() {
        job();
    }
private:
    std::function<void()> job;
};

} // namespace WhiteBirdEngine

#endif
