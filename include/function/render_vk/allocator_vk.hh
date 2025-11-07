/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef __WBE_ALLOCATOR_VK_HH__
#define __WBE_ALLOCATOR_VK_HH__

#include "core/allocator/allocator.hh"
#include "core/core_utils.hh"
#include <cstring>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class AllocatorVK
 * @brief Allocator for Vulkan.
 *
 */
template <typename AllocType = HeapAllocatorDefault>
    requires (!AllocatorTrait<AllocType>::WILL_ADDR_MOVE)
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
    AllocatorVK(AllocType* p_allocator) {
        allocator = p_allocator;
        allocator_callbacks.pUserData = this;
        allocator_callbacks.pfnAllocation = (PFN_vkAllocationFunction)(AllocatorVK::allocation);
        allocator_callbacks.pfnReallocation = (PFN_vkReallocationFunction)(AllocatorVK::reallocation);
        allocator_callbacks.pfnFree = (PFN_vkFreeFunction)(AllocatorVK::free);
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
    static void* allocation(AllocatorVK* p_this, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope);
    static void* reallocation(AllocatorVK* p_this, void* p_original, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope);
    static void free(AllocatorVK* p_this, void* p_memory);

    VkAllocationCallbacks allocator_callbacks{};
    AllocType* allocator;
};

template <typename AllocType>
    requires (!AllocatorTrait<AllocType>::WILL_ADDR_MOVE)
void* AllocatorVK<AllocType>::allocation(AllocatorVK* p_this, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    if (p_size == 0) {
        return nullptr;
    }
    void* result = std::bit_cast<void*>(p_this->allocator->allocate(p_size, p_alignment));
    return result;
}

template <typename AllocType>
    requires (!AllocatorTrait<AllocType>::WILL_ADDR_MOVE)
void* AllocatorVK<AllocType>::reallocation(AllocatorVK* p_this, void* p_original, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    if (p_size == 0) {
        free(p_this, p_original);
        return nullptr;
    }
    size_t original_size = p_this->allocator->get_allocated_data_size(std::bit_cast<MemID>(p_original));
    void* dest = std::bit_cast<void*>(p_this->allocator->allocate(p_size, p_alignment));
    memcpy(dest, p_original, std::min(original_size, p_size));
    p_this->allocator->deallocate(std::bit_cast<MemID>(p_original));
    return dest;
}

template <typename AllocType>
    requires (!AllocatorTrait<AllocType>::WILL_ADDR_MOVE)
void AllocatorVK<AllocType>::free(AllocatorVK* p_this, void* p_memory) {
    if (p_memory == nullptr) {
        return;
    }
    p_this->allocator->deallocate(std::bit_cast<MemID>(p_memory));
}

}

#endif
