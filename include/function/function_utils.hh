// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_FUNTION_UTILS_HH__
#define __WBE_FUNTION_UTILS_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "function/engine_function.hh"
namespace WhiteBirdEngine {

inline HeapAllocatorAlignedPool* get_render_allocator() {
    return EngineFunction::get_singleton()->renderer_allocator;
}

}

#endif
