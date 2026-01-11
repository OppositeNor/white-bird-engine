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
#include "core/memory/reference_strong.hh"
#include "global/stl_allocator.hh"
#include "job_buffer.hh"
#include "utils/utils.hh"
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

    WBE_R6_NDC_DELETE_COPY_MOVE_OVERRIDE(JobBufferRingSPSC)

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator this buffer uses.
     * @param p_buffer_size The size of the buffer.
     */
    JobBufferRingSPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size);

    Ref<JobType> retrieve_job(bool p_block = false);
    void add_job(Ref<JobType> p_job);

    std::counting_semaphore<32>& get_semaphore() {
        return semaphore;
    }

    void add_to_deref(Ref<JobType> p_job);

    void clear_to_deref();

private:
    std::counting_semaphore<32> semaphore{0};
    Vector<Ref<JobType>> buffer;
    WBE_NO_FALSE_SHARING std::atomic<size_t> head;
    WBE_NO_FALSE_SHARING std::atomic<size_t> tail;
};

template <typename JobType>
JobBufferRingSPSC<JobType>::JobBufferRingSPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size)
    : buffer(p_allocator), head(0), tail(0) {
    if (p_buffer_size <= 1) {
        throw std::runtime_error("Buffer has to be at least size 2.");
    }
    buffer.resize(p_buffer_size);
}

template <typename JobType>
Ref<JobType> JobBufferRingSPSC<JobType>::retrieve_job(bool p_block) {
    if (p_block) {
        semaphore.acquire();
    }
    else if (!semaphore.try_acquire()) {
        return MEM_NULL;
    }
    size_t tail_l = tail.load(std::memory_order_acquire);
    Ref<JobType> result = buffer[tail_l];
    // We don't dereference buffer[tail_l] here because it might cause the object to be destroyed.
    // And since the object would be allocated in the producer thread, destroying it at the consumer thread
    // requires an atomic pool, which desatisfies the lock-free requirement. So the job will be referenced
    // until the producer thread adds a new job, which overwrites the old reference and dereferences it safely.
    // This is not ideal, so a lock-free deallocation design would be better in the future.
    tail.store(ring_increment(tail_l, buffer.size()), std::memory_order_release);
    return result;
}

template <typename JobType>
void JobBufferRingSPSC<JobType>::add_job(Ref<JobType> p_job) {
    size_t head_l = head.load(std::memory_order_relaxed);
    size_t next = ring_increment(head_l, buffer.size());
    if (next == tail.load(std::memory_order_acquire)) {
        throw std::runtime_error("Buffer overflow.");
    }

    buffer[head_l] = p_job;
    head.store(next, std::memory_order_release);
    semaphore.release();
}
} // namespace WhiteBirdEngine

#endif
