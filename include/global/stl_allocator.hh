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
#include "core/core_utils.hh"
#include <deque>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>
namespace WhiteBirdEngine {

/**
 * @class STLAllocator
 * @brief Adapter allocator for C++ standard library.
 *
 * @tparam T The type of the data.
 * @tparam AllocType The type of the allocator.
 * @tparam POCCA Propagate on container copy assignment.
 * @tparam POCMA Propagate on container move assignment.
 * @tparam POCS Propagate on container swap.
 * @return 
 */
template <typename T, typename AllocType, bool POCCA, bool POCMA, bool POCS>
    requires (!(AllocatorTrait<AllocType>::WILL_ADDR_MOVE))
struct STLAllocator {
    using value_type = T;
    AllocType* allocator = nullptr;

    template <typename U>
    struct rebind {
        using other = STLAllocator<U, AllocType, POCCA, POCMA, POCS>;
    };

    using propagate_on_container_copy_assignment = std::conditional<POCCA, std::true_type, std::false_type>::type;
    using propagate_on_container_move_assignment = std::conditional<POCMA, std::true_type, std::false_type>::type;
    using propagate_on_container_swap = std::conditional<POCS, std::true_type, std::false_type>::type;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator that this STL allocator uses.
     */
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

    bool operator==(const STLAllocator& p_other) {
        return allocator == p_other.allocator;
    }

    /**
     * @brief Copy constructor for another instance with a different value type.
     *
     * @tparam T1 The type of the value.
     * @param p_other The allocator to be copied.
     */
    template <typename T1>
    STLAllocator(const STLAllocator<T1, AllocType, POCCA, POCMA, POCS>& p_other) noexcept
        : allocator(p_other.allocator) {}

    
    /**
     * @brief Allocate memory.
     *
     * @param p_num The number of instances to be allocated.
     * @return The allocated memory.
     */
    T* allocate(size_t p_num) {
        return std::bit_cast<T*>(allocator->allocate(p_num * sizeof(T)));
    }
    /**
     * @brief Deallocate memory
     *
     * @param p_ptr The pointer to the memory been allocated.
     * @param p_num Not used.
     */
    void deallocate(T* p_ptr, size_t) {
        allocator->deallocate((MemID)p_ptr);
    }
};

/**
 * @brief STL vector that uses a custom allocator.
 *
 * @tparam T The type of the values in the vector.
 * @tparam AllocType The type of the allocator. HeapAllocatorDefault by default.
 */
template <typename T, typename AllocType = HeapAllocatorDefault>
using vector = std::vector<T, STLAllocator<T, AllocType, false, true, false>>;

/**
 * @brief STL set that uses a custom allocator.
 *
 * @tparam T The type of the values in the set.
 * @tparam AllocType The type of the allocator. HeapAllocatorDefault by default.
 */
template <typename T, typename AllocType = HeapAllocatorDefault>
using set = std::set<T, std::less<T>, STLAllocator<T, AllocType, false, true, false>>;

/**
 * @brief STL set that uses a custom allocator.
 *
 * @tparam T The type of the values in the set.
 * @tparam AllocType The type of the allocator. HeapAllocatorDefault by default.
 */
template <typename T, typename AllocType = HeapAllocatorDefault>
using unordered_set = std::unordered_set<T, std::hash<T>, std::equal_to<T>, STLAllocator<T, AllocType, false, true, false>>;

/**
 * @brief STL string that uses a custom allocator.
 *
 * @tparam AllocType The type of the allocator. HeapAllocatorDefault by default.
 */
template <typename AllocType = HeapAllocatorDefault>
using string = std::basic_string<char, std::char_traits<char>, STLAllocator<char, AllocType, false, true, false>>;

/**
 * @brief STL deque that uses a custom allocator.
 *
 * @tparam T The type of the values in the vector.
 * @tparam AllocType The type of the allocator. HeapAllocatorDefault by default.
 */
template <typename T, typename AllocType = HeapAllocatorDefault>
using deque = std::deque<T, STLAllocator<T, AllocType, false, true, false>>;

/**
 * @brief Short name for stl allocator that uses HeapAllocatorDefault.
 *
 * @tparam T 
 */
template <typename T>
using STLAllocatorPool = STLAllocator<T, HeapAllocatorDefault, false, true, false>;

}

#endif
