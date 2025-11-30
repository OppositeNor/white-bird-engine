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
#ifndef __WBE_JOB_BUFFER_HH__
#define __WBE_JOB_BUFFER_HH__

#include "core/memory/reference_strong.hh"
namespace WhiteBirdEngine {

/**
 * @class JobBuffer
 * @brief Stores a buffer that contians all the job for an instance to execute.
 */
template <typename ChildT, typename JobT>
class JobBuffer {
public:
    JobBuffer() = default;
    virtual ~JobBuffer() {}

    using JobType = JobT;

    /**
     * @brief Retrieve a job from a buffer. If the buffer is empty, return MEM_NULL.
     */
    Ref<JobType> retrieve_job() {
        return static_cast<ChildT*>(this)->retrieve_job();
    }

    /**
     * @brief Add a job to the buffer.
     * 
     * @throws std::runtime_error If buffer overflow.
     * @param p_job The job to add to the buffer.
     */
    void add_job(Ref<JobType> p_job) {
        return static_cast<ChildT*>(this)->add_job();
    }
};

}

#endif
