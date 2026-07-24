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
#ifndef WBE_FILE_MEMORY_CHUNK_HH
#define WBE_FILE_MEMORY_CHUNK_HH

#include "utils/defs.hh"
#include <atomic>
#include <cstddef>

namespace WhiteBirdEngine {

/**
 * @class MemoryChunk
 * @brief Reference-counted handle to a continuous heap memory chunk.
 */
class MemoryChunk {
public:
    /**
     * @brief Construct an empty memory chunk handle.
     */
    MemoryChunk() = default;

    /**
     * @brief Allocate a memory chunk.
     *
     * @param p_size The usable size of the memory chunk.
     * @param p_alignment The allocation alignment.
     */
    explicit MemoryChunk(size_t p_size, size_t p_alignment = WBE_DEFAULT_ALIGNMENT);

    /**
     * @brief Copy and retain a memory chunk reference.
     */
    MemoryChunk(const MemoryChunk& p_other);

    /**
     * @brief Move a memory chunk reference.
     */
    MemoryChunk(MemoryChunk&& p_other) noexcept;

    /**
     * @brief Release a memory chunk reference.
     */
    ~MemoryChunk();

    /**
     * @brief Copy-assign and retain a memory chunk reference.
     */
    MemoryChunk& operator=(const MemoryChunk& p_other);

    /**
     * @brief Move-assign a memory chunk reference.
     */
    MemoryChunk& operator=(MemoryChunk&& p_other) noexcept;

    /**
     * @brief Get the beginning of this memory chunk.
     *
     * @return The beginning of this memory chunk.
     */
    char* get() const {
        return memory;
    }

    /**
     * @brief Get a validated occupied range start inside this memory chunk.
     *
     * @param p_start_offset The offset from the memory chunk start.
     * @param p_size The occupied range size.
     * @return The beginning of the occupied range.
     */
    char* get_occupied_start(size_t p_start_offset, size_t p_size) const;

    /**
     * @brief Get the usable size of this memory chunk.
     *
     * @return The usable size of this memory chunk.
     */
    size_t get_size() const {
        return memory_size;
    }

    /**
     * @brief Get the current reference count.
     *
     * @return The current reference count.
     */
    size_t get_reference_count() const {
        if (reference_counter == nullptr) {
            return 0;
        }
        return reference_counter->load(std::memory_order_acquire);
    }

    /**
     * @brief Check if this memory chunk references allocated memory.
     *
     * @return True if this memory chunk references allocated memory, false otherwise.
     */
    bool is_valid() const {
        return memory != nullptr;
    }

private:
    char* memory = nullptr;
    size_t memory_size = 0;
    std::atomic_size_t* reference_counter = nullptr;

    void retain() const;

    void release();
};

} // namespace WhiteBirdEngine

#endif