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
#ifndef __WBE_HEAP_ALLOCATOR_POOL_ALIGNED_HH__
#define __WBE_HEAP_ALLOCATOR_POOL_ALIGNED_HH__
#include "core/allocator/allocator.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <sstream>
#include <string>

namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocatorAligned> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorAligned>);
    static constexpr AllocatorType TYPE = AllocatorType::HEAP_ALLOCATOR;
    static constexpr bool IS_ALIGNABLE = true;
};

/**
 * @class HeapAllocatorAligned
 * @brief Heap allocator that has alignment allocation support.
 *
 */
class HeapAllocatorAligned {
public:
    HeapAllocatorAligned() = default;
    virtual ~HeapAllocatorAligned() {}

    /**
     * @brief Allocate memory of a specific size.
     * @note If IS_ALLOC_FIXED_SIZE is true for this allocator, allocating a size
     * that is not the same as the specified size for the allocator will cause an
     * error.
     *
     * @param p_size The size to allocate.
     * @param p_alignment Alignment.
     * @return The memory ID of the allocated resource.
     */
    virtual MemID allocate(size_t p_size, size_t p_alignment = WBE_DEFAULT_ALIGNMENT) = 0;

    /**
     * @brief Deallocate memroy.
     *
     * @param p_mem The memory id to be deallocated.
     */
    virtual void deallocate(MemID p_mem) = 0;

    /**
     * @brief Get the pointer pointing to the resource.
     *
     * @param p_id The resource id to get the pointer from.
     * @return The pointer of the resource.
     */
    virtual void* get(MemID p_id) const = 0;

    /**
     * @brief Is the allocator empty.
     *
     * @return true if allocator is empty, false otherwise.
     */
    virtual bool is_empty() const = 0;

    /**
     * @brief Clear the allocator.
     */
    virtual void clear() = 0;

    /**
     * @brief Get the size of an allocated memory.
     *
     * @param p_mem_id The memory ID of the allocated memory.
     * @return The size of the allocated memory.
     */
    virtual size_t get_allocated_data_size(MemID p_mem_id) const = 0;

    /**
     * @brief Get the pointer pointing to the resource.
     *
     * @tparam T The type of the resource.
     * @param p_id The resource id to get the pointer from.
     * @return The pointer of the resource.
     */
    template <typename T>
    T* get_obj(MemID p_id) const {
        return static_cast<T*>(get(p_id));
    }

    virtual operator std::string() const {
        std::stringstream ss;
        ss << "{\"type\":\"HeapAllocatorAligned\"}";
        return ss.str();
    }
};

/**
 * @brief Create an object.
 *
 * @tparam T The type of the object.
 * @tparam Args The types of the constructor arguments to construct the object.
 * @param p_allocator The allocator to allocate memory from.
 * @param p_args The arguments of the constructor.
 * @return The memory ID of the created object.
 */
template <typename T, typename... Args>
inline MemID create_obj_align(HeapAllocatorAligned& p_allocator, Args&&... p_args) {
    MemID id = p_allocator.allocate(sizeof(T));
    new(p_allocator.get(id)) T(std::forward<Args>(p_args)...);
    return id;
}

/**
 * @brief Create an array of objects.
 *
 * @tparam T The type of the objects.
 * @tparam Args The types of the constructor arguments to construct the object.
 * @param p_allocator The allocator to allocate memory from.
 * @param p_num The number of instance in the array.
 * @param p_args The arguments of the constructor.
 * @return The memory ID of the created object.
 */
template <typename T, typename... Args>
inline MemID create_obj_array_align(HeapAllocatorAligned& p_allocator, size_t p_num, Args&&... p_args) {
    MemID id = p_allocator.allocate(sizeof(T) * p_num);
    T* begin = static_cast<T*>(p_allocator.get(id));
    for (; p_num > 0; --p_num) {
        new(begin + p_num - 1) T(std::forward<Args>(p_args)...);
    }
    return id;
}

/**
 * @brief Destroy an object.
 *
 * @tparam T The type of the object.
 * @param p_allocator The allocator that this object's memory is allocated from.
 * @param p_id The memory ID of the object.
 */
template <typename T>
inline void destroy_obj(HeapAllocatorAligned& p_allocator, MemID p_id) {
    if (p_id == MEM_NULL) {
        return;
    }
    static_cast<T*>(p_allocator.get(p_id))->~T();
    p_allocator.deallocate(p_id);
}

/**
 * @brief Destroy an array of objects.
 *
 * @tparam T The type of the objects.
 * @param p_allocator The allocator that this object's memory is allocated from.
 * @param p_id The memory ID of the object.
 * @param p_num The number of instances in the array.
 */
template<typename T, typename AllocatorType>
void destroy_obj_array_align(AllocatorType& p_allocator, MemID p_id, size_t p_num) {
    T* array_ptr = static_cast<T*>(p_allocator.get(p_id));
    
    for (; p_num > 0; --p_num) {
        array_ptr[p_num - 1].~T();
    }
    
    p_allocator.deallocate(p_id);
}

/**
 * @brief Destroy an object.
 *
 * @tparam T The type of the object.
 * @param p_allocator The allocator that this object's memory is allocated from.
 * @param p_id The memory ID of the object.
 */
template <typename T>
inline void destroy_array(HeapAllocatorAligned& p_allocator, MemID p_id, size_t p_num) {
    if (p_id == MEM_NULL) {
        return;
    }
    T* begin = static_cast<T*>(p_allocator.get(p_id));
    for (size_t i = 0; i < p_num; ++i) {
        (begin + i)->~T();
    }
    p_allocator.deallocate(p_id);
}

}

#endif
