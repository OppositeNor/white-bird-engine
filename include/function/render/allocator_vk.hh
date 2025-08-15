#ifndef __WBE_ALLOCATOR_VK_HH__
#define __WBE_ALLOCATOR_VK_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class AllocatorVK
 * @brief Allocator for Vulkan.
 *
 */
class AllocatorVK {
public:

    ~AllocatorVK() {}
    AllocatorVK(const AllocatorVK&) = delete;
    AllocatorVK(AllocatorVK&&) = delete;
    AllocatorVK& operator=(const AllocatorVK&) = delete;
    AllocatorVK& operator=(AllocatorVK&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator.
     */
    template <typename AllocType> requires (AllocatorTrait<AllocType>::IS_ALIGNABLE && !AllocatorTrait<AllocType>::WILL_ADDR_MOVE)
    AllocatorVK(AllocType* p_allocator) {
        allocator = p_allocator;
        allocator_callbacks.pUserData = this;
        allocator_callbacks.pfnAllocation = AllocatorVK::allocation;
        allocator_callbacks.pfnReallocation = AllocatorVK::reallocation;
        allocator_callbacks.pfnFree = AllocatorVK::free;
    }

    /**
     * @brief Get the allocation callback.
     *
     * @return The allocation callback.
     */
    VkAllocationCallbacks* get_allocation_callbacks() {
        return &allocator_callbacks;
    }

private:
    static void* allocation(void* p_this, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope);
    static void* reallocation(void* p_this, void* p_original, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope);
    static void free(void* p_this, void* p_memory);

    VkAllocationCallbacks allocator_callbacks{};
    HeapAllocatorAligned* allocator;
};

}

#endif
