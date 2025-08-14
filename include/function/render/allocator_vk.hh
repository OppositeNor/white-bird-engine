#ifndef __WBE_ALLOCATOR_VK_HH__
#define __WBE_ALLOCATOR_VK_HH__

#include "core/allocator/heap_allocator.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class AllocatorVK
 * @brief Allocator for Vulkan.
 *
 */
class AllocatorVK {
public:

    ~AllocatorVK();
    AllocatorVK(const AllocatorVK&) = delete;
    AllocatorVK(AllocatorVK&&) = delete;
    AllocatorVK& operator=(const AllocatorVK&) = delete;
    AllocatorVK& operator=(AllocatorVK&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator.
     */
    AllocatorVK(HeapAllocator* p_allocator);

    /**
     * @brief Get the allocation callback.
     *
     * @return The allocation callback.
     */
    VkAllocationCallbacks* get_allocation_callback();

private:
    static void* allocation(AllocatorVK* p_this, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope);
    static void* reallocation(AllocatorVK* p_this, void* p_original, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope);
    static void* free(AllocatorVK* p_this, void* p_memory);

    VkAllocationCallbacks allocator_callbacks;
    HeapAllocator* allocator;
};

}

#endif
