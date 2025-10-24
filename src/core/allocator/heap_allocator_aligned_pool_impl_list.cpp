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
#include "core/allocator/heap_allocator_aligned_pool_impl_list.hh"
#include "core/allocator/allocator.hh"
#include "core/logging/log.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#define WBE_HAAPIL_GET_HEADER_SIZE(p_header) (p_header & TOTAL_SIZE_MASK)
#define WBE_HAAPIL_GET_CHUNK_SIZE(p_chunk) WBE_HAAPIL_GET_HEADER_SIZE(*reinterpret_cast<Header*>(p_chunk))

#define WBE_HAAPIL_GET_HEADER_TYPE(p_header) ((HeaderType)((p_header & HEADER_TYPE_MASK) >> 60))
#define WBE_HAAPIL_GET_CHUNK_TYPE(p_chunk) WBE_HAAPIL_GET_HEADER_TYPE(*reinterpret_cast<Header*>(p_chunk))

#define WBE_HAAPIL_SET_HEADER(p_header, p_head_type, p_size) (*p_header = (((Header)(p_head_type) << 60) | (p_size)))
#define WBE_HAAPIL_SET_CHUNK_HEADER(p_chunk, p_type, p_size) WBE_HAAPIL_SET_HEADER(reinterpret_cast<Header*>(p_chunk), (p_type), (p_size))

