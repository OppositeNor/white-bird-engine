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
#ifndef WBE_FILE_JOB_BUFFER_HH
#define WBE_FILE_JOB_BUFFER_HH

#include "core/memory/reference_strong.hh"
#include "utils/defs.hh"
#include <semaphore>
namespace WhiteBirdEngine {

/**
 * @class JobBuffer
 * @brief Stores a buffer that contians all the job for an instance to execute.
 */
template <typename ChildT, typename JobT>
class JobBuffer {
public:
    WBE_R6_DELETE_COPY_MOVE_VIRTUAL(JobBuffer)
    using JobType = JobT;

    /**
     * @brief Retrieve a job.
     *
     * @param p_block Block and wait for a new job if the buffer is empty.
     * @return THe instance of a job. Or MEM_NULL if p_block is false, and the buffer is empty.
     */
    Ref<JobType> retrieve_job(bool p_block = false) {
        return static_cast<ChildT*>(this)->retrieve_job(p_block);
    }

    /**
     * @brief Add a job to the buffer.
     * 
     * @throws std::runtime_error If buffer overflow.
     * @param p_job The job to add to the buffer.
     */
    void add_job(Ref<JobType> p_job) {
        return static_cast<ChildT*>(this)->add_job(p_job);
    }

    /**
     * @brief Get the semaphore.
     *
     * @return The semaphore.
     */
    std::counting_semaphore<32>& get_semaphore() {
        return static_cast<ChildT*>(this)->get_semaphore();
    }
};

} // namespace WhiteBirdEngine

#endif
