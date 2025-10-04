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
#ifndef __WBE_STACK_ALLOCATOR_HH__
#define __WBE_STACK_ALLOCATOR_HH__

#include "allocator.hh"
#include "core/logging/log.hh"
#include "utils/defs.hh"
#include <bit>
#include <cstddef>
#include <memory>
#include <sstream>
#include <stddef.h>

namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class StackAllocator> {
    WBE_TRAIT(AllocatorTrait<StackAllocator>);
    static constexpr AllocatorType TYPE = AllocatorType::STACK_ALLOCATOR;
    static constexpr bool IS_POOL = true;
    static constexpr bool IS_GURANTEED_CONTINUOUS = true;
    static constexpr bool IS_ALIGNABLE = false;
    static constexpr bool IS_LIMITED_SIZE = true;
    static constexpr bool IS_ALLOC_FIXED_SIZE = false;
    static constexpr bool IS_ATOMIC = false;
    static constexpr bool WILL_ADDR_MOVE = false;

    WBE_TRAIT_REQUIRES(AllocatorTraitConcept);
};

/**
 * @class StackAllocator
 * @brief Stack style allocation. Can only allocate and deallocate from the top of the stack.
 * @todo Test
 */
class StackAllocator {
public:
    StackAllocator() : StackAllocator(1024) {}
    ~StackAllocator() {
        if (stack_pointer != 0) {
            wbe_console_log()->warning("StackAllocator not empty during destruction.");
        }
    }
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator(StackAllocator&&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;
    StackAllocator& operator=(StackAllocator&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_size The size of the allocated buffer in bytes.
     */
    StackAllocator(size_t p_size)
        : total_size(p_size), stack_pointer(0),
        mem_chunk(std::make_unique<char[]>(total_size)) {
    }

    /**
     * @brief Allocate memory with a specified size.
     *
     * @param p_size The size of the memory to be allocated in bytes.
     * @return 
     */
    MemID allocate(size_t p_size) {
        void* result = mem_chunk.get() + stack_pointer;
        stack_pointer += get_align_size(p_size, WBE_DEFAULT_ALIGNMENT);
        return std::bit_cast<MemID>(result);
    }

    /**
     * @brief Get the pointer pointing to the memory with a specified memory id.
     *
     * @param p_id The ID of the memory to get.
     */
    void* get(MemID p_id) {
        WBE_DEBUG_ASSERT(std::bit_cast<void*>(p_id) < (mem_chunk.get() + stack_pointer));
        return std::bit_cast<void*>(p_id);
    }

    /**
     * @brief Get the pointer pointing to the memory with a specified memory id.
     *
     * @param p_id The ID of the memory to get.
     */
    void* get(MemID p_id) const {
        WBE_DEBUG_ASSERT(std::bit_cast<void*>(p_id) < (mem_chunk.get() + stack_pointer));
        return std::bit_cast<void*>(p_id);
    }

    /**
     * @brief Get the pointer pointing to the object with a specified memory id.
     *
     * @tparam T The type of the object.
     * @param p_id The memory ID.
     * @return The pointer to the object.
     */
    template <typename T>
    T* get_obj(MemID p_id) const {
        WBE_DEBUG_ASSERT(std::bit_cast<void*>(p_id) < (mem_chunk.get() + stack_pointer));
        return std::bit_cast<T*>(p_id);
    }

    /**
     * @brief Pop the last element out from the stack.
     *
     * @return The pointer to the deallocated object.
     */
    void* pop_stack(size_t p_size) {
        stack_pointer -= get_align_size(p_size, WBE_DEFAULT_ALIGNMENT);
        return mem_chunk.get() + stack_pointer;
    }

    /**
     * @brief Clear the allocator.
     */
    void clear() {
        stack_pointer = 0;
    }

    /**
     * @brief Get the size of the allocator buffer.
     *
     * @return The size of the buffer.
     */
    size_t get_alloc_size() const {
        return stack_pointer;
    }

    /**
     * @brief Get the total size of the allocator.
     *
     * @return 
     */
    size_t get_total_size() const {
        return total_size;
    }

    operator std::string() const {
        std::stringstream ss;
        ss << "{\"type\":\"StackAllocator\",\"total_size\":" << total_size
           << ",\"stack_pointer\":" << stack_pointer
           << ",\"available\":" << (total_size - stack_pointer) << "}";
        return ss.str();
    }

private:
    size_t total_size;
    size_t stack_pointer;
    std::unique_ptr<char[]> mem_chunk;
};

template <typename T, typename... Args>
MemID create_stack_obj(StackAllocator& p_allocator, Args&&... p_args) {
    MemID result = p_allocator.allocate(sizeof(T));
    new(p_allocator.get(result)) T(std::forward<Args>(p_args)...);
    return result;
}

template <typename T>
void pop_stack_obj(StackAllocator& p_allocator) {
    static_cast<T*>(p_allocator.pop_stack(sizeof(T)))->~T();
}

template <typename T>
void pop_stack_obj_array(StackAllocator& p_allocator, size_t p_num) {
    T* head = static_cast<T*>(p_allocator.get(p_num));
    for (; p_num > 0; --p_num) {
        head->~T();
    }
    p_allocator.pop_stack(p_num * sizeof(T));
}

}

#endif
