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
#include "core/allocator/heap_allocator.hh"
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
class HeapAllocatorAligned : public HeapAllocator {
public:
    HeapAllocatorAligned() = default;
    virtual ~HeapAllocatorAligned() override {}

    /**
     * @brief Allocate memory of a specific size.
     * @note If IS_ALLOC_FIXED_SIZE is true for this allocator, allocating a size
     * that is not the same as the specified size for the allocator will cause an
     * error.
     *
     * @param p_size The size to allocate.
     * @return The memory ID of the allocated resource.
     */
    virtual MemID allocate(size_t p_size) override {
        return allocate(p_size, WBE_DEFAULT_ALIGNMENT);
    }

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

    virtual operator std::string() const override {
        std::stringstream ss;
        ss << "{\"type\":\"HeapAllocatorAligned\"}";
        return ss.str();
    }
};

}

#endif
