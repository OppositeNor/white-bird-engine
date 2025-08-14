#include "function/render/allocator_vk.hh"

namespace WhiteBirdEngine {

AllocatorVK::AllocatorVK(HeapAllocator* p_allocator)
    : allocator(p_allocator) {
    allocator_callbacks.pUserData = this;
    allocator_callbacks.pfnAllocation = AllocatorVK::allocation;
    allocator_callbacks.pfnReallocation = AllocatorVK::reallocation;
    allocator_callbacks.pfnFree = AllocatorVK::free;
}

void* AllocatorVK::allocation(void* p_this, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    return nullptr; // TODO
}

void* AllocatorVK::reallocation(void* p_this, void* p_original, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    return nullptr; // TODO
}

void AllocatorVK::free(void* p_this, void* p_memory) {
    // TODO
}
}
