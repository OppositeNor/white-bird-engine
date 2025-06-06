#include "utils/defs.h"
#include "utils/utils.h"
#include <cstring>
#include <sstream>
#include <string>
#include "core/allocator/allocator_pool.h"

namespace WhiteBirdEngine {

AllocatorPool::~AllocatorPool() {
    // TODO: warning if not all memory freed (which is probably caused by a memory leak error)
}

AllocatorPool::AllocatorPool(size_t p_size, size_t p_alignment)
    : curr_id(1), size(p_size), alignment(p_alignment), mem_chunk(std::make_unique<std::byte[]>(get_align_size(size, alignment))) {
    idle_list_head = std::make_unique<IdleListNode>();
    idle_list_head->size = p_size;
    idle_list_head->next = nullptr;
    idle_list_head->mem_start = mem_chunk.get();
    idle_chunks_count = 1;
}

MemID AllocatorPool::allocate(size_t p_size) {
    std::unique_ptr<IdleListNode>* valid_idle_node = &idle_list_head;
    size_t align_size = get_align_size(p_size, alignment);
    while (valid_idle_node != nullptr) {
        if ((*valid_idle_node)->size >= p_size) {
            void* result = acquire_memory(*valid_idle_node, align_size);
            allocated.insert(curr_id);
            alloc_map[curr_id] = result;
            size_map[result] = align_size;
            return curr_id++;
        }
        valid_idle_node = &((*valid_idle_node)->next);
    }
    throw std::runtime_error("Failed to allocate memory: not enough space for memory pool.\nTrying to allocate: " + std::to_string(align_size) + " bytes.");
}

void AllocatorPool::deallocate(MemID p_mem) {
    auto mem_ptr_iter = alloc_map.find(p_mem);
    if (mem_ptr_iter == alloc_map.end()) {
        throw std::runtime_error("Failed to deallocate memory: memory not allocated before in this memory pool.");
    }
    void* data_loc = mem_ptr_iter->second;
    size_t data_size = size_map[data_loc];
    allocated.erase(p_mem);
    alloc_map.erase(p_mem);
    size_map.erase(data_loc);
    if (idle_list_head == nullptr || idle_list_head->mem_start > data_loc) {
        insert_free_memory(nullptr, static_cast<std::byte*>(data_loc), data_size);
        return;
    }
    IdleListNode* insert_pos = get_idle_node_before(data_loc).get();
    insert_free_memory(insert_pos, static_cast<std::byte*>(data_loc), data_size);
}

void* AllocatorPool::acquire_memory(std::unique_ptr<IdleListNode>& p_node, size_t p_mem_size) {
    WBE_DEBUG_ASSERT(p_node->size >= p_mem_size);
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
    WBE_DEBUG_ASSERT(p_node != nullptr);
    if (p_node->next == nullptr || p_node->mem_start + p_node->size != p_node->next->mem_start) {
        return false;
    }
    p_node->size += p_node->next->size;
    p_node->next = std::move(p_node->next->next);
    --idle_chunks_count;
    return true;
}

void AllocatorPool::mem_move(MemID p_id1, void* p_loc) {
    std::byte* data_addr = static_cast<std::byte*>(get(p_id1));
    size_t data_size = size_map[data_addr];
    memcpy(p_loc, data_addr, data_size);
    alloc_map[p_id1] = p_loc;
    size_map.erase(data_addr);
    size_map[data_addr] = data_size;
    IdleListNode* insert_pos = get_idle_node_before(data_addr).get();
    insert_free_memory(insert_pos, data_addr, data_size);
    auto& remove_pos = get_idle_node_before(data_addr);
    acquire_memory(remove_pos, data_size);
}

std::unique_ptr<AllocatorPool::IdleListNode>& AllocatorPool::get_idle_node_before(void* p_loc) {
    std::unique_ptr<IdleListNode>* curr = &idle_list_head;
    while (curr != nullptr) {
        if ((*curr)->next == nullptr || (*curr)->next->mem_start > p_loc) {
            return *curr;
        }
        curr = &((*curr)->next);
    }
    throw std::runtime_error("Unreachable code.");
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

void AllocatorPool::flush() {
    IdleListNode* node = idle_list_head.get();
    while (node != nullptr) {
        // TODO
    }
}

bool AllocatorPool::is_flushed() const {
    return idle_list_head == nullptr
        || (idle_list_head->next == nullptr
            && idle_list_head->mem_start + idle_list_head->size == mem_chunk.get() + size);
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

