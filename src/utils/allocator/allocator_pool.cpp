#include "utils/defs.h"
#include <sstream>
#include <string>
#include "utils/allocator/allocator_pool.h"

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
    std::unique_ptr<IdleListNode>* valid_idle_node = &idle_list_head;
    size_t align_size = get_align_size(p_size);
    while (valid_idle_node != nullptr) {
        if ((*valid_idle_node)->size >= p_size) {
            void* result = acquire_memory(*valid_idle_node, align_size);
            allocated.insert(result);
            size_map[result] = align_size;
            return result;
        }
        valid_idle_node = &((*valid_idle_node)->next);
    }
    throw std::runtime_error("Failed to create object: not enough space for object pool.\nTrying to allocate: " + std::to_string(align_size) + " bytes.");
}

void AllocatorPool::deallocate(void* p_mem) {
    auto mem_ptr_iter = size_map.find(p_mem);
    if (mem_ptr_iter == size_map.end()) {
        throw std::runtime_error("Failed to destroy object: object not allocated before in current memory pool.");
    }
    allocated.erase(p_mem);
    size_map.erase(p_mem);
    if (idle_list_head == nullptr || idle_list_head->mem_start > p_mem) {
        insert_free_memory(nullptr, static_cast<std::byte*>(p_mem), mem_ptr_iter->second);
        return;
    }
    IdleListNode* curr = idle_list_head.get();
    while (curr != nullptr) {
        if (curr->next == nullptr || curr->next->mem_start > p_mem) {
            insert_free_memory(curr, static_cast<std::byte*>(p_mem), mem_ptr_iter->second);
            return;
        }
        curr = curr->next.get();
    }
    PE_DEBUG_ASSERT(false); // unreachable
}

void* AllocatorPool::acquire_memory(std::unique_ptr<IdleListNode>& p_node, size_t p_mem_size) {
    PE_DEBUG_ASSERT(p_node->size >= p_mem_size);
    p_node->size -= p_mem_size;
    void* mem_start = p_node->mem_start;
    if (p_node->size == 0) {
        --idle_chunks_count;
        if (p_node->next == nullptr) {
            p_node = nullptr;
            return mem_start;
        }
        p_node->size = p_node->next->size;
        p_node->mem_start = p_node->next->mem_start;
        p_node->next = std::move(p_node->next->next);
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
        insert_node->next = std::move(idle_list_head);
        idle_list_head = std::move(insert_node);
        combine_idle_with_next(idle_list_head.get());
        return;
    }

    insert_node->next = std::move(p_node_before_insert->next);
    p_node_before_insert->next = std::move(insert_node);
    if (combine_idle_with_next(p_node_before_insert)) {
        combine_idle_with_next(p_node_before_insert);
    }
    else if (p_node_before_insert->next != nullptr) {
        combine_idle_with_next(p_node_before_insert->next.get());
    }
}

bool AllocatorPool::combine_idle_with_next(IdleListNode* p_node) {
    PE_DEBUG_ASSERT(p_node != nullptr);
    if (p_node->next == nullptr || p_node->mem_start + p_node->size != p_node->next->mem_start) {
        return false;
    }
    p_node->size += p_node->next->size;
    p_node->next = std::move(p_node->next->next);
    --idle_chunks_count;
    return true;
}

size_t AllocatorPool::get_remain_size() const {
    IdleListNode* node = idle_list_head.get();
    size_t total = 0;
    while (node != nullptr) {
        total += node->size;
        node = node->next.get();
    }
    return total;
}

AllocatorPool::operator std::string() const {
    std::stringstream ss;
    ss << "AllocatorPool: {obj_count: " << obj_count() << ";\n";
    ss << "free_chunk_layout: {\n";
    IdleListNode* node = idle_list_head.get();
    while (node != nullptr) {
        ss << "chunk: {\nbegin: " << node->mem_start - mem_chunk.get() << ";\nsize: " << node->size << ";\n};\n";
        node = node->next.get();
    }
    ss << "};\n};";
    return ss.str();
}


}

