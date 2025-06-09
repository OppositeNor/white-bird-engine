#ifndef __WBE_GLOBAL_H__
#define __WBE_GLOBAL_H__

#include "core/allocator/heap_allocator_pool.h"
#include "core/allocator/stack_allocator.h"
#include "core/engine_config/engine_config.h"

namespace WhiteBirdEngine {
class Global {
public:
    ~Global();


    // global objects
    inline static std::unique_ptr<EngineConfig> engine_config;
    inline static std::unique_ptr<StackAllocator> single_frame_allocator;
    inline static std::unique_ptr<HeapAllocatorPool> pool_allocator;


};

}


#endif
