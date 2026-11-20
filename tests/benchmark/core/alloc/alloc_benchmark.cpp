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
#include "core/alloc/heap_alloc_aligned_pool.hh"
#include "core/alloc/heap_alloc_aligned_pool_impl_list.hh"
#include "core/alloc/heap_alloc_atomic_arena_aligned_pool_impl_list.hh"
#include "core/alloc/heap_alloc_atomic_mutex_aligned_pool_impl_list.hh"
#include "core/alloc/heap_alloc_atomic_shared_mutex_aligned_pool.hh"
#include "core/alloc/heap_alloc_atomic_shared_mutex_aligned_pool_impl_list.hh"
#include "core/alloc/heap_alloc_atomic_size_classified.hh"
// #include "core/alloc/heap_alloc_atomic_thread_local_arena_aligned_pool_impl_list.hh"
#include "core/alloc/i_alloc.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <benchmark/benchmark.h>
#include <cstddef>
#include <ranges>
#include <utility>
#include <vector>

namespace WBE = WhiteBirdEngine;

constexpr size_t ALLOC_NUM = 1000;
constexpr size_t POOL_SIZE = WBE_MI_B(128);
constexpr size_t FREE_BATCH = 10000;

// Construct a pool whose allocations of ALLOC_NUM ints get p_pool_size bytes.
template <typename Alloc>
static Alloc make_pool(size_t p_pool_size) {
    return Alloc(p_pool_size);
}

// The benchmark allocations fall in the medium class, which gets the whole pool size; the other classes stay minimal.
template <>
WBE::HeapAllocAtomicSizeClassified make_pool<WBE::HeapAllocAtomicSizeClassified>(size_t p_pool_size) {
    static_assert(ALLOC_NUM * sizeof(int) > WBE::HeapAllocAtomicSizeClassified::DEFAULT_SMALL_MAX_ALLOCATION_SIZE);
    static_assert(ALLOC_NUM * sizeof(int) <= WBE::HeapAllocAtomicSizeClassified::DEFAULT_MEDIUM_MAX_ALLOCATION_SIZE);
    return WBE::HeapAllocAtomicSizeClassified(
        WBE::HeapAllocAtomicSizeClassified::make_default_size_classes(WBE_MI_B(1), p_pool_size, WBE_MI_B(1)));
}

template <typename Alloc>
static void write_allocation(Alloc& p_pool, WBE::MemID p_mem) {
    int* result_ptr = static_cast<int*>(p_pool.get(p_mem));
    for (size_t i = 0; i < ALLOC_NUM; ++i) {
        result_ptr[i] = static_cast<int>(i);
    }
}

template <typename Allocated>
static void shuffle_allocated(std::vector<Allocated>& p_allocated, size_t p_counter) {
    size_t n = p_allocated.size();
    for (size_t k = 0; k < n; ++k) {
        std::swap(p_allocated[WBE::dynam_hash(p_counter + k) % FREE_BATCH], p_allocated[WBE::dynam_hash(p_counter + n + k) % FREE_BATCH]);
    }
}

template <typename Alloc>
static void deallocate_allocated(Alloc& p_pool, std::vector<WBE::MemID>& p_allocated, size_t p_counter) {
    if (p_counter % (2 * FREE_BATCH)) {
        for (auto queued_free : p_allocated) {
            p_pool.deallocate(queued_free);
        }
    } else {
        for (auto queued_free : p_allocated | std::views::reverse) {
            p_pool.deallocate(queued_free);
        }
    }
    p_allocated.clear();
}

template <typename Alloc, bool ShuffleEnabled>
static void run_heap_allocated_pool_benchmark(benchmark::State& p_state, size_t p_pool_size) {
    Alloc pool = make_pool<Alloc>(p_pool_size);
    std::vector<WBE::MemID> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        WBE::MemID result = pool.allocate(ALLOC_NUM * sizeof(int));
        write_allocation(pool, result);
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if constexpr (ShuffleEnabled) {
                if (counter % (3 + FREE_BATCH)) {
                    shuffle_allocated(allocated, counter);
                }
            }
            deallocate_allocated(pool, allocated, counter);
        }
    }
    for (auto queued_free : allocated) {
        pool.deallocate(queued_free);
    }
}

