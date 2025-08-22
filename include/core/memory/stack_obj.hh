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
