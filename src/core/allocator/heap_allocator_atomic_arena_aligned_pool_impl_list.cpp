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
#include "core/allocator/heap_allocator_atomic_arena_aligned_pool_impl_list.hh"
#include "core/allocator/i_allocator.hh"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace WhiteBirdEngine {

HeapAllocatorAtomicArenaAlignedPoolImplicitList::HeapAllocatorAtomicArenaAlignedPoolImplicitList(size_t p_size)
    : HeapAllocatorAtomicArenaAlignedPoolImplicitList(p_size, DEFAULT_ARENA_COUNT) {
}

HeapAllocatorAtomicArenaAlignedPoolImplicitList::HeapAllocatorAtomicArenaAlignedPoolImplicitList(size_t p_size, size_t p_arena_count)
    : total_size(p_size), arena_count(p_arena_count) {
    if (total_size == 0) {
        throw std::runtime_error("Arena allocator size must not be 0.");
    }
    if (arena_count == 0) {
        throw std::runtime_error("Arena allocator arena count must not be 0.");
    }
    if (total_size % arena_count != 0) {
        throw std::runtime_error(std::format("Arena allocator size {} must be divisible by arena count {}.", total_size, arena_count));
    }
    arena_size = total_size / arena_count;
    if (arena_size < HEADER_SIZE) {
        throw std::runtime_error(std::format("Arena size {} must be at least {}.", arena_size, HEADER_SIZE));
    }
    if (arena_size % HEADER_SIZE != 0) {
        throw std::runtime_error(std::format("Arena size {} must be a multiple of {}.", arena_size, HEADER_SIZE));
    }

    memory_chunk = MemoryChunk(total_size, HEADER_SIZE);
    create_arenas();
}

HeapAllocatorAtomicArenaAlignedPoolImplicitList::~HeapAllocatorAtomicArenaAlignedPoolImplicitList() {
    destroy_arenas();
}

MemID HeapAllocatorAtomicArenaAlignedPoolImplicitList::allocate(size_t p_size, size_t p_alignment) {
    MemID result = try_allocate(p_size, p_alignment);
    if (result != MEM_NULL || p_size == 0) {
        return result;
    }
    throw std::runtime_error(std::format("Failed to allocate memory: not enough space in any arena.\nTrying to allocate: {} bytes.\nPool status: {}",
        p_size,
        static_cast<std::string>(*this)));
}

MemID HeapAllocatorAtomicArenaAlignedPoolImplicitList::try_allocate(size_t p_size, size_t p_alignment) {
    size_t first_arena_index = next_arena_index.fetch_add(1, std::memory_order_relaxed) % arena_count;
    for (size_t i = 0; i < arena_count; ++i) {
        size_t arena_index = (first_arena_index + i) % arena_count;
        MemID result = try_allocate_in_arena(p_size, p_alignment, arena_index);
        if (result != MEM_NULL || p_size == 0) {
            return result;
        }
    }
    return MEM_NULL;
}

MemID HeapAllocatorAtomicArenaAlignedPoolImplicitList::allocate_in_arena(size_t p_size, size_t p_alignment, size_t p_arena_index) {
    MemID result = try_allocate_in_arena(p_size, p_alignment, p_arena_index);
    if (result != MEM_NULL || p_size == 0) {
        return result;
    }
    throw std::runtime_error(std::format("Failed to allocate memory: not enough space in arena {}.\nTrying to allocate: {} bytes.\nPool status: {}",
        p_arena_index,
        p_size,
        static_cast<std::string>(*this)));
}

MemID HeapAllocatorAtomicArenaAlignedPoolImplicitList::try_allocate_in_arena(size_t p_size, size_t p_alignment, size_t p_arena_index) {
    if (p_arena_index >= arena_count) {
        throw std::runtime_error(std::format("Arena index {} exceeds arena count {}.", p_arena_index, arena_count));
    }
    return get_arena(p_arena_index).try_allocate(p_size, p_alignment);
}

void HeapAllocatorAtomicArenaAlignedPoolImplicitList::deallocate(MemID p_mem) {
    if (p_mem == MEM_NULL) {
        return;
    }
    get_arena(get_arena_index(p_mem)).deallocate(p_mem);
}

bool HeapAllocatorAtomicArenaAlignedPoolImplicitList::is_empty() const {
    for (size_t i = 0; i < arena_count; ++i) {
        if (!get_arena(i).is_empty()) {
            return false;
        }
    }
    return true;
}

void HeapAllocatorAtomicArenaAlignedPoolImplicitList::clear() {
    for (size_t i = 0; i < arena_count; ++i) {
        get_arena(i).clear();
    }
}

size_t HeapAllocatorAtomicArenaAlignedPoolImplicitList::get_allocated_data_size(MemID p_mem_id) const {
    return get_arena(get_arena_index(p_mem_id)).get_allocated_data_size(p_mem_id);
}

