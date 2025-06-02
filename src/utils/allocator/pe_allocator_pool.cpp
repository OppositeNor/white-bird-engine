#include "utils/pe_defs.h"
#include "utils/allocator/pe_allocator_pool.h"

namespace PencilEngine {
AllocatorPool::AllocatorPool(size_t p_size, size_t p_alignment)
    : size(p_size), alignment(p_alignment), mem_chunk(std::make_unique<std::byte[]>(get_align_size(size))) {
    idle_list_head = std::make_unique<IdleListNode>();
    idle_list_head->size = p_size;
    idle_list_head->next = nullptr;
    idle_list_head->mem_start = mem_chunk.get();
    idle_chunks_count = 1;
}

void* AllocatorPool::allocate(size_t p_size) {
    IdleListNode* valid_idle_node = idle_list_head.get();
    while (valid_idle_node->next == nullptr) {
        if (valid_idle_node->size > p_size) {
            size_t align_size = get_align_size(p_size);
            void* result = acquire_memory(valid_idle_node, align_size);
            return result;
        }
        valid_idle_node = valid_idle_node->next.get();
    }
    throw std::runtime_error("Failed to create object: not enough space for object pool.");
}

void AllocatorPool::deallocate(void* p_mem) {
    auto mem_ptr_iter = size_map.find(p_mem);
    if (mem_ptr_iter == size_map.end()) {
        throw std::runtime_error("Failed to destroy object: object not allocated before in current memory pool.");
    }
    if (idle_list_head == nullptr) {
        insert_free_memory(nullptr, static_cast<std::byte*>(p_mem), mem_ptr_iter->second);
        return;
    }
    IdleListNode* curr = idle_list_head.get();
    while (curr != nullptr) {
        if (curr->next == nullptr || curr->next->mem_start > p_mem) {
            insert_free_memory(curr, static_cast<std::byte*>(p_mem), mem_ptr_iter->second);
        }
    }
}

void* AllocatorPool::acquire_memory(IdleListNode* p_node, size_t p_mem_size) {
    PE_DEBUG_ASSERT(p_node->size >= p_mem_size);
    p_node->size -= p_mem_size;
    void* mem_start = p_node->mem_start;
    if (p_node->size == 0) {
        p_node->size = p_node->next->size;
        p_node->mem_start = p_node->next->mem_start;
        p_node->next = std::move(p_node->next->next);
        --idle_chunks_count;
    }
    else {
        p_node->mem_start += p_mem_size;
    }
    return mem_start;
}

void AllocatorPool::insert_free_memory(IdleListNode* p_node_before_insert, std::byte* p_insert_start, size_t p_insert_size) {
    if (idle_list_head == nullptr) {
        idle_list_head = std::make_unique<IdleListNode>();
        ++idle_chunks_count;
        idle_list_head->mem_start = p_insert_start;
        idle_list_head->size = p_insert_size;
        return;
    }

    std::unique_ptr<IdleListNode> insert_node = std::make_unique<IdleListNode>();
    ++idle_chunks_count;
    insert_node->mem_start = p_insert_start;
    insert_node->size = p_insert_size;
    if (p_node_before_insert == nullptr) {
        insert_node->next = std::move(idle_list_head->next);
        idle_list_head = std::move(insert_node);
        combine_idle_with_next(idle_list_head.get());
    }

    insert_node->next = std::move(p_node_before_insert->next);
    p_node_before_insert->next = std::move(insert_node);
    combine_idle_with_next(p_node_before_insert);
    if (p_node_before_insert->next != nullptr) {
        combine_idle_with_next(p_node_before_insert->next.get());
    }
}

void AllocatorPool::combine_idle_with_next(IdleListNode* p_node) {
    PE_DEBUG_ASSERT(p_node != nullptr);
    if (p_node->next == nullptr || p_node->mem_start + p_node->size != p_node->next->mem_start) {
        return;
    }
    p_node->size += p_node->next->size;
    p_node->next = std::move(p_node->next->next);
    --idle_chunks_count;
}
}
