#include "utils/allocator/allocator_ram.h"

namespace PencilEngine {

AllocatorRam::~AllocatorRam() {
    for (auto& iter : allocated) {
        free(iter);
    }
}

void* AllocatorRam::allocate(size_t p_size) {
    void* result = malloc(p_size);
    allocated.insert(result);
    return result;
}

void AllocatorRam::deallocate(void* p_mem) {
    auto iter = allocated.find(p_mem);
    if (iter == allocated.end()) {
        throw std::runtime_error("Failed to deallocate memory: memory not allocated by this allocator.");
    }
    free(p_mem);
}


}
