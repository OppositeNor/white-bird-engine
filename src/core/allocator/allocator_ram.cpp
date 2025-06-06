#include "core/allocator/allocator_ram.h"

namespace WhiteBirdEngine {

AllocatorRam::~AllocatorRam() {
    // TODO: warning if not empty
    for (auto& iter : alloc_map) {
        free(iter.second);
    }
}

MemID AllocatorRam::allocate(size_t p_size) {
    void* result = malloc(p_size);
    allocated.insert(curr_id);
    alloc_map[curr_id] = result;
    return curr_id++;
}

void AllocatorRam::deallocate(MemID p_mem) {
    auto iter = allocated.find(p_mem);
    if (iter == allocated.end()) {
        throw std::runtime_error("Failed to deallocate memory: memory not allocated by this allocator.");
    }
    free(get(p_mem));
    allocated.erase(p_mem);
    alloc_map.erase(p_mem);
}


}
