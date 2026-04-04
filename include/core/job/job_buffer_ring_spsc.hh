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
#ifndef WBE_FILE_JOB_BUFFER_RING_SPSC_HH
#define WBE_FILE_JOB_BUFFER_RING_SPSC_HH

#include "core/core_utils.hh"
#include "core/job/job.hh"
#include "core/memory/reference_strong.hh"
#include "global/stl_allocator.hh"
#include "job_buffer.hh"
#include "utils/defs.hh"
#include <atomic>
#include <cstddef>
#include <semaphore>

namespace WhiteBirdEngine {

/**
 * @class JobBufferRingSPSC
 *
 * @tparam JobT The type of the job.
 * @brief Job buffer, spsc ring buffer version.
 *
 */
class JobBufferRingSPSC final : public JobBuffer<JobBufferRingSPSC> {
public:

    WBE_R6_NDC_DELETE_COPY_MOVE_OVERRIDE(JobBufferRingSPSC)

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator this buffer uses.
     * @param p_buffer_size The size of the buffer.
     */
    JobBufferRingSPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size);

    Ref<Job> retrieve_job(bool p_block = false);
    void add_job(Ref<Job> p_job);

    std::counting_semaphore<32>& get_semaphore() {
        return semaphore;
    }

    void add_to_deref(Ref<Job> p_job);

    void clear_to_deref();

private:
    std::counting_semaphore<32> semaphore{0};
    Vector<Ref<Job>> buffer;
    WBE_NO_FALSE_SHARING std::atomic<size_t> head;
    WBE_NO_FALSE_SHARING std::atomic<size_t> tail;
};
} // namespace WhiteBirdEngine

#endif
