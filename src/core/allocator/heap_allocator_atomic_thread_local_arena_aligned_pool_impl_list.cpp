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
#include "core/allocator/heap_allocator_atomic_thread_local_arena_aligned_pool_impl_list.hh"
#include "core/allocator/i_allocator.hh"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>

namespace WhiteBirdEngine {

HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList(size_t p_size)
    : HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList(p_size, DEFAULT_ARENA_COUNT) {
}

HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList(
    size_t p_size, size_t p_arena_count)
    : total_size(p_size), arena_count(p_arena_count) {
    if (total_size == 0) {
        throw std::runtime_error("Thread local arena allocator size must not be 0.");
    }
    if (arena_count == 0) {
        throw std::runtime_error("Thread local arena allocator arena count must not be 0.");
    }
    if (total_size % arena_count != 0) {
        throw std::runtime_error(
            std::format("Thread local arena allocator size {} must be divisible by arena count {}.", total_size, arena_count));
    }
    arena_size = total_size / arena_count;
    if (arena_size < HEADER_SIZE) {
        throw std::runtime_error(std::format("Arena size {} must be at least {}.", arena_size, HEADER_SIZE));
    }
    if (arena_size % HEADER_SIZE != 0) {
        throw std::runtime_error(std::format("Arena size {} must be a multiple of {}.", arena_size, HEADER_SIZE));
    }

    memory_chunk = MemoryChunk(total_size, HEADER_SIZE);
    memory_begin = reinterpret_cast<uintptr_t>(memory_chunk.get());
    memory_end = memory_begin + total_size;
    create_arenas();
}

HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::~HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList() {
    destroy_arenas();
}

MemID HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::allocate(size_t p_size, size_t p_alignment) {
    return allocate_in_arena(p_size, p_alignment, get_thread_arena_index());
}

MemID HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::allocate_in_arena(size_t p_size, size_t p_alignment, size_t p_arena_index) {
    if (p_arena_index >= arena_count) {
        throw std::runtime_error(std::format("Arena index {} exceeds arena count {}.", p_arena_index, arena_count));
    }
    return get_arena(p_arena_index).allocate(p_size, p_alignment);
}

void HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::deallocate(MemID p_mem) {
    if (p_mem == MEM_NULL) {
        return;
    }
    get_arena(get_arena_index(p_mem)).deallocate(p_mem);
}

bool HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::is_empty() const {
    for (size_t i = 0; i < arena_count; ++i) {
        if (!get_arena(i).is_empty()) {
            return false;
        }
    }
    return true;
}

void HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::clear() {
    for (size_t i = 0; i < arena_count; ++i) {
        get_arena(i).clear();
    }
}

size_t HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::get_allocated_data_size(MemID p_mem_id) const {
    return get_arena(get_arena_index(p_mem_id)).get_allocated_data_size(p_mem_id);
}

size_t HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::get_remain_size() const {
    size_t remain_size = 0;
    for (size_t i = 0; i < arena_count; ++i) {
        remain_size += get_arena(i).get_remain_size();
    }
    return remain_size;
}

bool HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::is_in_pool(MemID p_mem_id) const {
    if (!is_in_memory_chunk(p_mem_id)) {
        return false;
    }
    return get_arena(get_arena_index(p_mem_id)).is_in_pool(p_mem_id);
}

void HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::check_broken() const {
    for (size_t i = 0; i < arena_count; ++i) {
        get_arena(i).check_broken();
    }
}

HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::operator std::string() const {
    std::stringstream ss;
    ss << "{";
    ss << R"("type":"HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList",)";
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

void HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::create_arenas() {
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

void HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::destroy_arenas() {
    if (arena_storage == nullptr) {
        return;
    }
    for (size_t i = arena_count; i > 0; --i) {
        get_arena(i - 1).~ArenaAllocator();
    }
    ::operator delete(arena_storage, std::align_val_t(alignof(ArenaAllocator)));
    arena_storage = nullptr;
}

HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::ArenaAllocator&
HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::get_arena(size_t p_arena_index) {
    return *reinterpret_cast<ArenaAllocator*>(arena_storage + p_arena_index * sizeof(ArenaAllocator));
}

const HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::ArenaAllocator&
HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::get_arena(size_t p_arena_index) const {
    return *reinterpret_cast<const ArenaAllocator*>(arena_storage + p_arena_index * sizeof(ArenaAllocator));
}

size_t HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::get_thread_arena_index() {
    thread_local const HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList* thread_allocator = nullptr;
    thread_local size_t thread_arena_index = 0;
    thread_local size_t thread_arena_count = 0;

    if (thread_allocator != this || thread_arena_count != arena_count) {
        thread_allocator = this;
        thread_arena_count = arena_count;
        thread_arena_index = next_thread_arena_index.fetch_add(1, std::memory_order_relaxed) % arena_count;
    }
    return thread_arena_index;
}

bool HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::is_in_memory_chunk(MemID p_mem_id) const {
    if (p_mem_id == MEM_NULL) {
        return false;
    }
    uintptr_t memory_id = static_cast<uintptr_t>(p_mem_id);
    return memory_id >= memory_begin && memory_id < memory_end;
}

size_t HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList::get_arena_index(MemID p_mem_id) const {
    if (!is_in_memory_chunk(p_mem_id)) {
        throw std::runtime_error("Memory id does not belong to this thread local arena allocator.");
    }
    size_t arena_index = (static_cast<uintptr_t>(p_mem_id) - memory_begin) / arena_size;
    if (arena_index >= arena_count) {
        throw std::runtime_error("Memory id does not belong to a valid arena.");
    }
    return arena_index;
}

} // namespace WhiteBirdEngine
