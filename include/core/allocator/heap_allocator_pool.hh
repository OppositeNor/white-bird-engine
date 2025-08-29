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

#include "heap_allocator.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocatorPool> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorPool>);
    static constexpr AllocatorType TYPE = AllocatorType::HEAP_ALLOCATOR;
    static constexpr bool IS_POOL = true;
    static constexpr bool IS_GURANTEED_CONTINUOUS = false;
    static constexpr bool IS_LIMITED_SIZE = true;
    static constexpr bool IS_ALLOC_FIXED_SIZE = false;
    static constexpr bool IS_ATOMIC = false;
    static constexpr bool WILL_ADDR_MOVE = false;
    static constexpr bool IS_ALIGNABLE = false;

    WBE_TRAIT_REQURIES(AllocatorTraitConcept);
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

    /**
     * @brief Constructor.
     *
     * @param p_size The size of the pool in bytes.
     */
    HeapAllocatorPool(size_t p_size);

    virtual MemID allocate(size_t p_size) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) override {
        if (p_id == 0) {
            return nullptr;
        }
        if (!is_in_pool(p_id)) {
            throw std::runtime_error("Failed to get memory from ID: " + std::to_string(p_id) + ". ID not belongs to this allocator.");
        }
        return reinterpret_cast<void*>(p_id);
    }

    virtual const void* get(MemID p_id) const override {
        if (p_id == 0) {
            return nullptr;
        }
        if (!is_in_pool(p_id)) {
            throw std::runtime_error("Failed to get memory from ID: " + std::to_string(p_id) + ". ID not belongs to this allocator.");
        }
        return reinterpret_cast<void*>(p_id);
    }

    virtual uint32_t obj_count() const override {
        return allocated.size();
    }

    virtual std::set<MemID> get_allocated() const override {
        return allocated;
    }

    virtual operator std::string() const override;

    size_t get_total_size() const {
        return size;
    }

    size_t get_remain_size() const;

    virtual bool is_empty() const override {
        return allocated.empty();
    }

    // TODO: Test
    virtual void clear() override {
        size_map.clear();
        allocated.clear();
        idle_list_head->size = size;
        idle_list_head->mem_start = mem_chunk;
        idle_list_head->next = nullptr;
    }

private:
    
    struct IdleListNode {
        size_t size;
        std::byte* mem_start;
        std::unique_ptr<IdleListNode> next = nullptr;
    };

    void* acquire_memory(std::unique_ptr<IdleListNode>& p_node, size_t p_mem_size);
    void insert_free_memory(IdleListNode* p_node_before_insert, std::byte* p_insert_start, size_t p_insert_size);
    bool combine_idle_with_next(IdleListNode* p_node);
    std::unique_ptr<IdleListNode>& get_idle_node_before(void* p_loc);

    size_t size;
    std::unordered_map<void*, size_t> size_map;
    std::byte* mem_chunk;
    uint32_t idle_chunks_count;
    std::unique_ptr<IdleListNode> idle_list_head;
    std::set<MemID> allocated;

    bool is_in_pool(MemID p_id) const {
        return allocated.find(p_id) != allocated.end();
    }
};

}

#endif
