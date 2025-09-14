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
#ifndef __WBE_HEAP_ALLOCATOR_HH__
#define __WBE_HEAP_ALLOCATOR_HH__

#include "allocator.hh"
#include <sstream>

namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocator> {
    static constexpr AllocatorType TYPE = AllocatorType::HEAP_ALLOCATOR;
};

/**
 * @class HeapAllocator
 * @brief Heap allocator.
 *
 */
class HeapAllocator {
public:
    HeapAllocator() = default;
    virtual ~HeapAllocator() {}

    /**
     * @brief Allocate memory of a specific size.
     * @note If IS_ALLOC_FIXED_SIZE is true for this allocator, allocating a size
     * that is not the same as the specified size for the allocator will cause an
     * error.
     *
     * @param p_size The size to allocate.
     * @return The memory ID of the allocated resource.
     */
    virtual MemID allocate(size_t p_size) = 0;

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
    virtual void* get(MemID p_id) = 0;

    /**
     * @brief Get the pointer pointing to the resource.
     *
     * @param p_id The resource id to get the pointer from.
     * @return The pointer of the resource.
     */
    virtual const void* get(MemID p_id) const = 0;

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
     * @brief Get the pointer pointing to the resource.
     *
     * @tparam T The type of the resource.
     * @param p_id The resource id to get the pointer from.
     * @return The pointer of the resource.
     */
    template <typename T>
    T* get_obj(MemID p_id) {
        return static_cast<T*>(get(p_id));
    }

    /**
     * @brief Get the pointer pointing to the resource.
     *
     * @tparam T The type of the resource.
     * @param p_id The resource id to get the pointer from.
     * @return The pointer of the resource.
     */
    template <typename T>
    const T* get_obj(MemID p_id) const {
        return static_cast<const T*>(get(p_id));
    }

    virtual operator std::string() const {
        std::stringstream ss;
        ss << "{\"type\":\"HeapAllocator\"}";
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
inline MemID create_obj(HeapAllocator& p_allocator, Args&&... p_args) {
    MemID id = p_allocator.allocate(sizeof(T));
    new(p_allocator.get(id)) T(std::forward<Args>(p_args)...);
    return id;
}

/**
 * @brief Destroy an object.
 *
 * @tparam T The type of the object.
 * @param p_allocator The allocator that this object's memory is allocated from.
 * @param p_id The memory ID of the object.
 */
template <typename T, typename AllocType>
inline void destroy_obj(AllocType& p_allocator, MemID p_id) {
    if (p_id == MEM_NULL) {
        return;
    }
    static_cast<T*>(p_allocator.get(p_id))->~T();
    p_allocator.deallocate(p_id);
}

}

#endif
