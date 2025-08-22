// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_STL_ALLOCATOR_HH__
#define __WBE_STL_ALLOCATOR_HH__

#include "core/allocator/allocator.hh"
#include <cstddef>
#include <set>
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

    STLAllocator(AllocType* p_allocator)
        : allocator(p_allocator) {}
    
    T* allocate(size_t p_num) {
        return reinterpret_cast<T*>(allocator->allocate(p_num * sizeof(T), alignof(T)));
    }
    void deallocate(T* p_ptr, size_t p_num) {
        allocator->deallocate(reinterpret_cast<MemID>(p_ptr));
    }
};

template <typename T, typename AllocType>
using vector = std::vector<T, STLAllocator<T, AllocType>>;

template <typename T, typename AllocType>
using set = std::set<T, STLAllocator<T, AllocType>>;

}

#endif
