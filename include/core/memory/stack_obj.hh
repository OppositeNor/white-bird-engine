// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_STACK_OBJ_HH__
#define __WBE_STACK_OBJ_HH__

#include "core/allocator/stack_allocator.hh"
#include "utils/defs.hh"

namespace WhiteBirdEngine {

template <typename T>
class StackObj {
public:
    ~StackObj() {
        pop_stack_obj<T>(allocator);
    }
    StackObj(const StackObj& p_other)
        : allocator(p_other.allocator) {
        WBE_DEBUG_ASSERT(p_other.allocator != nullptr);
        mem_id = create_stack_obj<T>(allocator, p_other);
    }
    StackObj(StackObj&& p_other)
        : allocator(p_other.allocator) {
        WBE_DEBUG_ASSERT(p_other.allocator != nullptr);
        mem_id = create_stack_obj<T>(allocator, std::move(p_other));
    }
    StackObj& operator=(const StackObj& p_other) {
        *(allocator->get_obj<T>(mem_id)) = *(p_other.allocator->get_obj<T>(p_other.mem_id));
        return *this;
    }
    StackObj& operator=(StackObj&& p_other) {
        *(allocator->get_obj<T>(mem_id)) = std::move(*(p_other.allocator->get_obj<T>(p_other.mem_id)));
        return *this;
    }

    template <typename... Args>
    StackObj(StackAllocator* p_allocator, Args&&... p_args)
        : allocator(p_allocator) {
        WBE_DEBUG_ASSERT(p_allocator != nullptr);
        mem_id = create_stack_obj<T>(allocator, std::forward<Args>(p_args)...);
    }

    // TODO


private:
    StackAllocator* allocator;
    MemID mem_id;
};

}

#endif