namespace WhiteBirdEngine {

HeapAllocatorAlignedPoolImplicitList::HeapAllocatorAlignedPoolImplicitList(size_t p_size)
    : size(p_size) {
    if (p_size > TOTAL_SIZE_MASK) {
        throw std::runtime_error("Failed to create pool: size: " + std::to_string(p_size) + " exceeds maximum: " + std::to_string(TOTAL_SIZE_MASK) + ".");
    }
    mem_chunk = static_cast<char*>(aligned_alloc(WORD_SIZE, p_size));
    if (mem_chunk == nullptr) {
        throw std::runtime_error("Failed to create pool: malloc failed.");
    }
    memset(mem_chunk, 0, p_size);
    WBE_HAAPIL_SET_CHUNK_HEADER(mem_chunk, HeaderType::IDLE, size);
    possible_valid = mem_chunk;
}

HeapAllocatorAlignedPoolImplicitList::~HeapAllocatorAlignedPoolImplicitList() {
    if (!is_empty()) {
        wbe_console_log(WBE_CHANNEL_GLOBAL)->warning("Non-empty allocator destructed. Allocator status: " + static_cast<std::string>(*this));
    }
    free(mem_chunk);
    mem_chunk = nullptr;
}

MemID HeapAllocatorAlignedPoolImplicitList::allocate(size_t p_size, size_t p_alignment) {
    if (p_alignment % WORD_SIZE != 0) {
        throw std::runtime_error("Failed to allocate resource: allocation alignment must be a multiple of " + std::to_string(WORD_SIZE) + ".");
    }
    if (p_size == 0) {
        return MEM_NULL;
    }
    // Clamp the padding size to the default alignment.
    size_t aligned_size = get_align_size(p_size, WORD_SIZE) + WORD_SIZE;
    MemID result = find_valid_chunk<false>(aligned_size, p_alignment);
    if (result == MEM_NULL) {
        coalesce_all();
        result = find_valid_chunk<false>(aligned_size, p_alignment);
        if (result != MEM_NULL) {
            return result;
        }
    }
    else {
        return result;
    }
    std::string err_msg = "Failed to allocate memory: not enough space for memory pool.\n"
        "Trying to allocate: " + std::to_string(aligned_size) + " bytes.\n"
        "Pool status: " + static_cast<std::string>(*this);
    throw std::runtime_error(err_msg);
}

MemID HeapAllocatorAlignedPoolImplicitList::check_posible_free(size_t p_aligned_size, size_t p_alignment) {
    if (possible_valid == nullptr) {
        return MEM_NULL;
    }
    uintptr_t proxy_mem_start_addr = reinterpret_cast<uintptr_t>(possible_valid) + WORD_SIZE;
    char* idle_mem_start = reinterpret_cast<char*>(
        (proxy_mem_start_addr / p_alignment) * p_alignment == proxy_mem_start_addr ?
            proxy_mem_start_addr : (proxy_mem_start_addr / p_alignment + 1) * p_alignment
    ) - WORD_SIZE;
    // If idle node valid, insert.
    if (idle_mem_start + p_aligned_size <= possible_valid + WBE_HAAPIL_GET_CHUNK_SIZE(possible_valid)) {
        void* result_loc = acquire_memory(possible_valid, idle_mem_start, p_aligned_size);
        char* next_chunk = possible_valid + p_aligned_size;
        if (next_chunk < mem_chunk + size && WBE_HAAPIL_GET_CHUNK_TYPE(next_chunk) == HeaderType::IDLE) {
            possible_valid = next_chunk;
        }
        else {
            possible_valid = nullptr;
        }
        MemID result_id = reinterpret_cast<MemID>(result_loc) + WORD_SIZE;
        *static_cast<Header*>(result_loc) = p_aligned_size;
        internal_fragmentation_tracker = std::max(internal_fragmentation_tracker, (size_t)result_loc + p_aligned_size - (size_t)mem_chunk);
        return result_id;
    }
    return MEM_NULL;
}

template <bool COALESCE_ENABLED>
MemID HeapAllocatorAlignedPoolImplicitList::find_valid_chunk(size_t p_aligned_size, size_t p_alignment) {
    MemID possible_result = check_posible_free(p_aligned_size, p_alignment);
    if (possible_result != MEM_NULL) {
        return possible_result;
    }
    char* free_memory = get_next_free_memory<true, COALESCE_ENABLED>(mem_chunk);
    while (free_memory != nullptr) {
        // Find the aligned starting point.
        uintptr_t proxy_mem_start_addr = reinterpret_cast<uintptr_t>(free_memory) + WORD_SIZE;
        char* idle_mem_start = reinterpret_cast<char*>(
            (proxy_mem_start_addr / p_alignment) * p_alignment == proxy_mem_start_addr ?
                proxy_mem_start_addr : (proxy_mem_start_addr / p_alignment + 1) * p_alignment
        ) - WORD_SIZE;
        // If idle node valid, insert.
        if (idle_mem_start + p_aligned_size <= free_memory + WBE_HAAPIL_GET_CHUNK_SIZE(free_memory)) {
            void* result_loc = acquire_memory(free_memory, idle_mem_start, p_aligned_size);
            char* next_chunk = free_memory + p_aligned_size;
            if (next_chunk < mem_chunk + size && WBE_HAAPIL_GET_CHUNK_TYPE(next_chunk) == HeaderType::IDLE) {
                possible_valid = next_chunk;
            }
            else {
                possible_valid = nullptr;
            }
            MemID result_id = reinterpret_cast<MemID>(result_loc) + WORD_SIZE;
            *static_cast<Header*>(result_loc) = p_aligned_size;
            internal_fragmentation_tracker = std::max(internal_fragmentation_tracker, (size_t)result_loc + p_aligned_size - (size_t)mem_chunk);
            return result_id;
        }
        free_memory = get_next_free_memory<false, COALESCE_ENABLED>(free_memory);
    }
    // If not found, return null.
    return MEM_NULL;
}

void HeapAllocatorAlignedPoolImplicitList::deallocate(MemID p_mem) {
    WBE_DEBUG_ASSERT(is_in_pool(p_mem));
    char* data_loc = reinterpret_cast<char*>(p_mem - WORD_SIZE);
    size_t data_size = WBE_HAAPIL_GET_HEADER_SIZE(*reinterpret_cast<Header*>((p_mem - WORD_SIZE)));
    insert_free_memory(data_loc, data_size);
}

template <bool CHECK_FIRST, bool COALESCE_ENABLED>
char* HeapAllocatorAlignedPoolImplicitList::get_next_free_memory(char* p_from) {
    if constexpr (CHECK_FIRST) {
        while (WBE_HAAPIL_GET_CHUNK_TYPE(p_from) != HeaderType::IDLE) {
            p_from += WBE_HAAPIL_GET_CHUNK_SIZE(p_from);
            if (p_from >= mem_chunk + size) {
                return nullptr;
            }
        }
    }
    else {
        do {
            p_from += WBE_HAAPIL_GET_CHUNK_SIZE(p_from);
            if (p_from >= mem_chunk + size) {
                return nullptr;
            }
        } while (WBE_HAAPIL_GET_CHUNK_TYPE(p_from) != HeaderType::IDLE);
    }
    if constexpr (COALESCE_ENABLED) {
        coalesce_chunk(p_from);
    }
    return p_from;
}

void* HeapAllocatorAlignedPoolImplicitList::acquire_memory(char* p_idle_chunk, char* p_mem_start, size_t p_mem_size) {
    WBE_DEBUG_ASSERT(p_mem_start >= mem_chunk && p_mem_start + p_mem_size <= p_idle_chunk + WBE_HAAPIL_GET_CHUNK_SIZE(p_idle_chunk));
    WBE_DEBUG_ASSERT(WBE_HAAPIL_GET_CHUNK_TYPE(p_idle_chunk) == HeaderType::IDLE);
    size_t idle_chunk_size = WBE_HAAPIL_GET_CHUNK_SIZE(p_idle_chunk);
    size_t idle_before_size = p_mem_start - p_idle_chunk;
    // Insert the idle memory after the acquired memory chunk.
    if (p_mem_start + p_mem_size < p_idle_chunk + idle_chunk_size) {
        // The memory after the acquired should be size: idle_chunk_size - idle_size_before - inserted_memory_size
        WBE_HAAPIL_SET_CHUNK_HEADER(p_mem_start + p_mem_size, HeaderType::IDLE, idle_chunk_size - idle_before_size - p_mem_size);
    }
    // Insert the idle memory before the acquired memory chunk.
    if (p_idle_chunk != p_mem_start) {
        WBE_HAAPIL_SET_CHUNK_HEADER(p_idle_chunk, HeaderType::IDLE, idle_before_size);
    }
    // Get the inserted memory.
    WBE_HAAPIL_SET_CHUNK_HEADER(p_mem_start, HeaderType::OCCUPIED, p_mem_size);
    return p_mem_start;
}

void HeapAllocatorAlignedPoolImplicitList::insert_free_memory(char* p_insert_start, size_t p_insert_size) {
    WBE_HAAPIL_SET_CHUNK_HEADER(p_insert_start, HeaderType::IDLE, p_insert_size);
    coalesce_chunk(p_insert_start);
}

char* HeapAllocatorAlignedPoolImplicitList::get_chunk_before(void* p_loc) {
    char* curr = mem_chunk;
    while (curr < mem_chunk + size) {
        if (curr + WBE_HAAPIL_GET_CHUNK_SIZE(curr) == p_loc) {
            return curr;
        }
        curr += WBE_HAAPIL_GET_CHUNK_SIZE(curr);
    }
    throw std::runtime_error("Memory location is not a start of a memory pool.");
}


size_t HeapAllocatorAlignedPoolImplicitList::get_remain_size() const {
    char* curr = mem_chunk;
    size_t total = 0;
    while (curr < mem_chunk + size) {
        if (WBE_HAAPIL_GET_CHUNK_TYPE(curr) != HeaderType::OCCUPIED) {
            total += WBE_HAAPIL_GET_CHUNK_SIZE(curr);
        }
        curr += WBE_HAAPIL_GET_CHUNK_SIZE(curr);
    }
    return total;
}

bool HeapAllocatorAlignedPoolImplicitList::is_in_pool(MemID p_mem_id) const {
    char* curr = mem_chunk;
    char* mem_ptr = reinterpret_cast<char*>(p_mem_id);
    while (curr <= mem_ptr && curr < mem_chunk + size - WORD_SIZE) {
        if (curr + WORD_SIZE == mem_ptr && WBE_HAAPIL_GET_CHUNK_TYPE(curr) == HeaderType::OCCUPIED) {
            return true;
        }
        curr += WBE_HAAPIL_GET_CHUNK_SIZE(curr);
    }
    return false;
}

void HeapAllocatorAlignedPoolImplicitList::coalesce_all() const {
    char* curr = mem_chunk;
    do {
        coalesce_chunk(curr);
        curr += WBE_HAAPIL_GET_CHUNK_SIZE(curr);
    } while (curr < mem_chunk + size);
}

void HeapAllocatorAlignedPoolImplicitList::coalesce_chunk(char* p_chunk) const {
    if (WBE_HAAPIL_GET_CHUNK_TYPE(p_chunk) != HeaderType::IDLE) {
        return;
    }
    // Coalesce is not really changing the internal data. So here it allows to be called in a const function.
    size_t chunk_size = WBE_HAAPIL_GET_CHUNK_SIZE(p_chunk);
    while (p_chunk + chunk_size < mem_chunk + size && WBE_HAAPIL_GET_CHUNK_TYPE(p_chunk + chunk_size) == HeaderType::IDLE) {
        WBE_HAAPIL_SET_CHUNK_HEADER(p_chunk, HeaderType::IDLE, chunk_size + WBE_HAAPIL_GET_CHUNK_SIZE(p_chunk + chunk_size));
        chunk_size = WBE_HAAPIL_GET_CHUNK_SIZE(p_chunk);
    }
    possible_valid = p_chunk;
}

HeapAllocatorAlignedPoolImplicitList::operator std::string() const {
    std::stringstream ss;
    ss << "{";
    ss << "\"type\":\"HeapAllocatorAlignedPoolImplicitList\",";
    ss << "\"total_size\":" << get_total_size() << ",";
    ss << "\"chunk_layout\":[";
    char* curr = mem_chunk;
    bool first = true;
    while (curr < mem_chunk + size) {
        if (!first) ss << ",";
        first = false;
        ss << "{"
            << "\"occupied\":" << std::to_string(WBE_HAAPIL_GET_CHUNK_TYPE(curr) == HeaderType::OCCUPIED) << ","
            << "\"begin\":" << (curr - mem_chunk) << ","
            << "\"size\":" << WBE_HAAPIL_GET_CHUNK_SIZE(curr)
            << "}";
        curr += WBE_HAAPIL_GET_CHUNK_SIZE(curr);
    }
    ss << "]";
    ss << "}";
    return ss.str();
}

}
