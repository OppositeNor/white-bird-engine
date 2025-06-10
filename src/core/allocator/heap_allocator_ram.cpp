#include "core/allocator/heap_allocator_ram.h"
#include "core/global/global.h"

namespace WhiteBirdEngine {

HeapAllocatorRam::~HeapAllocatorRam() {
    // TODO: warning if not empty
    for (auto& iter : alloc_map) {
        free(iter.second);
    }
}

MemID HeapAllocatorRam::allocate(size_t p_size) {
    void* result = malloc(p_size);
    allocated.insert(curr_id);
    alloc_map[curr_id] = result;
    return curr_id++;
}

void HeapAllocatorRam::deallocate(MemID p_mem) {
    auto iter = allocated.find(p_mem);
    if (iter == allocated.end()) {
        throw std::runtime_error("Failed to deallocate memory: memory not allocated by this allocator.");
    }
    free(get(p_mem));
    allocated.erase(p_mem);
    alloc_map.erase(p_mem);
}


}
