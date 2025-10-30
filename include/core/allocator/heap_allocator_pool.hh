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
#ifndef __WBE_HEAP_ALLOCATOR_POOL_HH__
#define __WBE_HEAP_ALLOCATOR_POOL_HH__

#include "core/allocator/allocator.hh"
#include "heap_allocator.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocatorPool> final : public AllocatorTrait<HeapAllocator> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorPool>);
    static constexpr bool IS_POOL = true;
    static constexpr bool IS_GURANTEED_CONTINUOUS = false;
    static constexpr bool IS_LIMITED_SIZE = true;
    static constexpr bool IS_ALLOC_FIXED_SIZE = false;
    static constexpr bool IS_ATOMIC = false;
    static constexpr bool WILL_ADDR_MOVE = false;
    static constexpr bool IS_ALIGNABLE = false;

    WBE_TRAIT_REQUIRES(AllocatorTraitConcept);
};

/**
 * @class HeapAllocatorPool
 * @brief Pool allocator. Allocate from a continuous memory pool, to prevent memory fragmentation.
 *
 */
class HeapAllocatorPool final : public HeapAllocator {
public:
    HeapAllocatorPool() : HeapAllocatorPool(WBE_KiB(1)) {}
    virtual ~HeapAllocatorPool() override;
    HeapAllocatorPool(const HeapAllocatorPool&) = delete;
    HeapAllocatorPool(HeapAllocatorPool&&) = delete;
    HeapAllocatorPool& operator=(const HeapAllocatorPool&) = delete;
    HeapAllocatorPool& operator=(HeapAllocatorPool&&) = delete;

    using Header = uint32_t;

    /**
     * @brief The size of the allocated memory header.
     */
    static constexpr size_t HEADER_SIZE = sizeof(Header);

    /**
     * @brief The maximum total size that the allocator can contain.
     */
    static constexpr size_t MAX_TOTAL_SIZE = std::numeric_limits<Header>::max() >> 4;

    /**
     * @brief Constructor.
     *
     * @param p_size The size of the pool in bytes.
     */
    HeapAllocatorPool(size_t p_size);

    virtual MemID allocate(size_t p_size) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) const override {
        if (p_id == MEM_NULL) {
            return nullptr;
        }
        return reinterpret_cast<void*>(p_id);
    }

    virtual operator std::string() const override;

    size_t get_allocated_data_size(MemID p_mem_id) const {
        return *reinterpret_cast<size_t*>(p_mem_id - HEADER_SIZE) & MAX_TOTAL_SIZE;
    }

    size_t get_total_size() const {
        return size;
    }

    size_t get_remain_size() const;

    virtual bool is_empty() const override {
        return idle_list_head != nullptr && idle_list_head->size == size;
    }

    // TODO: Test
    virtual void clear() override {
        idle_list_head->size = size;
        idle_list_head->mem_start = mem_chunk;
        idle_list_head->next = nullptr;
    }

    size_t get_max_data_size() const {
        return max_data_loc_tracker;
    }

private:
    
    struct IdleListNode {
        size_t size;
        char* mem_start;
        std::unique_ptr<IdleListNode> next = nullptr;
    };

    void* acquire_memory(std::unique_ptr<IdleListNode>& p_node, size_t p_mem_size);
    void insert_free_memory(IdleListNode* p_node_before_insert, char* p_insert_start, size_t p_insert_size);
    bool combine_idle_with_next(IdleListNode* p_node);
    std::unique_ptr<IdleListNode>& get_idle_node_before(void* p_loc);

    size_t size;
    char* mem_chunk;
    uint32_t idle_chunks_count;
    std::unique_ptr<IdleListNode> idle_list_head;

    size_t max_data_loc_tracker = 0;
};

}

#endif