template <bool ShuffleEnabled>
static void run_heap_allocated_arena_pool_benchmark(benchmark::State& p_state, size_t p_pool_size, size_t p_arena_count) {
    WBE::HeapAllocAtomicArenaAlignedPoolImplicitList pool(p_pool_size, p_arena_count);
    std::vector<WBE::MemID> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        WBE::MemID result = pool.allocate(ALLOC_NUM * sizeof(int));
        write_allocation(pool, result);
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if constexpr (ShuffleEnabled) {
                if (counter % (3 + FREE_BATCH)) {
                    shuffle_allocated(allocated, counter);
                }
            }
            deallocate_allocated(pool, allocated, counter);
        }
    }
    for (auto queued_free : allocated) {
        pool.deallocate(queued_free);
    }
}

template <typename Alloc>
static void run_async_heap_allocated_pool_benchmark(benchmark::State& p_state) {
    static Alloc pool = make_pool<Alloc>(POOL_SIZE * 8);
    std::vector<WBE::MemID> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        WBE::MemID result = pool.allocate(ALLOC_NUM * sizeof(int));
        write_allocation(pool, result);
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (3 + FREE_BATCH)) {
                shuffle_allocated(allocated, counter);
            }
            deallocate_allocated(pool, allocated, counter);
        }
    }
    for (auto queued_free : allocated) {
        pool.deallocate(queued_free);
    }
}

template <size_t ArenaCount>
static void run_async_heap_allocated_arena_pool_benchmark(benchmark::State& p_state) {
    static WBE::HeapAllocAtomicArenaAlignedPoolImplicitList pool(POOL_SIZE * 8, ArenaCount);
    std::vector<WBE::MemID> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        WBE::MemID result = pool.allocate(ALLOC_NUM * sizeof(int));
        write_allocation(pool, result);
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (3 + FREE_BATCH)) {
                shuffle_allocated(allocated, counter);
            }
            deallocate_allocated(pool, allocated, counter);
        }
    }
    for (auto queued_free : allocated) {
        pool.deallocate(queued_free);
    }
}

static void malloc_free_benchmark_with_shuffle(benchmark::State& p_state) {
    std::vector<int*> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        int* result = static_cast<int*>(malloc(ALLOC_NUM * sizeof(int))); // NOLINT
        for (size_t i = 0; i < ALLOC_NUM; ++i) {
            result[i] = static_cast<int>(i);
        }
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (3 + FREE_BATCH)) {
                size_t n = allocated.size();
                for (size_t k = 0; k < n; ++k) {
                    std::swap(allocated[WBE::dynam_hash(counter + k) % FREE_BATCH],
                        allocated[WBE::dynam_hash(counter + n + k) % FREE_BATCH]);
                }
            }
            if (counter % (2 * FREE_BATCH)) {
                for (auto* queued_free : allocated) {
                    free(queued_free); // NOLINT
                }
            } else {
                for (auto* queued_free : allocated | std::views::reverse) {
                    free(queued_free); // NOLINT
                }
            }
            allocated.clear();
        }
    }
    for (auto* queued_free : allocated) {
        free(queued_free); // NOLINT
    }
}
BENCHMARK(malloc_free_benchmark_with_shuffle);

static void heap_allocated_aligned_pool_benchmark_with_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAlignedPool, true>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_aligned_pool_benchmark_with_shuffle);

static void heap_allocated_aligned_pool_impl_list_benchmark_with_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAlignedPoolImplicitList, true>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_aligned_pool_impl_list_benchmark_with_shuffle);

static void malloc_free_benchmark_without_shuffle(benchmark::State& p_state) {
    std::vector<int*> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        int* result = static_cast<int*>(malloc(ALLOC_NUM * sizeof(int))); // NOLINT
        for (size_t i = 0; i < ALLOC_NUM; ++i) {
            result[i] = static_cast<int>(i);
        }
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (2 * FREE_BATCH)) {
                for (auto* queued_free : allocated) {
                    free(queued_free); // NOLINT
                }
            } else {
                for (auto* queued_free : allocated | std::views::reverse) {
                    free(queued_free); // NOLINT
                }
            }
            allocated.clear();
        }
    }
    for (auto* queued_free : allocated) {
        free(queued_free); // NOLINT
    }
}
BENCHMARK(malloc_free_benchmark_without_shuffle);

static void heap_allocated_aligned_pool_benchmark_without_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAlignedPool, false>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_aligned_pool_benchmark_without_shuffle);

static void heap_allocated_aligned_pool_impl_list_benchmark_without_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAlignedPoolImplicitList, false>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_aligned_pool_impl_list_benchmark_without_shuffle);

