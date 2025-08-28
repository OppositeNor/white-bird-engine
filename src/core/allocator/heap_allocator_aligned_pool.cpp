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
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/allocator/allocator.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace WhiteBirdEngine {

HeapAllocatorAlignedPool::HeapAllocatorAlignedPool(size_t p_size)
    : size(p_size), mem_chunk(new char[p_size]) {
    if (mem_chunk == nullptr) {
        throw std::runtime_error("Failed to allocate memory for pool.");
    }
    idle_list_head = std::make_unique<IdleListNode>();
    idle_list_head->size = p_size;
    idle_list_head->next = nullptr;
    idle_list_head->mem_start = mem_chunk;
    idle_chunks_count = 1;
}

HeapAllocatorAlignedPool::~HeapAllocatorAlignedPool() {
    delete[] mem_chunk;
}

MemID HeapAllocatorAlignedPool::allocate(size_t p_size, size_t p_alignment) {
    std::unique_ptr<IdleListNode>* valid_idle_node = &idle_list_head;
    size_t aligned_size = get_align_size(p_size, p_alignment);
    while (*valid_idle_node != nullptr) {
        uintptr_t mem_start_addr = (uintptr_t)((*valid_idle_node)->mem_start);
        char* idle_node_mem_start = reinterpret_cast<char*>(
            (mem_start_addr / p_alignment) * p_alignment == mem_start_addr ? mem_start_addr : (mem_start_addr / p_alignment + 1) * p_alignment
        );
        if (idle_node_mem_start + aligned_size <= (*valid_idle_node)->mem_start + (*valid_idle_node)->size) {
            void* result = acquire_memory(*valid_idle_node, idle_node_mem_start, aligned_size);
            allocated.insert((MemID)(result));
            size_map[result] = aligned_size;
#ifdef WBE_ENABLE_HEAP_ALLOCATOR_TRACKER
            max_data_loc_tracker = std::max(max_data_loc_tracker, (size_t)result + aligned_size - (size_t)mem_chunk);
#endif
            return (MemID)result;
        }
        valid_idle_node = &((*valid_idle_node)->next);
    }
    std::string err_msg = "Failed to allocate memory: not enough space for memory pool.\n"
        "Trying to allocate: " + std::to_string(aligned_size) + " bytes.\n"
        "Pool status: " + (std::string)*this;
    throw std::runtime_error(err_msg);
}

void HeapAllocatorAlignedPool::deallocate(MemID p_mem) {
    if (!is_in_pool(p_mem)) {
        throw std::runtime_error("Failed to deallocate memory: memory id: " + std::to_string(p_mem) + " not allocated in this memory pool.\n"
                                 "Pool status: " + (std::string)*this);
    }
    void* data_loc = reinterpret_cast<void*>(p_mem);
    size_t data_size = size_map[data_loc];
    allocated.erase(p_mem);
    size_map.erase(data_loc);
    if (idle_list_head == nullptr || idle_list_head->mem_start > data_loc) {
        insert_free_memory(nullptr, static_cast<char*>(data_loc), data_size);
        return;
    }
    IdleListNode* insert_pos = get_idle_node_before(data_loc).get();
    insert_free_memory(insert_pos, static_cast<char*>(data_loc), data_size);
}

void* HeapAllocatorAlignedPool::get(MemID p_id) const {
    return reinterpret_cast<void*>(p_id);
}

void* HeapAllocatorAlignedPool::acquire_memory(std::unique_ptr<IdleListNode>& p_node, char* p_mem_start, size_t p_mem_size) {
    WBE_DEBUG_ASSERT(p_mem_start >= p_node->mem_start);
    WBE_DEBUG_ASSERT(p_mem_start + p_mem_size <= p_node->mem_start + p_node->size);
    if (p_node->mem_start == p_mem_start) {
        p_node->size -= p_mem_size;
        void* node_mem_start = p_node->mem_start;
        if (p_node->size == 0) {
            --idle_chunks_count;
            if (p_node->next == nullptr) {
                p_node = nullptr;
                return node_mem_start;
            }
            p_node->size = p_node->next->size;
            p_node->mem_start = p_node->next->mem_start;
            p_node->next = std::move(p_node->next->next);
        }
        else {
            p_node->mem_start += p_mem_size;
        }
        return node_mem_start;
    }
    else {
        int node_size_diff = p_mem_start - p_node->mem_start;
        if (p_mem_start + p_mem_size != p_node->mem_start + p_node->size) {
            std::unique_ptr<IdleListNode> node_next = std::make_unique<IdleListNode>();
            node_next->mem_start = p_mem_start + p_mem_size;
            node_next->size = p_node->size - node_size_diff - p_mem_size;
            node_next->next = std::move(p_node->next);
            p_node->next = std::move(node_next);
        }
        p_node->size = node_size_diff;
        return p_mem_start;
    }
}

void HeapAllocatorAlignedPool::insert_free_memory(IdleListNode* p_node_before_insert, char* p_insert_start, size_t p_insert_size) {
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

bool HeapAllocatorAlignedPool::combine_idle_with_next(IdleListNode* p_node) {
    WBE_DEBUG_ASSERT(p_node != nullptr);
    if (p_node->next == nullptr || p_node->mem_start + p_node->size != p_node->next->mem_start) {
        return false;
    }
    p_node->size += p_node->next->size;
    p_node->next = std::move(p_node->next->next);
    --idle_chunks_count;
    return true;
}

std::unique_ptr<HeapAllocatorAlignedPool::IdleListNode>& HeapAllocatorAlignedPool::get_idle_node_before(void* p_loc) {
    std::unique_ptr<IdleListNode>* curr = &idle_list_head;
    while (curr != nullptr) {
        if ((*curr)->next == nullptr || (*curr)->next->mem_start > p_loc) {
            return *curr;
        }
        curr = &((*curr)->next);
    }
    throw std::runtime_error("Unreachable code.");
}


size_t HeapAllocatorAlignedPool::get_remain_size() const {
    IdleListNode* node = idle_list_head.get();
    size_t total = 0;
    while (node != nullptr) {
        total += node->size;
        node = node->next.get();
    }
    return total;
}

}
