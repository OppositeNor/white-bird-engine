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
#ifndef __WBE_HEAP_ALLOCATOR_FIXED_SIZE_POOL_HH__
#define __WBE_HEAP_ALLOCATOR_FIXED_SIZE_POOL_HH__

#include "core/allocator/allocator.hh"
#include "heap_allocator.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocatorFixedSizePool> final : public AllocatorTrait<HeapAllocator> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorFixedSizePool>);
    static constexpr bool IS_POOL = true;
    static constexpr bool IS_GURANTEED_CONTINUOUS = true;
    static constexpr bool IS_ALIGNABLE = false;
    static constexpr bool IS_LIMITED_SIZE = true;
    static constexpr bool IS_ALLOC_FIXED_SIZE = true;
    static constexpr bool IS_ATOMIC = false;
    static constexpr bool WILL_ADDR_MOVE = true;

    WBE_TRAIT_REQUIRES(AllocatorTraitConcept);
};

class HeapAllocatorFixedSizePool final : public HeapAllocator {
public:
    using DataIndex = uint16_t;
    using InternalID = uint16_t;

    /**
     * @brief The maximum number of objects the pool can hold.
     */
    static constexpr uint32_t MAX_OBJ = std::numeric_limits<InternalID>::max() - 1;

    virtual ~HeapAllocatorFixedSizePool() override;
    HeapAllocatorFixedSizePool(const HeapAllocatorFixedSizePool&) = delete;
    HeapAllocatorFixedSizePool(HeapAllocatorFixedSizePool&&) = delete;
    HeapAllocatorFixedSizePool& operator=(const HeapAllocatorFixedSizePool&) = delete;
    HeapAllocatorFixedSizePool& operator=(HeapAllocatorFixedSizePool&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_max_obj The maximum objects this allocator could hold. Up to MAX_OBJ maximum.
     */
    HeapAllocatorFixedSizePool(size_t p_element_size, uint32_t p_max_obj)
        : max_obj(p_max_obj), alloc_obj_count(0), element_size(p_element_size) {
        if (p_max_obj > MAX_OBJ) {
            throw std::runtime_error("Failed to create allocator: allocator only allows a maximum of " + std::to_string(MAX_OBJ) + " objects");
        }
        static_assert(MAX_OBJ < std::numeric_limits<InternalID>::max());
        static_assert(MAX_OBJ < std::numeric_limits<DataIndex>::max());
        // The memory chunk is separated by the "index space", "reverse index space" and the "address space".
        // index space maps MemID (casted to InternalID) to DataIndex, which represents the index of slot of the data it's referencing
        // reverse index space maps DataIndex to InteralID.
        // data space stores the data.
        // Notice that when DataIndex or InternalID is 0 it maps to MEM_NULL,
        // so for offseting, the true offset for the reverse data is internal id - 1.
        mem_chunk = (char*)malloc(element_size * max_obj + sizeof(DataIndex) * max_obj + sizeof(InternalID) * max_obj);
        clear_indices();
    }

    virtual MemID allocate(size_t p_size = 0) override;

    virtual void deallocate(MemID p_mem) override;

    virtual const void* get(MemID p_id) const override {
        if (p_id == 0) {
            return nullptr;
        }
        return get_mem_loc_at_id(p_id);
    }

    virtual void* get(MemID p_id) override {
        if (p_id == 0) {
            return nullptr;
        }
        return get_mem_loc_at_id(p_id);
    }

    /**
     * @brief Get memory ID by its index in the pool.
     *
     * @param p_index The index of the element.
     */
    MemID get_id_by_index(DataIndex p_index) {
        return static_cast<MemID>(get_internal_id(p_index));
    }

    /**
     * @brief Get number of allocated objects.
     *
     * @return The number of allocated objects.
     */
    uint32_t obj_count() const {
        return alloc_obj_count;
    }

    std::vector<MemID> get_allocated() const {
        std::vector<MemID> result;
        result.reserve(alloc_obj_count);
        for (uint32_t i = 0; i < alloc_obj_count; ++i) {
            result.push_back(*(index_chunk_rev_start() + i));
        }
        return result;
    }

    virtual operator std::string() const override;

    const void* get_mem_start() const {
        return data_chunk_start();
    }

    virtual bool is_empty() const override {
        return alloc_obj_count == 0;
    }

    size_t get_allocated_data_size(MemID p_mem_id) const {
        return element_size;
    }

    // TODO: Test
    virtual void clear() override {
        clear_indices();
        alloc_obj_count = 0;
    }

    size_t get_element_size() const {
        return element_size;
    }

private:
    DataIndex max_obj;
    char* mem_chunk;
    DataIndex alloc_obj_count;
    const size_t element_size;

    DataIndex get_data_index(InternalID p_id) const {
        if (p_id > max_obj) {
            return MEM_NULL;
        }
        return *((DataIndex*)index_chunk_start() + p_id - 1);
    }

    InternalID get_internal_id(DataIndex p_data_index) const {
        if (p_data_index > max_obj) {
            return MEM_NULL;
        }
        return *((InternalID*)index_chunk_rev_start() + p_data_index - 1);
    }

    void* get_mem_loc_at_id(InternalID p_id) {
        if (p_id > max_obj) {
            return nullptr;
        }
        uintptr_t offset = (get_data_index(p_id) - 1) * element_size;
        return data_chunk_start() + offset;
    }

    const void* get_mem_loc_at_id(InternalID p_id) const {
        if (p_id > max_obj) {
            return nullptr;
        }
        uintptr_t offset = (get_data_index(p_id) - 1) * element_size;
        return data_chunk_start() + offset;
    }

    void write_id(InternalID p_id, DataIndex p_data_index) {
        *(index_chunk_start() + p_id - 1) = p_data_index;
    }

    void write_data_index(DataIndex p_data_index, InternalID p_id) {
        *(index_chunk_rev_start() + p_data_index - 1) = p_id;
    }

    void write_info(InternalID p_id, DataIndex p_data_index) {
        write_id(p_id, p_data_index);
        write_data_index(p_data_index, p_id);
    }

    void clear_indices() {
        memset(index_chunk_start(), MEM_NULL, max_obj * sizeof(DataIndex));
        memset(index_chunk_rev_start(), MEM_NULL, max_obj * sizeof(InternalID));
    }

    InternalID retrieve_valid_index() {
        for (InternalID id = 1; id <= max_obj; ++id) {
            if (get_data_index(id) == MEM_NULL) {
                return id;
            }
        }
        throw std::runtime_error("Failed to retrieve valid index: memory chunk is full.");
    }

    DataIndex* index_chunk_start() {
        return (DataIndex*)(mem_chunk + element_size * max_obj);
    }

    InternalID* index_chunk_rev_start() {
        return (InternalID*)(mem_chunk + element_size * max_obj + max_obj * sizeof(DataIndex));
    }

    char* data_chunk_start() {
        return mem_chunk;
    }

    const DataIndex* index_chunk_start() const {
        return (DataIndex*)(mem_chunk + element_size * max_obj);
    }

    const InternalID* index_chunk_rev_start() const {
        return (InternalID*)(mem_chunk + element_size * max_obj + max_obj * sizeof(DataIndex));
    }

    const char* data_chunk_start() const {
        return mem_chunk;
    }

};


}

#endif
