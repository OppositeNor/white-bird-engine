#ifndef __PE_ALLOCATOR_POOL_H__
#define __PE_ALLOCATOR_POOL_H__

#include "pe_allocator.h"
#include "utils/pe_defs.h"
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace PencilEngine {

class AllocatorPool : Allocator {
public:
    AllocatorPool() : AllocatorPool(1024) {}
    virtual ~AllocatorPool() override {}
    AllocatorPool(const AllocatorPool&) = delete;
    AllocatorPool(AllocatorPool&&) = delete;
    AllocatorPool& operator=(const AllocatorPool&) = delete;
    AllocatorPool& operator=(AllocatorPool&&) = delete;

    AllocatorPool(size_t p_size, size_t p_alignment = alignof(std::max_align_t));

    void* allocate(size_t p_size) override;

    void deallocate(void* p_mem) override;

    size_t get_size() const {
        return size;
    }

private:
    
    struct IdleListNode {
        size_t size;
        std::byte* mem_start;
        std::unique_ptr<IdleListNode> next = nullptr;
    };

    void* acquire_memory(IdleListNode* p_node, size_t p_mem_size);

    void insert_free_memory(IdleListNode* p_node_before_insert, std::byte* p_insert_start, size_t p_insert_size) noexcept;

    void combine_idle_with_next(IdleListNode* p_node);

    size_t get_align_size(size_t p_size) {
        return (size_t)std::ceil(static_cast<double>(p_size) / alignment) * alignment;
    }

    size_t size;
    size_t alignment;
    std::unordered_map<void*, size_t> alloc_mem;
    std::unique_ptr<std::byte[]> mem_chunk;
    uint32_t idle_chunks_count;
    std::unique_ptr<IdleListNode> idle_list_head;

};

}

#endif
