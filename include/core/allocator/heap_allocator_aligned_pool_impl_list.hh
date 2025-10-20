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
#ifndef __WBE_HEAP_ALLOCATOR_ALIGNED_POOL_IMPL_LIST_HH__
#define __WBE_HEAP_ALLOCATOR_ALIGNED_POOL_IMPL_LIST_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <limits>

#define WBE_HAAPIL_GET_HEADER_SIZE(p_header) p_header & TOTAL_SIZE_MASK
#define WBE_HAAPIL_GET_CHUNK_SIZE(p_chunk) WBE_HAAPIL_GET_HEADER_SIZE(*reinterpret_cast<Header*>(p_mem_chunk))
#define WBE_HAAPIL_SET_HEADER(p_header, p_head_type, p_size) (*p_header = (((Header)(p_head_type) << 60) | (p_size)))
#define WBE_HAAPIL_SET_CHUNK_HEADER(p_chunk, p_type, p_size) WBE_HAAPIL_SET_HEADER(reinterpret_cast<Header*>(p_chunk), (p_type), (p_size))


namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocatorAlignedPoolImplicitList> final : public AllocatorTrait<HeapAllocatorAligned> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorAlignedPoolImplicitList>);
    static constexpr bool IS_POOL = true;
    static constexpr bool IS_GURANTEED_CONTINUOUS = false;
    static constexpr bool IS_LIMITED_SIZE = true;
    static constexpr bool IS_ALLOC_FIXED_SIZE = false;
    static constexpr bool IS_ATOMIC = false;
    static constexpr bool WILL_ADDR_MOVE = false;

    WBE_TRAIT_REQUIRES(AllocatorTraitConcept);
};

/**
 * @class HeapAllocatorAlignedPoolImplicitList
 * @brief Heap allocator pool with memory alignment support, with an implicit list.
 */
class HeapAllocatorAlignedPoolImplicitList final : public HeapAllocatorAligned {
public:
    HeapAllocatorAlignedPoolImplicitList()
        : HeapAllocatorAlignedPoolImplicitList(WBE_KiB(64)) {}
    virtual ~HeapAllocatorAlignedPoolImplicitList() override;
    HeapAllocatorAlignedPoolImplicitList(const HeapAllocatorAlignedPoolImplicitList&) = delete;
    HeapAllocatorAlignedPoolImplicitList(HeapAllocatorAlignedPoolImplicitList&&) = delete;
    HeapAllocatorAlignedPoolImplicitList& operator=(const HeapAllocatorAlignedPoolImplicitList&) = delete;
    HeapAllocatorAlignedPoolImplicitList& operator=(HeapAllocatorAlignedPoolImplicitList&&) = delete;

    using Header = uint64_t;

    /**
     * @brief The size of the allocated memory header.
     */
    static constexpr size_t WORD_SIZE = sizeof(Header);

    /**
     * @brief The maximum total size that the allocator can contain.
     */
    static constexpr size_t TOTAL_SIZE_MASK = std::numeric_limits<Header>::max() >> 4;

    /**
     * @brief Constructor.
     *
     * @param p_size The total size of the pool.
     */
    HeapAllocatorAlignedPoolImplicitList(size_t p_size);

    virtual MemID allocate(size_t p_size, size_t p_alignment = WORD_SIZE) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) const override {
        if (p_id == MEM_NULL) {
            return nullptr;
        }
        return reinterpret_cast<void*>(p_id);
    }

    virtual bool is_empty() const override {
        return get_remain_size() == size;
    }

    virtual void clear() override {
        WBE_HAAPIL_SET_CHUNK_HEADER(mem_chunk, HeaderType::IDLE, size);
    }

    virtual size_t get_allocated_data_size(MemID p_mem_id) const override {
        return WBE_HAAPIL_GET_HEADER_SIZE(*reinterpret_cast<Header*>((p_mem_id - WORD_SIZE)));
    }

    /**
     * @brief Get the total size of the allocator.
     *
     * @return The total size of the allocator.
     */
    size_t get_total_size() const {
        return size;
    }

    /**
     * @brief Get the remaining size of the allocator.
     *
     * @return The remaining size of the allocator.
     */
    size_t get_remain_size() const;

    virtual operator std::string() const override;

    /**
     * @brief Get the internal fragmentation tracker.
     *
     * @return The internal fragmentation tracker.
     */
    size_t get_internal_fragmentation_tracker() const {
        return internal_fragmentation_tracker;
    }

    /**
     * @brief Check if a memory id belongs in this pool.
     *
     * @param p_mem_id The memory ID to check.
     * @return True if it belongs to this pool, false otherwise.
     */
    bool is_in_pool(MemID p_mem_id) const;

private:

    size_t size;
    char* mem_chunk;
    uint32_t idle_chunks_count;

    size_t internal_fragmentation_tracker = 0;

    static constexpr Header HEADER_TYPE_MASK = (0b1ull << 60);
    enum class HeaderType {
        // Occupied head
        OCCUPIED = 0,
        // Idle memory head
        IDLE = 1,
    };

    template <bool COALESCE_ENABLED>
    MemID find_valid_chunk(size_t p_aligned_size, size_t p_alignment);
    template <bool CHECK_FIRST, bool COALESCE_ENABLED>
    char* get_next_free_memory(char* p_from);
    void* acquire_memory(char* p_idle_chunk, char* p_mem_start, size_t p_mem_size);
    void insert_free_memory(char* p_insert_start, size_t p_insert_size);
    char* get_chunk_before(void* p_loc);

    void coalesce_all() const;
    void coalesce_chunk(char* p_chunk) const;
};

}

#undef WBE_HAAPIL_GET_HEADER_SIZE
#undef WBE_HAAPIL_GET_CHUNK_SIZE
#undef WBE_HAAPIL_SET_HEADER
#undef WBE_HAAPIL_SET_CHUNK_HEADER


#endif
