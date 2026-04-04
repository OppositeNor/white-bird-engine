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
#include "core/job/job_buffer_ring_spsc.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/job/job.hh"
#include "core/allocator/i_allocator.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <stdexcept>
#include <atomic>

namespace WhiteBirdEngine {

JobBufferRingSPSC::JobBufferRingSPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size)
    : buffer(p_allocator), head(0), tail(0) {
    if (p_buffer_size <= 1) {
        throw std::runtime_error("Buffer has to be at least size 2.");
    }
    buffer.resize(p_buffer_size);
}

Ref<Job> JobBufferRingSPSC::retrieve_job(bool p_block) {
    if (p_block) {
        semaphore.acquire();
    }
    else if (!semaphore.try_acquire()) {
        return MEM_NULL;
    }
    size_t tail_l = tail.load(std::memory_order_acquire);
    Ref<Job> result = buffer[tail_l];
    // We don't dereference buffer[tail_l] here because it might cause the Job object to be destroyed.
    // And since the object was allocated in the producer thread, destroying it at the consumer thread
    // requires an atomic pool, which desatisfies the lock-free requirement. So the job will be referenced
    // until the producer thread adds a new job, which overwrites the old reference and dereferences it safely.
    // This is not ideal, so a lock-free deallocation design would be better in the future.
    tail.store(ring_increment(tail_l, buffer.size()), std::memory_order_release);
    return result;
}

void JobBufferRingSPSC::add_job(Ref<Job> p_job) {
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
