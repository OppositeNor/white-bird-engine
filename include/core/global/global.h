#ifndef __WBE_GLOBAL_H__
#define __WBE_GLOBAL_H__

#include "core/allocator/heap_allocator_pool.h"
#include "core/allocator/stack_allocator.h"
#include "core/engine_config/engine_config.h"

namespace WhiteBirdEngine {
class Global {
public:



    // global objects
    static EngineConfig* engine_config;
    static HeapAllocatorPool* pool_allocator;
    static StackAllocator* single_frame_allocator;


};

}


#endif
