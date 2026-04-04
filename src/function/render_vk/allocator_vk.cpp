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
#include "core/allocator/i_allocator.hh"
#include "function/render_vk/allocator_vk.hh"
#include <cstddef>
#include <vulkan/vulkan_core.h>
#include <cstring>
#include <algorithm>

namespace WhiteBirdEngine {

void* AllocatorVK::allocation(AllocatorVK* p_this, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    (void)p_allocation_scope;
    if (p_size == 0) {
        return nullptr;
    }
    return reinterpret_cast<void*>(p_this->allocator->allocate(p_size, p_alignment));
}

void* AllocatorVK::reallocation(AllocatorVK* p_this, void* p_original, size_t p_size, size_t p_alignment, VkSystemAllocationScope p_allocation_scope) {
    (void)p_allocation_scope;
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

} // namespace WhiteBirdEngine
