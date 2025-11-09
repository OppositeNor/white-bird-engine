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
#include "global/global.hh"
#include "utils/utils.hh"
#include <benchmark/benchmark.h>
#include <cstddef>
#include <utility>
#include <vector>
#include <ranges>

namespace WBE = WhiteBirdEngine;

constexpr size_t ALLOC_NUM = 1000;
constexpr size_t POOL_SIZE = WBE_MiB(100);
constexpr size_t FREE_BATCH = 10000;

void malloc_free_benchmark_with_shuffle(benchmark::State& p_state) {
    std::vector<int*> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        int* result = (int*)malloc(ALLOC_NUM*sizeof(int));
        for (int i = 0; i < ALLOC_NUM; ++i) {
            result[i] = i;
        }
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (3 + FREE_BATCH)) {
                size_t n = allocated.size();
                for (int k = 0; k < n; ++k) {
                    std::swap(allocated[WBE::dynam_hash(counter + k) % FREE_BATCH],
                              allocated[WBE::dynam_hash(counter + n + k) % FREE_BATCH]);
                }
            }
            if (counter % (2 * FREE_BATCH)) {
                for (auto queued_free : allocated) {
                    free(queued_free);
                }
            }
            else {
                for (auto queued_free : allocated | std::views::reverse) {
                    free(queued_free);
                }
            }
            allocated.clear();
        }
    }
    for (auto queued_free : allocated) {
        free(queued_free);
    }
}
BENCHMARK(malloc_free_benchmark_with_shuffle);

void heap_allocated_aligned_pool_benchmark_with_shuffle(benchmark::State& p_state) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(POOL_SIZE);
    std::vector<WBE::MemID> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        WBE::MemID result = pool.allocate(ALLOC_NUM*sizeof(int));
        int* result_ptr = static_cast<int*>(pool.get(result));
        for (int i = 0; i < ALLOC_NUM; ++i) {
            result_ptr[i] = i;
        }
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (3 + FREE_BATCH)) {
                size_t n = allocated.size();
                for (int k = 0; k < n; ++k) {
                    std::swap(allocated[WBE::dynam_hash(counter + k) % FREE_BATCH],
                              allocated[WBE::dynam_hash(counter + n + k) % FREE_BATCH]);
                }
            }
            if (counter % (2 * FREE_BATCH)) {
                for (auto queued_free : allocated) {
                    pool.deallocate(queued_free);
                }
            }
            else {
                for (auto queued_free : allocated | std::views::reverse) {
                    pool.deallocate(queued_free);
                }
            }
            allocated.clear();
        }
    }
    for (auto queued_free : allocated) {
        pool.deallocate(queued_free);
    }
}
BENCHMARK(heap_allocated_aligned_pool_benchmark_with_shuffle);

void heap_allocated_aligned_pool_impl_list_benchmark_with_shuffle(benchmark::State& p_state) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPoolImplicitList pool(POOL_SIZE);
    std::vector<WBE::MemID> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        WBE::MemID result = pool.allocate(ALLOC_NUM*sizeof(int));
        int* result_ptr = static_cast<int*>(pool.get(result));
        for (int i = 0; i < ALLOC_NUM; ++i) {
            result_ptr[i] = i;
        }
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (3 + FREE_BATCH)) {
                size_t n = allocated.size();
                for (int k = 0; k < n; ++k) {
                    std::swap(allocated[WBE::dynam_hash(counter + k) % FREE_BATCH],
                              allocated[WBE::dynam_hash(counter + n + k) % FREE_BATCH]);
                }
            }
            if (counter % (2 * FREE_BATCH)) {
                for (auto queued_free : allocated) {
                    pool.deallocate(queued_free);
                }
            }
            else {
                for (auto queued_free : allocated | std::views::reverse) {
                    pool.deallocate(queued_free);
                }
            }
            allocated.clear();
        }
    }
    for (auto queued_free : allocated) {
        pool.deallocate(queued_free);
    }
}
BENCHMARK(heap_allocated_aligned_pool_impl_list_benchmark_with_shuffle);

void malloc_free_benchmark_without_shuffle(benchmark::State& p_state) {
    std::vector<int*> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        int* result = (int*)malloc(ALLOC_NUM*sizeof(int));
        for (int i = 0; i < ALLOC_NUM; ++i) {
            result[i] = i;
        }
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (2 * FREE_BATCH)) {
                for (auto queued_free : allocated) {
                    free(queued_free);
                }
            }
            else {
                for (auto queued_free : allocated | std::views::reverse) {
                    free(queued_free);
                }
            }
            allocated.clear();
        }
    }
    for (auto queued_free : allocated) {
        free(queued_free);
    }
}
BENCHMARK(malloc_free_benchmark_without_shuffle);

void heap_allocated_aligned_pool_benchmark_without_shuffle(benchmark::State& p_state) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(POOL_SIZE);
    std::vector<WBE::MemID> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        WBE::MemID result = pool.allocate(ALLOC_NUM*sizeof(int));
        int* result_ptr = static_cast<int*>(pool.get(result));
        for (int i = 0; i < ALLOC_NUM; ++i) {
            result_ptr[i] = i;
        }
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (2 * FREE_BATCH)) {
                for (auto queued_free : allocated) {
                    pool.deallocate(queued_free);
                }
            }
            else {
                for (auto queued_free : allocated | std::views::reverse) {
                    pool.deallocate(queued_free);
                }
            }
            allocated.clear();
        }
    }
    for (auto queued_free : allocated) {
        pool.deallocate(queued_free);
    }
}
BENCHMARK(heap_allocated_aligned_pool_benchmark_without_shuffle);

void heap_allocated_aligned_pool_impl_list_benchmark_without_shuffle(benchmark::State& p_state) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPoolImplicitList pool(POOL_SIZE);
    std::vector<WBE::MemID> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        WBE::MemID result = pool.allocate(ALLOC_NUM*sizeof(int));
        int* result_ptr = static_cast<int*>(pool.get(result));
        for (int i = 0; i < ALLOC_NUM; ++i) {
            result_ptr[i] = i;
        }
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (2 * FREE_BATCH)) {
                for (auto queued_free : allocated) {
                    pool.deallocate(queued_free);
                }
            }
            else {
                for (auto queued_free : allocated | std::views::reverse) {
                    pool.deallocate(queued_free);
                }
            }
            allocated.clear();
        }
    }
    for (auto queued_free : allocated) {
        pool.deallocate(queued_free);
    }
}
BENCHMARK(heap_allocated_aligned_pool_impl_list_benchmark_without_shuffle);
BENCHMARK_MAIN();


#endif
