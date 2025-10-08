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
#include "core/allocator/heap_allocator_fixed_size_pool.hh"
#include "core/logging/log.hh"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <stdexcept>

namespace WhiteBirdEngine {
HeapAllocatorFixedSizePool::~HeapAllocatorFixedSizePool() {
    if (obj_count() != 0) {
        wbe_console_log(WBE_CHANNEL_GLOBAL)->warning("HeapAllocatorFixedSizePool not empty during destruction.");
    }
    free(mem_chunk);
}

inline MemID HeapAllocatorFixedSizePool::allocate(size_t p_size) {
    if (p_size == 0) {
        p_size = element_size;
    }
    if (p_size != element_size) {
        throw std::runtime_error(std::format("Failed to allocate memory: size must be: ", element_size));
    }
    if (alloc_obj_count >= max_obj) {
        throw std::runtime_error("Failed to allocate memory: not enough space for memory pool.");
    }
    InternalID curr_id = retrieve_valid_index();
    write_info(curr_id, ++alloc_obj_count);
    return curr_id;
}

inline void HeapAllocatorFixedSizePool::deallocate(MemID p_mem) {
    DataIndex data_index = get_data_index(p_mem);
    if (data_index == MEM_NULL || p_mem > max_obj) {
        throw std::runtime_error("Failed to deallocate memory: memory not allocated in this memory pool.");
    }
    if (alloc_obj_count > 1 && data_index != alloc_obj_count) {
        void* data_loc = get_mem_loc_at_id(p_mem);
        InternalID copy_from_id = get_internal_id(alloc_obj_count);
        void* copy_from = get_mem_loc_at_id(copy_from_id);
        memcpy(data_loc, copy_from, element_size);
        write_info(copy_from_id, data_index);
    }
    write_data_index(alloc_obj_count, MEM_NULL);
    --alloc_obj_count;
}

inline HeapAllocatorFixedSizePool::operator std::string() const {
    std::stringstream ss;
    ss << "{";
    ss << "\"type\":\"HeapAllocatorFixedSizePool\",";
    ss << "\"size\":" << element_size << ",";
    ss << "\"obj_count\":" << obj_count() << ",";
    ss << "\"max_obj\":" << static_cast<uint32_t>(max_obj) << ",";
    ss << "\"allocated\":[";
    auto alloc_mems = get_allocated();
    bool first = true;
    for (auto alloc_mem : alloc_mems) {
        if (!first) ss << ",";
        first = false;
        ss << alloc_mem;
    }
    ss << "]";
    ss << "}";
    return ss.str();
}


}
