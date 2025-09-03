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
#ifndef __WBE_HEAP_ALLOCATOR_ALIGNED_POOL_HH__
#define __WBE_HEAP_ALLOCATOR_ALIGNED_POOL_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocatorAlignedPool> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorAlignedPool>);
    static constexpr AllocatorType TYPE = AllocatorType::HEAP_ALLOCATOR;
    static constexpr bool IS_POOL = true;
    static constexpr bool IS_GURANTEED_CONTINUOUS = false;
    static constexpr bool IS_ALIGNABLE = true;
    static constexpr bool IS_LIMITED_SIZE = true;
    static constexpr bool IS_ALLOC_FIXED_SIZE = false;
    static constexpr bool IS_ATOMIC = false;
    static constexpr bool WILL_ADDR_MOVE = false;

    WBE_TRAIT_REQURIES(AllocatorTraitConcept);
};

/**
 * @class HeapAllocatorAlignedPool
 * @brief Heap allocator pool with memory alignment support.
 *
 * @todo Test
 */
class HeapAllocatorAlignedPool : public HeapAllocatorAligned {
public:
    HeapAllocatorAlignedPool()
        : HeapAllocatorAlignedPool(WBE_KiB(64)) {}
    virtual ~HeapAllocatorAlignedPool() override;
    HeapAllocatorAlignedPool(const HeapAllocatorAlignedPool&) = delete;
    HeapAllocatorAlignedPool(HeapAllocatorAlignedPool&&) = delete;
    HeapAllocatorAlignedPool& operator=(const HeapAllocatorAlignedPool&) = delete;
    HeapAllocatorAlignedPool& operator=(HeapAllocatorAlignedPool&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_size The total size of the pool.
     */
    HeapAllocatorAlignedPool(size_t p_size);

    virtual MemID allocate(size_t p_size, size_t p_alignment) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) const override;

    virtual uint32_t obj_count() const override {
        return allocated.size();
    }

    virtual std::set<MemID> get_allocated() const override {
        return allocated;
    }

    virtual bool is_empty() const override {
        return allocated.empty();
    }

    virtual void clear() override {
        size_map.clear();
        allocated.clear();
        idle_list_head->size = size;
        idle_list_head->mem_start = mem_chunk;
        idle_list_head->next = nullptr;
    }

    virtual size_t get_allocated_data_size(MemID p_mem_id) const override {
        return size_map.at(get(p_mem_id));
    }

    size_t get_total_size() const {
        return size;
    }

    size_t get_remain_size() const;

    virtual operator std::string() const override {
        std::stringstream ss;
        ss << "HeapAllocatorAlignedPool: {obj_count: " << obj_count() << ";\n";
        ss << "free_chunk_layout: {\n";
        IdleListNode* node = idle_list_head.get();
        while (node != nullptr) {
            ss << "chunk: {\nbegin: " << node->mem_start - mem_chunk << ";\nsize: " << node->size << ";\n};\n";
            node = node->next.get();
        }
        ss << "};\n};";
        return ss.str();
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

    void* acquire_memory(std::unique_ptr<IdleListNode>& p_node, char* p_mem_start, size_t p_mem_size);
    void insert_free_memory(IdleListNode* p_node_before_insert, char* p_insert_start, size_t p_insert_size);
    bool combine_idle_with_next(IdleListNode* p_node);
    std::unique_ptr<IdleListNode>& get_idle_node_before(void* p_loc);

    size_t size;
    std::unordered_map<void*, size_t> size_map;
    char* mem_chunk;
    uint32_t idle_chunks_count;
    std::unique_ptr<IdleListNode> idle_list_head;
    std::set<MemID> allocated;

    size_t max_data_loc_tracker = 0;

    bool is_in_pool(MemID p_id) const {
        return allocated.contains(p_id);
    }
};

}

#endif
