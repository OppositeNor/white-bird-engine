#ifndef __WBE_ALLOCATOR_POOL_H__
#define __WBE_ALLOCATOR_POOL_H__

#include "allocator.h"
#include <cstddef>
#include <memory>
#include <unordered_map>

namespace WhiteBirdEngine {

class AllocatorPool : public Allocator {
public:
    AllocatorPool() : AllocatorPool(1024) {}
    virtual ~AllocatorPool() override;
    AllocatorPool(const AllocatorPool&) = delete;
    AllocatorPool(AllocatorPool&&) = delete;
    AllocatorPool& operator=(const AllocatorPool&) = delete;
    AllocatorPool& operator=(AllocatorPool&&) = delete;

    AllocatorPool(size_t p_size, size_t p_alignment = alignof(std::max_align_t));

    virtual MemID allocate(size_t p_size) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) override {
        return alloc_map.find(p_id)->second;
    }

    virtual uint32_t obj_count() const override {
        return allocated.size();
    }

    virtual const std::set<MemID>& get_allocated() const override {
        return allocated;
    }

    virtual operator std::string() const override;

    size_t get_total_size() const {
        return size;
    }

    size_t get_remain_size() const;

    size_t get_alignment() const {
        return alignment;
    }

    // TODO: Test
    void flush();

    // TODO: Test
    bool is_flushed() const;


private:
    
    struct IdleListNode {
        size_t size;
        std::byte* mem_start;
        std::unique_ptr<IdleListNode> next = nullptr;
    };

    void* acquire_memory(std::unique_ptr<IdleListNode>& p_node, size_t p_mem_size);
    void insert_free_memory(IdleListNode* p_node_before_insert, std::byte* p_insert_start, size_t p_insert_size);
    bool combine_idle_with_next(IdleListNode* p_node);
    void mem_move(MemID p_id1, void* p_loc);
    std::unique_ptr<IdleListNode>& get_idle_node_before(void* p_loc);

    MemID curr_id;
    size_t size;
    size_t alignment;
    std::unordered_map<MemID, void*> alloc_map;
    std::unordered_map<void*, size_t> size_map;
    std::unique_ptr<std::byte[]> mem_chunk;
    uint32_t idle_chunks_count;
    std::unique_ptr<IdleListNode> idle_list_head;
    std::set<MemID> allocated;
};

}

#endif
