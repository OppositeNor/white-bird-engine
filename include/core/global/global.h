#ifndef __WBE_GLOBAL_H__
#define __WBE_GLOBAL_H__

#include "core/allocator/heap_allocator_pool.h"
#include "core/allocator/stack_allocator.h"

namespace WhiteBirdEngine {
class Global {
public:



    // global objects
    static HeapAllocatorPool* pool_allocator;
    static StackAllocator* single_frame_allocator;


};

}


#endif
