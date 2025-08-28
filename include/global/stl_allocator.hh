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
#ifndef __WBE_STL_ALLOCATOR_HH__
#define __WBE_STL_ALLOCATOR_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include <cstddef>
#include <set>
#include <string>
#include <vector>
namespace WhiteBirdEngine {

/**
 * @class STLAllocator
 * @brief Allocator for C++ standard library.
 * @todo Test
 *
 */
template <typename T, typename AllocType> requires (!AllocatorTrait<AllocType>::WILL_ADDR_MOVE && AllocatorTrait<AllocType>::IS_ALIGNABLE)
struct STLAllocator {
    using value_type = T;
    AllocType* allocator = nullptr;

    template <typename U>
    struct rebind {
        using other = STLAllocator<U, AllocType>;
    };

    STLAllocator(AllocType* p_allocator)
        : allocator(p_allocator) {}
    ~STLAllocator() {}
    STLAllocator(const STLAllocator& p_other)
        : allocator(p_other.allocator) {}
    STLAllocator(STLAllocator&& p_other)
        : allocator(p_other.allocator) {
        p_other.allocator = nullptr;
    }
    STLAllocator& operator=(const STLAllocator& p_other) {
        allocator = p_other.allocator;
        return *this;
    }
    STLAllocator& operator=(STLAllocator&& p_other) {
        allocator = p_other.allocator;
        p_other.allocator = nullptr;
        return *this;
    }

    template <typename T1>
    STLAllocator(const STLAllocator<T1, AllocType>& p_other) noexcept
        : allocator(p_other.allocator) {}

    
    T* allocate(size_t p_num) {
        return reinterpret_cast<T*>(allocator->allocate(p_num * sizeof(T), alignof(T)));
    }
    void deallocate(T* p_ptr, size_t p_num) {
        allocator->deallocate(reinterpret_cast<MemID>(p_ptr));
    }
};

template <typename T, typename AllocType = HeapAllocatorAlignedPool>
using vector = std::vector<T, STLAllocator<T, AllocType>>;

template <typename T, typename AllocType = HeapAllocatorAlignedPool>
using set = std::set<T, std::less<T>, STLAllocator<T, AllocType>>;

template <typename AllocType = HeapAllocatorAlignedPool>
using string = std::basic_string<char, std::char_traits<char>, STLAllocator<char, AllocType>>;

}

#endif
