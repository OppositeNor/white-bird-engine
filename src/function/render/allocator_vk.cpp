#include "function/render/allocator_vk.hh"
#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include <cstring>

namespace WhiteBirdEngine {

void* AllocatorVK::allocation(void* p_this, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    auto alloc_this = static_cast<AllocatorVK*>(p_this);
    return reinterpret_cast<void*>(alloc_this->allocator->allocate(p_size, p_alignment));
}

void* AllocatorVK::reallocation(void* p_this, void* p_original, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    auto alloc_this = static_cast<AllocatorVK*>(p_this);
    void* dest = reinterpret_cast<void*>(alloc_this->allocator->allocate(p_size, p_alignment));
    memcpy(dest, p_original, alloc_this->allocator->get_allocated_data_size(reinterpret_cast<MemID>(p_original)));
    alloc_this->allocator->deallocate(reinterpret_cast<MemID>(p_original));
    return dest;
}

void AllocatorVK::free(void* p_this, void* p_memory) {
    auto alloc_this = static_cast<AllocatorVK*>(p_this);
    alloc_this->allocator->deallocate(reinterpret_cast<MemID>(p_memory));
}
}
