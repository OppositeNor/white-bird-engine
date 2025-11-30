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
#ifndef __WBE_JOB_BUFFER_RING_SPSC_HH__
#define __WBE_JOB_BUFFER_RING_SPSC_HH__

#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "global/stl_allocator.hh"
#include "job_buffer.hh"
#include <cstddef>

namespace WhiteBirdEngine {

/**
 * @class JobBufferRingSPSC
 *
 * @tparam JobT The type of the job.
 * @brief Job buffer, spsc ring buffer version.
 *
 */
template <typename JobT>
class JobBufferRingSPSC final : public JobBuffer<JobBufferRingSPSC<JobT>, JobT> {
public:
    // The type of the job this buffer is holding.
    using JobType = JobT;

    virtual ~JobBufferRingSPSC() override {}
    JobBufferRingSPSC(const JobBufferRingSPSC&) = delete;
    JobBufferRingSPSC(JobBufferRingSPSC&&) = delete;
    JobBufferRingSPSC& operator=(const JobBufferRingSPSC&) = delete;
    JobBufferRingSPSC& operator=(JobBufferRingSPSC&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator this buffer uses.
     * @param p_buffer_size The size of the buffer.
     */
    JobBufferRingSPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size);

    Ref<JobType> retrieve_job();
    void add_job(Ref<JobType> p_job);

private:
    vector<Ref<JobType>> buffer;
    WBE_NO_FALSE_SHARING std::atomic<size_t> head;
    WBE_NO_FALSE_SHARING std::atomic<size_t> tail;
};

#define WBE_RING_INCREMENT(x) ((x) + 1) % buffer.size()

template <typename JobType>
JobBufferRingSPSC<JobType>::JobBufferRingSPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size)
    : buffer(p_allocator), head(0), tail(0) {
    if (p_buffer_size <= 1) {
        throw std::runtime_error("Buffer has to be at least size 2.");
    }
    buffer.resize(p_buffer_size);
}

template <typename JobType>
Ref<JobType> JobBufferRingSPSC<JobType>::retrieve_job() {
    size_t tail_l = tail.load(std::memory_order_acquire);

    if (tail_l == head.load(std::memory_order_acquire)) {
        return MEM_NULL;
    }
    Ref<JobType> result = buffer[tail_l];
    tail.store(WBE_RING_INCREMENT(tail_l), std::memory_order_release);
    return result;
}

template <typename JobType>
void JobBufferRingSPSC<JobType>::add_job(Ref<JobType> p_job) {
    size_t head_l = head.load(std::memory_order_relaxed);
    size_t next = WBE_RING_INCREMENT(head_l);
    if (next == tail.load(std::memory_order_acquire)) {
        throw std::runtime_error("Buffer overflow.");
    }

    buffer[head_l] = p_job;
    head.store(next, std::memory_order_release);
}

#undef WBE_RING_INCREMENT

}

#endif
