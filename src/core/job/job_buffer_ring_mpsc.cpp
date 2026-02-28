#include "core/job/job_buffer_ring_mpsc.hh"
#include "utils/utils.hh"
#include <stdexcept>

namespace WhiteBirdEngine {

JobBufferRingMPSC::JobBufferRingMPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size)
    : buffer(p_allocator), head(0), tail(0) {
    if (p_buffer_size <= 1) {
        throw std::runtime_error("Buffer has to be at least size 2.");
    }
    buffer.resize(p_buffer_size);
}

void JobBufferRingMPSC::add_job(Ref<Job> p_job) {
    size_t head_l;
    size_t next;

    // Use CAS for lock free increment.
    while (true) {
        head_l = head.load(std::memory_order_relaxed);
        next = ring_increment(head_l, buffer.size());
        if (next == tail.load(std::memory_order_acquire)) {
            throw std::runtime_error("Buffer overflow.");
        }
        if (head.compare_exchange_weak(
                head_l,
                next,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            break;
        }
    }
    buffer[head_l] = p_job;
    std::atomic_thread_fence(std::memory_order_release);
    semaphore.release();
}

Ref<Job> JobBufferRingMPSC::retrieve_job(bool p_block) {
    if (p_block) {
        semaphore.acquire();
    }
    else if (!semaphore.try_acquire()) {
        return MEM_NULL;
    }
    size_t tail_l = tail.load(std::memory_order_acquire);

    Ref<Job> result = buffer[tail_l];
    tail.store(ring_increment(tail_l, buffer.size()), std::memory_order_release);
    return result;
}


} // namespace WhiteBirdEngine

