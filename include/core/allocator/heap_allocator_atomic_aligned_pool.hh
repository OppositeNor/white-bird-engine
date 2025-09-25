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
#ifndef __WBE_HEAP_ALLOCATOR_ATOMIC_ALIGNED_POOL_HH__
#define __WBE_HEAP_ALLOCATOR_ATOMIC_ALIGNED_POOL_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <limits>
#include <memory>

namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocatorAtomicAlignedPool> final : public AllocatorTrait<HeapAllocatorAligned> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorAtomicAlignedPool>);
    static constexpr bool IS_POOL = true;
    static constexpr bool IS_GURANTEED_CONTINUOUS = false;
    static constexpr bool IS_LIMITED_SIZE = true;
    static constexpr bool IS_ALLOC_FIXED_SIZE = false;
    static constexpr bool IS_ATOMIC = true;
    static constexpr bool WILL_ADDR_MOVE = false;

    WBE_TRAIT_REQUIRES(AllocatorTraitConcept);
};


/**
 * @class HeapAllocatorAtomicAlignedPool
 * @brief Heap allocator aligned pool atomic version.
 *
 */
class HeapAllocatorAtomicAlignedPool : public HeapAllocatorAligned {
public:
public:
    HeapAllocatorAtomicAlignedPool()
        : HeapAllocatorAtomicAlignedPool(WBE_KiB(64)) {}
    virtual ~HeapAllocatorAtomicAlignedPool() override;
    HeapAllocatorAtomicAlignedPool(const HeapAllocatorAtomicAlignedPool&) = delete;
    HeapAllocatorAtomicAlignedPool(HeapAllocatorAtomicAlignedPool&&) = delete;
    HeapAllocatorAtomicAlignedPool& operator=(const HeapAllocatorAtomicAlignedPool&) = delete;
    HeapAllocatorAtomicAlignedPool& operator=(HeapAllocatorAtomicAlignedPool&&) = delete;

    using Header = uint64_t;

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
     * @param p_size The total size of the pool.
     */
    HeapAllocatorAtomicAlignedPool(size_t p_size);

    virtual MemID allocate(size_t p_size, size_t p_alignment = WBE_DEFAULT_ALIGNMENT) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) const override;

    virtual bool is_empty() const override {
        boost::shared_lock lock(mutex);
        return idle_list_head != nullptr && idle_list_head->size == size;
    }

    virtual void clear() override {
        boost::unique_lock lock(mutex);
        idle_list_head = std::make_unique<IdleListNode>();
        idle_list_head->size = size;
        idle_list_head->mem_start = mem_chunk;
        idle_list_head->next = nullptr;
    }

    virtual size_t get_allocated_data_size(MemID p_mem_id) const override {
        boost::shared_lock lock(mutex);
        return *reinterpret_cast<Header*>((p_mem_id - HEADER_SIZE)) & MAX_TOTAL_SIZE;
    }

    size_t get_total_size() const {
        boost::shared_lock lock(mutex);
        return size;
    }

    size_t get_remain_size() const;


    /**
     * @brief Is the memory ID in this pool
     *
     * @param p_mem_id The memory ID to check.
     * @return True if the memory ID belongs to this pool, false otherwise.
     */
    bool is_in_pool(MemID p_mem_id) const {
        boost::shared_lock lock(mutex);
        bool result = unguard_is_in_pool(p_mem_id);
        return result;
    }

    virtual operator std::string() const override;

    size_t get_internal_fragmentation_tracker() const {
        boost::shared_lock lock(mutex);
        return internal_fragmentation_tracker;
    }

private:

    struct IdleListNode {
        size_t size;
        char* mem_start;
        std::unique_ptr<IdleListNode> next = nullptr;
    };

    bool unguard_is_in_pool(MemID p_mem_id) const;

    void* acquire_memory(std::unique_ptr<IdleListNode>& p_node, char* p_mem_start, size_t p_mem_size);
    void insert_free_memory(IdleListNode* p_node_before_insert, char* p_insert_start, size_t p_insert_size);
    bool combine_idle_with_next(IdleListNode* p_node);
    std::unique_ptr<IdleListNode>& get_idle_node_before(void* p_loc);

    size_t size;
    char* mem_chunk;
    uint32_t idle_chunks_count;
    std::unique_ptr<IdleListNode> idle_list_head;

    size_t internal_fragmentation_tracker = 0;
    mutable boost::shared_mutex mutex;
};

}

#endif