static void heap_allocated_atomic_shared_mutex_aligned_pool_benchmark_with_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicSharedMutexAlignedPool, true>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_atomic_shared_mutex_aligned_pool_benchmark_with_shuffle);

static void heap_allocated_atomic_shared_mutex_aligned_pool_impl_list_benchmark_with_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicSharedMutexAlignedPoolImplicitList, true>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_atomic_shared_mutex_aligned_pool_impl_list_benchmark_with_shuffle);

static void heap_allocated_atomic_mutex_aligned_pool_impl_list_benchmark_with_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicMutexAlignedPoolImplicitList, true>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_atomic_mutex_aligned_pool_impl_list_benchmark_with_shuffle);

static void heap_allocated_atomic_arena_aligned_pool_impl_list_benchmark_with_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicArenaAlignedPoolImplicitList, true>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_atomic_arena_aligned_pool_impl_list_benchmark_with_shuffle);

static void heap_allocated_atomic_arena_aligned_pool_impl_list_2_arenas_benchmark_with_shuffle(benchmark::State& p_state) {
    run_heap_allocated_arena_pool_benchmark<true>(p_state, POOL_SIZE, 2);
}
BENCHMARK(heap_allocated_atomic_arena_aligned_pool_impl_list_2_arenas_benchmark_with_shuffle);

static void heap_allocated_atomic_arena_aligned_pool_impl_list_4_arenas_benchmark_with_shuffle(benchmark::State& p_state) {
    run_heap_allocated_arena_pool_benchmark<true>(p_state, POOL_SIZE, 4);
}
BENCHMARK(heap_allocated_atomic_arena_aligned_pool_impl_list_4_arenas_benchmark_with_shuffle);

static void heap_allocated_atomic_size_classified_benchmark_with_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicSizeClassified, true>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_atomic_size_classified_benchmark_with_shuffle);

// static void heap_allocated_atomic_thread_local_arena_aligned_pool_impl_list_benchmark_with_shuffle(benchmark::State& p_state) {
//    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicThreadLocalArenaAlignedPoolImplicitList, true>(p_state, POOL_SIZE);
// }
// BENCHMARK(heap_allocated_atomic_thread_local_arena_aligned_pool_impl_list_benchmark_with_shuffle);

static void heap_allocated_atomic_shared_mutex_aligned_pool_benchmark_without_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicSharedMutexAlignedPool, false>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_atomic_shared_mutex_aligned_pool_benchmark_without_shuffle);

static void heap_allocated_atomic_shared_mutex_aligned_pool_impl_list_benchmark_without_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicSharedMutexAlignedPoolImplicitList, false>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_atomic_shared_mutex_aligned_pool_impl_list_benchmark_without_shuffle);

static void heap_allocated_atomic_mutex_aligned_pool_impl_list_benchmark_without_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicMutexAlignedPoolImplicitList, false>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_atomic_mutex_aligned_pool_impl_list_benchmark_without_shuffle);

static void heap_allocated_atomic_arena_aligned_pool_impl_list_benchmark_without_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicArenaAlignedPoolImplicitList, false>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_atomic_arena_aligned_pool_impl_list_benchmark_without_shuffle);

static void heap_allocated_atomic_arena_aligned_pool_impl_list_2_arenas_benchmark_without_shuffle(benchmark::State& p_state) {
    run_heap_allocated_arena_pool_benchmark<false>(p_state, POOL_SIZE, 2);
}
BENCHMARK(heap_allocated_atomic_arena_aligned_pool_impl_list_2_arenas_benchmark_without_shuffle);

static void heap_allocated_atomic_arena_aligned_pool_impl_list_4_arenas_benchmark_without_shuffle(benchmark::State& p_state) {
    run_heap_allocated_arena_pool_benchmark<false>(p_state, POOL_SIZE, 4);
}
BENCHMARK(heap_allocated_atomic_arena_aligned_pool_impl_list_4_arenas_benchmark_without_shuffle);

static void heap_allocated_atomic_size_classified_benchmark_without_shuffle(benchmark::State& p_state) {
    run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicSizeClassified, false>(p_state, POOL_SIZE);
}
BENCHMARK(heap_allocated_atomic_size_classified_benchmark_without_shuffle);

// static void heap_allocated_atomic_thread_local_arena_aligned_pool_impl_list_benchmark_without_shuffle(benchmark::State& p_state) {
//     run_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicThreadLocalArenaAlignedPoolImplicitList, false>(p_state, POOL_SIZE);
// }
// BENCHMARK(heap_allocated_atomic_thread_local_arena_aligned_pool_impl_list_benchmark_without_shuffle);

