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
#include "core/allocator/memory_chunk.hh"
#include "utils/utils.hh"
#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

MemoryChunk::MemoryChunk(size_t p_size, size_t p_alignment) : memory_size(p_size) {
    if (p_alignment == 0) {
        throw std::runtime_error("Memory chunk alignment must not be 0.");
    }
    size_t allocation_size = get_align_size(p_size, p_alignment);
    memory = static_cast<char*>(::aligned_alloc(p_alignment, allocation_size)); // NOLINT
    if (memory == nullptr) {
        throw std::runtime_error("Failed to create memory chunk: allocation failed.");
    }
    try {
        reference_counter = new std::atomic_size_t(1);
    } catch (...) {
        ::free(memory); // NOLINT
        memory = nullptr;
        memory_size = 0;
        throw;
    }
}

MemoryChunk::MemoryChunk(const MemoryChunk& p_other)
    : memory(p_other.memory), memory_size(p_other.memory_size), reference_counter(p_other.reference_counter) {
    retain();
}

MemoryChunk::MemoryChunk(MemoryChunk&& p_other) noexcept
    : memory(std::exchange(p_other.memory, nullptr)), memory_size(std::exchange(p_other.memory_size, 0)),
      reference_counter(std::exchange(p_other.reference_counter, nullptr)) {
}

MemoryChunk::~MemoryChunk() {
    release();
}

MemoryChunk& MemoryChunk::operator=(const MemoryChunk& p_other) {
    if (this != &p_other) {
        p_other.retain();
        release();
        memory = p_other.memory;
        memory_size = p_other.memory_size;
        reference_counter = p_other.reference_counter;
    }
    return *this;
}

MemoryChunk& MemoryChunk::operator=(MemoryChunk&& p_other) noexcept {
    if (this != &p_other) {
        release();
        memory = std::exchange(p_other.memory, nullptr);
        memory_size = std::exchange(p_other.memory_size, 0);
        reference_counter = std::exchange(p_other.reference_counter, nullptr);
    }
    return *this;
}

char* MemoryChunk::get_occupied_start(size_t p_start_offset, size_t p_size) const {
    if (memory == nullptr) {
        throw std::runtime_error("Cannot occupy an empty memory chunk.");
    }
    if (p_start_offset > memory_size || p_size > memory_size - p_start_offset) {
        throw std::runtime_error("Memory chunk occupation range exceeds memory chunk size.");
    }
    return memory + p_start_offset;
}

void MemoryChunk::retain() const {
    if (reference_counter != nullptr) {
        reference_counter->fetch_add(1, std::memory_order_acq_rel);
    }
}

void MemoryChunk::release() {
    if (reference_counter == nullptr) {
        return;
    }
    if (reference_counter->fetch_sub(1, std::memory_order_acq_rel) == 1) {
        ::free(memory); // NOLINT
        delete reference_counter;
    }
    memory = nullptr;
    memory_size = 0;
    reference_counter = nullptr;
}

} // namespace WhiteBirdEngine