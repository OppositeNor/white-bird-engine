#include "core/task/task_buffer_ring_spsc.hh"
#include "core/allocator/allocator.hh"
#include <atomic>
#include <stdexcept>

namespace WhiteBirdEngine {

#define INCREMENT(x) ((x) + 1) % buffer.size()

TaskBufferRingSPSC::TaskBufferRingSPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size)
    : buffer(p_allocator), head(0), tail(0) {
    if (p_buffer_size <= 1) {
        throw std::runtime_error("Buffer has to be at least size 2.");
    }
    buffer.resize(p_buffer_size);
}

Ref<Task> TaskBufferRingSPSC::retrieve_task() {
    size_t tail_l = tail.load(std::memory_order_acquire);

    if (tail_l == head.load(std::memory_order_acquire)) {
        return MEM_NULL;
    }
    Ref<Task> result = buffer[tail_l];
    tail.store(INCREMENT(tail_l), std::memory_order_release);
    return result;
}

void TaskBufferRingSPSC::add_task(Ref<Task> p_task) {
    size_t head_l = head.load(std::memory_order_relaxed);
    size_t next = INCREMENT(head_l);
    if (next == tail.load(std::memory_order_acquire)) {
        throw std::runtime_error("Buffer overflow.");
    }

    buffer[head_l] = p_task;
    head.store(next, std::memory_order_release);
}

}
