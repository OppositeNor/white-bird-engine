#ifndef __WBE_STACK_ALLOCATOR_H__
#define __WBE_STACK_ALLOCATOR_H__

#include "utils/utils.h"
#include <cstddef>
#include <memory>
#include <stddef.h>

namespace WhiteBirdEngine {

// TODO: Test
class StackAllocator {
public:
    StackAllocator() : StackAllocator(1024) {}
    ~StackAllocator() {}
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator(StackAllocator&&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;
    StackAllocator& operator=(StackAllocator&&) = delete;

    StackAllocator(size_t p_size, size_t p_alignment = alignof(std::max_align_t))
        : total_size(get_align_size(p_size, p_alignment)), alignment(p_alignment), marker(0),
        mem_chunk(std::make_unique<std::byte[]>(total_size)) {}

    void* allocate(size_t p_size) {
        void* result = mem_chunk.get() + marker;
        marker += get_align_size(p_size, alignment);
        return result;
    }

    void pop_stack(size_t p_size) {
        marker -= p_size;
    }

    size_t get_alloc_size() const {
        return marker;
    }

    size_t get_total_size() const {
        return total_size;
    }

    size_t get_alignment() const {
        return alignment;
    }

private:
    size_t total_size;
    size_t alignment;
    size_t marker;
    std::unique_ptr<std::byte[]> mem_chunk;
};

}

#endif
