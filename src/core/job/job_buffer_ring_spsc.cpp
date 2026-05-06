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
#include "utils/utils.hh"
#include <atomic>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

JobBufferRingSPSC::JobBufferRingSPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size)
    : buffer(p_allocator), head(0), tail(0) {
    if (p_buffer_size <= 1) {
        throw std::runtime_error("Buffer has to be at least size 2.");
    }
    buffer.resize(p_buffer_size);
}

std::function<void()> JobBufferRingSPSC::retrieve_job(bool p_block) {
    if (p_block) {
        semaphore.acquire();
    } else if (!semaphore.try_acquire()) {
        return std::function<void()>();
    }
    size_t tail_l = tail.load(std::memory_order_acquire);
    std::function<void()> result = std::move(buffer[tail_l]);
    buffer[tail_l] = nullptr;
    tail.store(ring_increment(tail_l, buffer.size()), std::memory_order_release);
    return result;
}

void JobBufferRingSPSC::add_job(std::function<void()> p_job) {
    size_t head_l = head.load(std::memory_order_relaxed);
    size_t next = ring_increment(head_l, buffer.size());
    if (next == tail.load(std::memory_order_acquire)) {
        throw std::runtime_error("Buffer overflow.");
    }

    buffer[head_l] = std::move(p_job);
    head.store(next, std::memory_order_release);
    semaphore.release();
}

} // namespace WhiteBirdEngine
