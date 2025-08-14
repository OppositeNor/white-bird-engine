#include "function/render/allocator_vk.hh"

namespace WhiteBirdEngine {

AllocatorVK::AllocatorVK(HeapAllocator* p_allocator)
    : allocator(p_allocator) {
    // TODO
}

VkAllocationCallbacks* AllocatorVK::get_allocation_callback() {
    return nullptr; // TODO
}

void* AllocatorVK::allocation(AllocatorVK* p_this, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    return nullptr; // TODO
}

void* AllocatorVK::reallocation(AllocatorVK* p_this, void* p_original, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    return nullptr; // TODO
}

void* AllocatorVK::free(AllocatorVK* p_this, void* p_memory) {
    return nullptr; // TODO
}
}
