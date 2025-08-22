// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_HEAP_ALLOCATOR_POOL_ALIGNED_HH__
#define __WBE_HEAP_ALLOCATOR_POOL_ALIGNED_HH__
#include "core/allocator/allocator.hh"
#include <cstddef>
#include <set>
#include <sstream>
#include <string>

namespace WhiteBirdEngine {
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
    virtual MemID allocate(size_t p_size, size_t p_alignment) = 0;

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
     * @brief Get the number of allocated resources.
     *
     * @return The number of allocated resources.
     */
    virtual uint32_t obj_count() const = 0;

    /**
     * @brief Get the allocated memories' ids
     */
    virtual const std::set<MemID>& get_allocated() const = 0;

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
        ss << "HeapAllocatorAligned{obj_count: " << obj_count() << "}";
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
    MemID id = p_allocator.allocate(sizeof(T), alignof(T));
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
template <typename T>
inline void destroy_obj(HeapAllocatorAligned& p_allocator, MemID p_id) {
    if (p_id == MEM_NULL) {
        return;
    }
    static_cast<T*>(p_allocator.get(p_id))->~T();
    p_allocator.deallocate(p_id);
}

}

#endif