static void async_malloc_free_benchmark(benchmark::State& p_state) {
    std::vector<int*> allocated;
    allocated.reserve(FREE_BATCH);
    size_t counter = 0;
    for (auto _ : p_state) {
        ++counter;
        int* result = static_cast<int*>(malloc(ALLOC_NUM * sizeof(int))); // NOLINT
        for (size_t i = 0; i < ALLOC_NUM; ++i) {
            result[i] = static_cast<int>(i);
        }
        allocated.push_back(result);
        if (counter % FREE_BATCH == 0) {
            if (counter % (3 + FREE_BATCH)) {
                size_t n = allocated.size();
                for (size_t k = 0; k < n; ++k) {
                    std::swap(allocated[WBE::dynam_hash(counter + k) % FREE_BATCH],
                        allocated[WBE::dynam_hash(counter + n + k) % FREE_BATCH]);
                }
            }
            if (counter % (2 * FREE_BATCH)) {
                for (auto* queued_free : allocated) {
                    free(queued_free); // NOLINT
                }
            } else {
                for (auto* queued_free : allocated | std::views::reverse) {
                    free(queued_free); // NOLINT
                }
            }
            allocated.clear();
        }
    }
    for (auto* queued_free : allocated) {
        free(queued_free); // NOLINT
    }
}
BENCHMARK(async_malloc_free_benchmark)->Threads(2)->Threads(4)->Threads(8);

static void async_heap_allocated_atomic_shared_mutex_aligned_pool_benchmark(benchmark::State& p_state) {
    run_async_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicSharedMutexAlignedPool>(p_state);
}
BENCHMARK(async_heap_allocated_atomic_shared_mutex_aligned_pool_benchmark)->Threads(2)->Threads(4)->Threads(8);

static void async_heap_allocated_atomic_shared_mutex_aligned_pool_impl_list_benchmark(benchmark::State& p_state) {
    run_async_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicSharedMutexAlignedPoolImplicitList>(p_state);
}
BENCHMARK(async_heap_allocated_atomic_shared_mutex_aligned_pool_impl_list_benchmark)->Threads(2)->Threads(4)->Threads(8);

static void async_heap_allocated_atomic_mutex_aligned_pool_impl_list_benchmark(benchmark::State& p_state) {
    run_async_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicMutexAlignedPoolImplicitList>(p_state);
}
BENCHMARK(async_heap_allocated_atomic_mutex_aligned_pool_impl_list_benchmark)->Threads(2)->Threads(4)->Threads(8);

static void async_heap_allocated_atomic_arena_aligned_pool_impl_list_benchmark(benchmark::State& p_state) {
    run_async_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicArenaAlignedPoolImplicitList>(p_state);
}
BENCHMARK(async_heap_allocated_atomic_arena_aligned_pool_impl_list_benchmark)->Threads(2)->Threads(4)->Threads(8);

static void async_heap_allocated_atomic_arena_aligned_pool_impl_list_2_arenas_benchmark(benchmark::State& p_state) {
    run_async_heap_allocated_arena_pool_benchmark<2>(p_state);
}
BENCHMARK(async_heap_allocated_atomic_arena_aligned_pool_impl_list_2_arenas_benchmark)->Threads(2)->Threads(4)->Threads(8);

static void async_heap_allocated_atomic_arena_aligned_pool_impl_list_4_arenas_benchmark(benchmark::State& p_state) {
    run_async_heap_allocated_arena_pool_benchmark<4>(p_state);
}
BENCHMARK(async_heap_allocated_atomic_arena_aligned_pool_impl_list_4_arenas_benchmark)->Threads(2)->Threads(4)->Threads(8);

static void async_heap_allocated_atomic_size_classified_benchmark(benchmark::State& p_state) {
    run_async_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicSizeClassified>(p_state);
}
BENCHMARK(async_heap_allocated_atomic_size_classified_benchmark)->Threads(2)->Threads(4)->Threads(8);

// static void async_heap_allocated_atomic_thread_local_arena_aligned_pool_impl_list_benchmark(benchmark::State& p_state) {
//     run_async_heap_allocated_pool_benchmark<WBE::HeapAllocAtomicThreadLocalArenaAlignedPoolImplicitList>(p_state);
// }
// BENCHMARK(async_heap_allocated_atomic_thread_local_arena_aligned_pool_impl_list_benchmark)->Threads(2)->Threads(4)->Threads(8);

BENCHMARK_MAIN();
