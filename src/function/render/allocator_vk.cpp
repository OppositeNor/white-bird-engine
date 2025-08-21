#include "function/render/allocator_vk.hh"
#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include <algorithm>
#include <cstring>

namespace WhiteBirdEngine {

void* AllocatorVK::allocation(AllocatorVK* p_this, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    if (p_size == 0) {
        return nullptr;
    }
    void* result = reinterpret_cast<void*>(p_this->allocator->allocate(p_size, p_alignment));
    return result;
}

void* AllocatorVK::reallocation(AllocatorVK* p_this, void* p_original, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    if (p_size == 0) {
        free(p_this, p_original);
        return nullptr;
    }
    size_t original_size = p_this->allocator->get_allocated_data_size(reinterpret_cast<MemID>(p_original));
    void* dest = reinterpret_cast<void*>(p_this->allocator->allocate(p_size, p_alignment));
    memcpy(dest, p_original, std::min(original_size, p_size));
    p_this->allocator->deallocate(reinterpret_cast<MemID>(p_original));
    return dest;
}

void AllocatorVK::free(AllocatorVK* p_this, void* p_memory) {
    if (p_memory == nullptr) {
        return;
    }
    p_this->allocator->deallocate(reinterpret_cast<MemID>(p_memory));
}
}
