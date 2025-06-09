#ifndef __WBE_STACK_ALLOCATOR_H__
#define __WBE_STACK_ALLOCATOR_H__

#include "allocator.h"
#include "utils/defs.h"
#include "utils/utils.h"
#include <cstddef>
#include <memory>
#include <stddef.h>
#include <vector>

namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class StackAllocator> {
    WBE_TRAIT(AllocatorTrait<StackAllocator>);
    static constexpr AllocatorType TYPE = AllocatorType::STACK_ALLOCATOR;
    static constexpr bool IS_POOL = true;
    static constexpr bool IS_GURANTEED_CONTINUOUS = true;
    static constexpr bool IS_LIMITED_SIZE = true;
    static constexpr bool IS_ALLOC_FIXED_SIZE = false;
    static constexpr bool IS_ATOMIC = false;

    WBE_TRAIT_REQURIES(AllocatorTraitConcept);
};

// TODO: Test
class StackAllocator {
public:
    StackAllocator() : StackAllocator(1024) {}
    ~StackAllocator() {}
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator(StackAllocator&&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;
    StackAllocator& operator=(StackAllocator&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_size The size of the allocated buffer in bytes.
     * @param p_alignment The memory alighment.
     */
    StackAllocator(size_t p_size, size_t p_alignment = alignof(std::max_align_t))
        : total_size(get_align_size(p_size, p_alignment)), alignment(p_alignment), marker(0),
        mem_chunk(std::make_unique<std::byte[]>(total_size)) {
        size_stack.reserve(64);
    }

    /**
     * @brief Allocate memory with a specified size.
     *
     * @param p_size The size of the memory to be allocated in bytes.
     * @return 
     */
    MemID allocate(size_t p_size) {
        void* result = mem_chunk.get() + marker;
        size_t aligned_size = get_align_size(p_size, alignment);
        marker += aligned_size;
        MemID result_id = reinterpret_cast<MemID>(result);
        size_stack.push_back(result_id);
        return result_id;
    }

    /**
     * @brief Get the pointer pointing to the memory with a specified memory id.
     *
     * @param p_id The ID of the memory to get.
     */
    void* get(MemID p_id) {
        WBE_DEBUG_ASSERT(reinterpret_cast<void*>(p_id) < (mem_chunk.get() + marker));
        return reinterpret_cast<void*>(p_id);
    }

    /**
     * @brief Get the pointer pointing to the memory with a specified memory id.
     *
     * @param p_id The ID of the memory to get.
     */
    const void* get(MemID p_id) const {
        WBE_DEBUG_ASSERT(reinterpret_cast<void*>(p_id) < (mem_chunk.get() + marker));
        return reinterpret_cast<void*>(p_id);
    }

    /**
     * @brief Pop the last element out from the stack.
     */
    void pop_stack() {
        marker -= size_stack.back();
        size_stack.pop_back();
    }

    /**
     * @brief Clear the allocator.
     */
    void clear() {
        marker = 0;
    }

    /**
     * @brief Get the size of the allocator buffer.
     *
     * @return The size of the buffer.
     */
    size_t get_alloc_size() const {
        return marker;
    }

    /**
     * @brief Get the total size of the allocator.
     *
     * @return 
     */
    size_t get_total_size() const {
        return total_size;
    }

    /**
     * @brief Get the alignment of the allocation.
     *
     * @return The alignment of the allocation.
     */
    size_t get_alignment() const {
        return alignment;
    }

private:
    size_t total_size;
    size_t alignment;
    size_t marker;
    std::unique_ptr<std::byte[]> mem_chunk;
    std::vector<size_t> size_stack;
};


}

#endif
