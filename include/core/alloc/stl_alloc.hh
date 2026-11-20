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
#ifndef WBE_FILE_STL_ALLOC_HH
#define WBE_FILE_STL_ALLOC_HH

#include "core/alloc/i_alloc.hh"
#include <boost/container/flat_map.hpp>
#include <cstddef>
#include <deque>
#include <functional>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
namespace WhiteBirdEngine {

// NOLINTBEGIN
/**
 * @class STLAlloc
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
    requires(!(AllocTrait<AllocType>::WILL_ADDR_MOVE))
struct STLAlloc {
    using value_type = T;
    AllocType* alloc = nullptr;

    template <typename U>
    struct rebind {
        using other = STLAlloc<U, AllocType, POCCA, POCMA, POCS>;
    };

    using propagate_on_container_copy_assignment = std::conditional_t<POCCA, std::true_type, std::false_type>;
    using propagate_on_container_move_assignment = std::conditional_t<POCMA, std::true_type, std::false_type>;
    using propagate_on_container_swap = std::conditional_t<POCS, std::true_type, std::false_type>;

    /**
     * @brief Constructor.
     *
     * @param p_alloc The allocator that this STL allocator uses.
     */
    STLAlloc(AllocType* p_alloc) : alloc(p_alloc) {
    }
    ~STLAlloc() = default;
    STLAlloc(const STLAlloc& p_other) : alloc(p_other.alloc) {
    }
    STLAlloc(STLAlloc&& p_other) noexcept : alloc(p_other.alloc) {
        p_other.alloc = nullptr;
    }
    STLAlloc& operator=(const STLAlloc& p_other) {
        if (this == &p_other) {
            return *this;
        }
        alloc = p_other.alloc;
        return *this;
    }
    STLAlloc& operator=(STLAlloc&& p_other) noexcept {
        alloc = p_other.alloc;
        p_other.alloc = nullptr;
        return *this;
    }

    bool operator==(const STLAlloc& p_other) {
        return alloc == p_other.alloc;
    }

    /**
     * @brief Copy constructor for another instance with a different value type.
     *
     * @tparam T1 The type of the value.
     * @param p_other The allocator to be copied.
     */
    template <typename T1>
    STLAlloc(const STLAlloc<T1, AllocType, POCCA, POCMA, POCS>& p_other) noexcept : alloc(p_other.alloc) {
    }

    /**
     * @brief Allocate memory.
     *
     * @param p_num The number of instances to be allocated.
     * @return The allocated memory.
     */
    T* allocate(size_t p_num) {
        return reinterpret_cast<T*>(alloc->allocate(p_num * sizeof(T)));
    }
    /**
     * @brief Deallocate memory
     *
     * @param p_ptr The pointer to the memory been allocated.
     * @param p_num Not used.
     */
    void deallocate(T* p_ptr, size_t) {
        alloc->deallocate(reinterpret_cast<MemID>(p_ptr));
    }
};

/**
 * @brief STL vector that uses a custom allocator.
 *
 * @tparam T The type of the values in the vector.
 * @tparam AllocType The type of the allocator. HeapAllocDefault by default.
 */
template <typename T, typename AllocType>
using Vector = std::vector<T, STLAlloc<T, AllocType, false, true, false>>;

/**
 * @brief STL set that uses a custom allocator.
 *
 * @tparam T The type of the values in the set.
 * @tparam AllocType The type of the allocator. HeapAllocDefault by default.
 */
template <typename T, typename AllocType>
using Set = std::set<T, std::less<T>, STLAlloc<T, AllocType, false, true, false>>;

/**
 * @brief STL set that uses a custom allocator.
 *
 * @tparam T The type of the values in the set.
 * @tparam AllocType The type of the allocator. HeapAllocDefault by default.
 */
template <typename T, typename AllocType>
using HashSet = std::unordered_set<T, std::hash<T>, std::equal_to<T>, STLAlloc<T, AllocType, false, true, false>>;

/**
 * @brief STL hash map that uses a custom allocator.
 *
 * @tparam TKey The type of the keys in the map.
 * @tparam TVal The type of the values in the map.
 * @tparam AllocType The type of the allocator. HeapAllocDefault by default.
 */
template <typename TKey, typename TVal, typename AllocType>
using HashMap =
    std::unordered_map<TKey, TVal, std::hash<TKey>, std::equal_to<TKey>, STLAlloc<std::pair<const TKey, TVal>, AllocType, false, true, false>>;

/**
 * @brief STL string that uses a custom allocator.
 *
 * @tparam AllocType The type of the allocator. HeapAllocDefault by default.
 */
template <typename AllocType>
using String = std::basic_string<char, std::char_traits<char>, STLAlloc<char, AllocType, false, true, false>>;

/**
 * @brief STL deque that uses a custom allocator.
 *
 * @tparam T The type of the values in the vector.
 * @tparam AllocType The type of the allocator. HeapAllocDefault by default.
 */
template <typename T, typename AllocType>
using Deque = std::deque<T, STLAlloc<T, AllocType, false, true, false>>;

template <typename TKey, typename TVal, typename AllocType>
using Map = boost::container::flat_map<TKey, TVal, std::less<>, STLAlloc<std::pair<TKey, TVal>, AllocType, false, true, false>>;

} // namespace WhiteBirdEngine
// NOLINTEND

#endif