size_t HeapAllocatorAtomicArenaAlignedPoolImplicitList::get_remain_size() const {
    size_t remain_size = 0;
    for (size_t i = 0; i < arena_count; ++i) {
        remain_size += get_arena(i).get_remain_size();
    }
    return remain_size;
}

std::vector<size_t> HeapAllocatorAtomicArenaAlignedPoolImplicitList::get_internal_fragmentation_tracker() const {
    std::vector<size_t> result;
    result.reserve(arena_count);
    for (size_t i = 0; i < arena_count; ++i) {
        result.push_back(get_arena(i).get_internal_fragmentation_tracker());
    }
    return result;
}

bool HeapAllocatorAtomicArenaAlignedPoolImplicitList::is_in_pool(MemID p_mem_id) const {
    if (!is_in_memory_chunk(p_mem_id)) {
        return false;
    }
    return get_arena(get_arena_index(p_mem_id)).is_in_pool(p_mem_id);
}

void HeapAllocatorAtomicArenaAlignedPoolImplicitList::check_broken() const {
    for (size_t i = 0; i < arena_count; ++i) {
        get_arena(i).check_broken();
    }
}

HeapAllocatorAtomicArenaAlignedPoolImplicitList::operator std::string() const {
    std::stringstream ss;
    ss << "{";
    ss << R"("type":"HeapAllocatorAtomicArenaAlignedPoolImplicitList",)";
    ss << "\"total_size\":" << total_size << ",";
    ss << "\"arena_count\":" << arena_count << ",";
    ss << "\"arena_size\":" << arena_size << ",";
    ss << "\"arenas\":[";
    for (size_t i = 0; i < arena_count; ++i) {
        if (i != 0) {
            ss << ",";
        }
        ss << static_cast<std::string>(get_arena(i));
    }
    ss << "]";
    ss << "}";
    return ss.str();
}

void HeapAllocatorAtomicArenaAlignedPoolImplicitList::create_arenas() {
    void* arena_memory = ::operator new(sizeof(ArenaAllocator) * arena_count, std::align_val_t(alignof(ArenaAllocator)));
    arena_storage = static_cast<std::byte*>(arena_memory);
    size_t constructed_count = 0;
    try {
        for (; constructed_count < arena_count; ++constructed_count) {
            new (&get_arena(constructed_count)) ArenaAllocator(memory_chunk, constructed_count * arena_size, arena_size);
        }
    } catch (...) {
        for (size_t i = constructed_count; i > 0; --i) {
            get_arena(i - 1).~ArenaAllocator();
        }
        ::operator delete(arena_storage, std::align_val_t(alignof(ArenaAllocator)));
        arena_storage = nullptr;
        throw;
    }
}

void HeapAllocatorAtomicArenaAlignedPoolImplicitList::destroy_arenas() {
    if (arena_storage == nullptr) {
        return;
    }
    for (size_t i = arena_count; i > 0; --i) {
        get_arena(i - 1).~ArenaAllocator();
    }
    ::operator delete(arena_storage, std::align_val_t(alignof(ArenaAllocator)));
    arena_storage = nullptr;
}

HeapAllocatorAtomicArenaAlignedPoolImplicitList::ArenaAllocator& HeapAllocatorAtomicArenaAlignedPoolImplicitList::get_arena(
    size_t p_arena_index) {
    return *reinterpret_cast<ArenaAllocator*>(arena_storage + p_arena_index * sizeof(ArenaAllocator));
}

const HeapAllocatorAtomicArenaAlignedPoolImplicitList::ArenaAllocator& HeapAllocatorAtomicArenaAlignedPoolImplicitList::get_arena(
    size_t p_arena_index) const {
    return *reinterpret_cast<const ArenaAllocator*>(arena_storage + p_arena_index * sizeof(ArenaAllocator));
}

bool HeapAllocatorAtomicArenaAlignedPoolImplicitList::is_in_memory_chunk(MemID p_mem_id) const {
    if (p_mem_id == MEM_NULL || !memory_chunk.is_valid()) {
        return false;
    }
    uintptr_t memory_begin = reinterpret_cast<uintptr_t>(memory_chunk.get());
    uintptr_t memory_id = static_cast<uintptr_t>(p_mem_id);
    return memory_id >= memory_begin && memory_id - memory_begin < total_size;
}

size_t HeapAllocatorAtomicArenaAlignedPoolImplicitList::get_arena_index(MemID p_mem_id) const {
    if (!is_in_memory_chunk(p_mem_id)) {
        throw std::runtime_error("Memory id does not belong to this arena allocator.");
    }
    uintptr_t memory_begin = reinterpret_cast<uintptr_t>(memory_chunk.get());
    uintptr_t memory_id = static_cast<uintptr_t>(p_mem_id);
    size_t arena_index = (memory_id - memory_begin) / arena_size;
    if (arena_index >= arena_count) {
        throw std::runtime_error("Memory id does not belong to a valid arena.");
    }
    return arena_index;
}

} // namespace WhiteBirdEngine
