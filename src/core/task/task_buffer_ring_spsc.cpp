#include "core/task/task_buffer_ring_spsc.hh"
#include "core/allocator/allocator.hh"
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
    return MEM_NULL; // TODO
}

void TaskBufferRingSPSC::add_task(Ref<Task> p_task) {
    // TODO
}

}
