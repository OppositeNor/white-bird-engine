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
#ifndef WBE_FILE_HEAP_ALLOCATOR_FIXED_SIZE_POOL_HH
#define WBE_FILE_HEAP_ALLOCATOR_FIXED_SIZE_POOL_HH

#include "core/allocator/i_allocator.hh"
#include "core/logging/log.hh"
#include "heap_allocator.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#define WBE_HAFSP_GET_DATA_INDEX(id) (*(index_chunk_start + (id) - 1))
#define WBE_HAFSP_INDEX_CHUNK_REV_START
#define WBE_HAFSP_DATA_CHUNK_START (mem_chunk)
#define WBE_HAFSP_WRITE_ID(id, data_index) do { *(index_chunk_start + (id) - 1) = (data_index); } while (false)
#define WBE_HAFSP_WRITE_DATA_INDEX(data_index, id) do { *(index_chunk_rev_start + (data_index) - 1) = (id); } while (false)

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

/**
 * @class HeapAllocatorFixedSizePool
 * @brief Pooled heap allocator with fixed object size.
 *
 */
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
        : max_obj(p_max_obj), element_size(p_element_size) {
        if (get_align_size(p_element_size, WBE_DEFAULT_ALIGNMENT) != p_element_size) {
            stdout_log(WBE_CHANNEL_GLOBAL)->warning(std::format(
                "Creating HeapAllocatorFixedSizePool with element size {} which is not aligned to default alignment {}."
                " Consider using aligned size for better performance.", p_element_size, WBE_DEFAULT_ALIGNMENT));
        }
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
        mem_chunk = static_cast<char*>(malloc(element_size * max_obj + sizeof(DataIndex) * max_obj + sizeof(InternalID) * max_obj)); // NOLINT
        index_chunk_start = reinterpret_cast<DataIndex*>(mem_chunk + element_size * max_obj);
        index_chunk_rev_start = reinterpret_cast<InternalID*>(mem_chunk + element_size * max_obj + max_obj * sizeof(DataIndex));
        clear_indices();
    }

    virtual MemID allocate(size_t p_size = 0) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) const override {
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
            result.push_back(*(index_chunk_rev_start + i));
        }
        return result;
    }

    virtual operator std::string() const override;

    const void* get_mem_start() const {
        return WBE_HAFSP_DATA_CHUNK_START;
    }

    virtual bool is_empty() const override {
        return alloc_obj_count == 0;
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
    DataIndex* index_chunk_start = nullptr;
    InternalID* index_chunk_rev_start = nullptr;
    DataIndex alloc_obj_count = 0;
    const size_t element_size;

    DataIndex get_data_index(InternalID p_id) const {
        if (p_id > max_obj) {
            return MEM_NULL;
        }
        return *(index_chunk_start + p_id - 1); // NOLINT
    }

    InternalID get_internal_id(DataIndex p_data_index) const {
        if (p_data_index > max_obj) {
            return MEM_NULL;
        }
        return *(index_chunk_rev_start + p_data_index - 1);
    }

    void* get_mem_loc_at_id(InternalID p_id) const {
        if (p_id > max_obj) {
            return nullptr;
        }
        uintptr_t offset = (WBE_HAFSP_GET_DATA_INDEX(p_id) - 1) * element_size;
        return WBE_HAFSP_DATA_CHUNK_START + offset;
    }

    void write_info(InternalID p_id, DataIndex p_data_index) {
        WBE_HAFSP_WRITE_ID(p_id, p_data_index);
        WBE_HAFSP_WRITE_DATA_INDEX(p_data_index, p_id);
    }

    void clear_indices() {
        memset(index_chunk_start, MEM_NULL, max_obj * sizeof(DataIndex));
        memset(index_chunk_rev_start, MEM_NULL, max_obj * sizeof(InternalID));
    }

    InternalID retrieve_valid_index() {
        for (InternalID id = 1; id <= max_obj; ++id) {
            if (WBE_HAFSP_GET_DATA_INDEX(id) == MEM_NULL) {
                return id;
            }
        }
        throw std::runtime_error("Failed to retrieve valid index: memory chunk is full.");
    }
};


} // namespace WhiteBirdEngine
#undef WBE_HAFSP_GET_DATA_INDEX
#undef WBE_HAFSP_DATA_CHUNK_START
#undef WBE_HAFSP_WRITE_ID
#undef WBE_HAFSP_WRITE_DATA_INDEX

#endif
