// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_HEAP_ALLOCATOR_FIXED_SIZE_POOL_HH__
#define __WBE_HEAP_ALLOCATOR_FIXED_SIZE_POOL_HH__

#include "core/allocator/allocator.hh"
#include "core/logging/log.hh"
#include "heap_allocator.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace WhiteBirdEngine {

template <size_t SIZE, template <size_t> class HeapAllocatorFixedSizePool>
struct AllocatorTrait<class HeapAllocatorFixedSizePool<SIZE>> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorFixedSizePool<SIZE>>);
    static constexpr AllocatorType TYPE = AllocatorType::HEAP_ALLOCATOR;
    static constexpr bool IS_POOL = true;
    static constexpr bool IS_GURANTEED_CONTINUOUS = true;
    static constexpr bool IS_ALIGNABLE = false;
    static constexpr bool IS_LIMITED_SIZE = true;
    static constexpr bool IS_ALLOC_FIXED_SIZE = true;
    static constexpr bool IS_ATOMIC = false;
    static constexpr bool WILL_ADDR_MOVE = true;

    WBE_TRAIT_REQURIES(AllocatorTraitConcept);
};

template <size_t SIZE>
class HeapAllocatorFixedSizePool final : public HeapAllocator {
    using DataIndex = uint8_t;
    using InternalID = uint8_t;
public:

    HeapAllocatorFixedSizePool()
        : HeapAllocatorFixedSizePool(64) {}
    virtual ~HeapAllocatorFixedSizePool() override;
    HeapAllocatorFixedSizePool(const HeapAllocatorFixedSizePool&) = delete;
    HeapAllocatorFixedSizePool(HeapAllocatorFixedSizePool&&) = delete;
    HeapAllocatorFixedSizePool& operator=(const HeapAllocatorFixedSizePool&) = delete;
    HeapAllocatorFixedSizePool& operator=(HeapAllocatorFixedSizePool&&) = delete;

    static constexpr uint8_t MAX_OBJ = 127;

    HeapAllocatorFixedSizePool(uint32_t p_max_obj)
        : max_obj(p_max_obj), alloc_obj_count(0) {
        if (p_max_obj > MAX_OBJ) {
            throw std::runtime_error("Failed to create allocator: allocator only allows a maximum of " + std::to_string(MAX_OBJ) + " objects");
        }
        static_assert(MAX_OBJ < std::numeric_limits<InternalID>::max());
        // The memory chunk is separated by the "index space", "reverse index space" and the "address space".
        // index space maps MemID (casted to InternalID) to DataIndex, which represents the index of slot of the data it's referencing
        // reverse index space maps DataIndex to InteralID.
        // data space stores the data.
        // Notice that when DataIndex or InternalID is 0 it maps to MEM_NULL,
        // so for offseting, the true offset for the reverse data is internal id - 1.
        mem_chunk = (char*)malloc(SIZE * max_obj + sizeof(DataIndex) * max_obj + sizeof(InternalID) * max_obj);
        clear_indices();
    }

    virtual MemID allocate(size_t p_size = SIZE) override;

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

    virtual uint32_t obj_count() const override {
        return alloc_obj_count;
    }

    virtual const std::set<MemID>& get_allocated() const override {
        static std::set<MemID> result;
        result.clear();
        for (uint32_t i = 0; i < alloc_obj_count; ++i) {
            result.insert(*(index_chunk_rev_start() + i));
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

    // TODO: Test
    virtual void clear() override {
        clear_indices();
        alloc_obj_count = 0;
    }

private:
    DataIndex max_obj;
    char* mem_chunk;
    DataIndex alloc_obj_count;

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
        uintptr_t offset = (get_data_index(p_id) - 1) * SIZE;
        return data_chunk_start() + offset;
    }

    const void* get_mem_loc_at_id(InternalID p_id) const {
        if (p_id > max_obj) {
            return nullptr;
        }
        uintptr_t offset = (get_data_index(p_id) - 1) * SIZE;
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
        return (DataIndex*)(mem_chunk + SIZE * max_obj);
    }

    InternalID* index_chunk_rev_start() {
        return (InternalID*)(mem_chunk + SIZE * max_obj + max_obj * sizeof(DataIndex));
    }

    char* data_chunk_start() {
        return mem_chunk;
    }

    const DataIndex* index_chunk_start() const {
        return (DataIndex*)(mem_chunk + SIZE * max_obj);
    }

    const InternalID* index_chunk_rev_start() const {
        return (InternalID*)(mem_chunk + SIZE * max_obj + max_obj * sizeof(DataIndex));
    }

    const char* data_chunk_start() const {
        return mem_chunk;
    }

};

template <size_t SIZE>
HeapAllocatorFixedSizePool<SIZE>::~HeapAllocatorFixedSizePool() {
    if (obj_count() != 0) {
        wbe_console_log(WBE_CHANNEL_GLOBAL)->warning("HeapAllocatorFixedSizePool not empty during destruction.");
    }
    free(mem_chunk);
}

template <size_t SIZE>
inline MemID HeapAllocatorFixedSizePool<SIZE>::allocate(size_t p_size) {
    WBE_DEBUG_ASSERT(p_size == SIZE);
    if (alloc_obj_count >= max_obj) {
        throw std::runtime_error("Failed to allocate memory: not enough space for memory pool.");
    }
    InternalID curr_id = retrieve_valid_index();
    write_info(curr_id, ++alloc_obj_count);
    return curr_id;
}

template <size_t SIZE>
inline void HeapAllocatorFixedSizePool<SIZE>::deallocate(MemID p_mem) {
    // TODO: fix bug
    DataIndex data_index = get_data_index(p_mem);
    if (data_index == MEM_NULL || p_mem > max_obj) {
        throw std::runtime_error("Failed to deallocate memory: memory not allocated in this memory pool.");
    }
    if (alloc_obj_count > 1 && data_index != alloc_obj_count) {
        void* data_loc = get_mem_loc_at_id(p_mem);
        InternalID copy_from_id = get_internal_id(alloc_obj_count);
        void* copy_from = get_mem_loc_at_id(copy_from_id);
        memcpy(data_loc, copy_from, SIZE);
        write_info(copy_from_id, data_index);
    }
    write_data_index(alloc_obj_count, MEM_NULL);
    --alloc_obj_count;
}

template <size_t SIZE>
inline HeapAllocatorFixedSizePool<SIZE>::operator std::string() const {
    std::stringstream ss;
    ss << "HeapAllocatorFix: {obj_count: " << obj_count() << ";\n" << "max_obj: " << static_cast<uint32_t>(max_obj) << "\n";
    std::set<MemID> alloc_mems = get_allocated();
    for (auto alloc_mem : alloc_mems) {
        ss << "allocated: " << alloc_mem << ";\n";
    }
    ss << "};";
    return ss.str();
}

}

#endif
