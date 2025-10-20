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
#ifndef __WBE_ALLOCATOR_BENCHMARK_HH__
#define __WBE_ALLOCATOR_BENCHMARK_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/allocator/heap_allocator_aligned_pool_impl_list.hh"
#include <benchmark/benchmark.h>

namespace WhiteBirdEngine {

void malloc_free_benchmark(benchmark::State& p_state) {
    for (auto _ : p_state) {
        int* mem = static_cast<int*>(malloc(1000 * sizeof(int)));
        for (int i = 0; i < 10; ++i) {
            // trigger page fault to allocate memory.
            mem[i] = i;
        }
        free(mem);
    }
}
BENCHMARK(malloc_free_benchmark);

void heap_allocated_aligned_pool_benchmark(benchmark::State& p_state) {
    HeapAllocatorAlignedPool pool;
    for (auto _ : p_state) {
        MemID mem_id = pool.allocate(1000 * sizeof(int));
        int* mem = static_cast<int*>(pool.get(mem_id));
        for (int i = 0; i < 10; ++i) {
            mem[i] = i;
        }
        pool.deallocate(mem_id);
    }
}
BENCHMARK(heap_allocated_aligned_pool_benchmark);

void heap_allocated_aligned_pool_impl_list_benchmark(benchmark::State& p_state) {
    HeapAllocatorAlignedPoolImplicitList pool;
    for (auto _ : p_state) {
        MemID mem_id = pool.allocate(10 * sizeof(int));
        int* mem = static_cast<int*>(pool.get(mem_id));
        for (int i = 0; i < 10; ++i) {
            mem[i] = i;
        }
        pool.deallocate(mem_id);
    }
}
BENCHMARK(heap_allocated_aligned_pool_impl_list_benchmark);

BENCHMARK_MAIN();

}

#endif
