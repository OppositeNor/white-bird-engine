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
#ifndef WBE_FILE_HEAP_ALLOC_ATOMIC_SHARED_MUTEX_ALIGNED_POOL_TEST_HH
#define WBE_FILE_HEAP_ALLOC_ATOMIC_SHARED_MUTEX_ALIGNED_POOL_TEST_HH

#include "core/alloc/heap_alloc_atomic_shared_mutex_aligned_pool.hh"
#include "core/alloc/i_alloc.hh"
#include "global/global.hh"
#include "test_global.hh"
#include "utils/defs.hh"
#include <atomic>
#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace WBE = WhiteBirdEngine;
constexpr size_t AAAPT_HEADER_SIZE = WBE::HeapAllocAtomicSharedMutexAlignedPool::HEADER_SIZE;

class WBEAllocAtomicSharedMutexAlignedPoolTest : public ::testing::Test {
protected:
    virtual void SetUp() override {
        global = WBE::make_test_global();
    }

    virtual void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
};

TEST_F(WBEAllocAtomicSharedMutexAlignedPoolTest, TraitTest) {
    ASSERT_TRUE(WBE::AllocTrait<WBE::HeapAllocAtomicSharedMutexAlignedPool>::IS_POOL);
    ASSERT_TRUE(WBE::AllocTrait<WBE::HeapAllocAtomicSharedMutexAlignedPool>::IS_LIMITED_SIZE);
    ASSERT_FALSE(WBE::AllocTrait<WBE::HeapAllocAtomicSharedMutexAlignedPool>::IS_GURANTEED_CONTINUOUS);
    ASSERT_FALSE(WBE::AllocTrait<WBE::HeapAllocAtomicSharedMutexAlignedPool>::IS_ALLOC_FIXED_SIZE);
    ASSERT_TRUE(WBE::AllocTrait<WBE::HeapAllocAtomicSharedMutexAlignedPool>::IS_ATOMIC);
}

TEST_F(WBEAllocAtomicSharedMutexAlignedPoolTest, SynchronizationTest) {
    const size_t pool_size = WBE_MI_B(1);
    WBE::HeapAllocAtomicSharedMutexAlignedPool alloc(pool_size);
    const int num_threads = 8;
    const int allocs_per_thread = 128;
    std::vector<std::thread> threads;
    std::vector<WBE::MemID> mem_ids(num_threads * allocs_per_thread);
    std::atomic<int> alloc_count{0};

    auto alloc_func = [&](int p_thread_idx) {
        for (int i = 0; i < allocs_per_thread; ++i) {
            WBE::MemID mem = alloc.allocate(8);
            ASSERT_NE(mem, WBE::MEM_NULL);
            mem_ids[p_thread_idx * allocs_per_thread + i] = mem;
            ++alloc_count;
        }
    };
    threads.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(alloc_func, t);
    }
    for (auto& th : threads) {
        th.join();
    }
    ASSERT_EQ(alloc_count, num_threads * allocs_per_thread);
    for (auto mem : mem_ids) {
        alloc.deallocate(mem);
    }
    ASSERT_EQ(alloc.get_remain_size(), pool_size);
}

TEST_F(WBEAllocAtomicSharedMutexAlignedPoolTest, SynchronizationGetPointerContentTest) {
    const size_t pool_size = WBE_MI_B(1);
    WBE::HeapAllocAtomicSharedMutexAlignedPool alloc(pool_size);
    const int num_threads = 8;
    const int allocs_per_thread = 128;
    std::vector<std::thread> threads;
    std::vector<WBE::MemID> mem_ids(num_threads * allocs_per_thread);
    std::vector<int> values(num_threads * allocs_per_thread, 0);
    std::atomic<int> success_count{0};

    auto alloc_and_write_func = [&](int p_thread_idx) {
        for (int i = 0; i < allocs_per_thread; ++i) {
            int idx = p_thread_idx * allocs_per_thread + i;
            WBE::MemID mem = alloc.allocate(sizeof(int));
            ASSERT_NE(mem, WBE::MEM_NULL);
            mem_ids[idx] = mem;
            int* ptr = static_cast<int*>(alloc.get(mem));
            ASSERT_NE(ptr, nullptr);
            *ptr = idx * 10;
            values[idx] = *ptr;
            if (*ptr == idx * 10) {
                ++success_count;
            }
        }
    };
    threads.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(alloc_and_write_func, t);
    }
    for (auto& th : threads) {
        th.join();
    }
    ASSERT_EQ(success_count, num_threads * allocs_per_thread);
    // Verify values after all threads finished
    for (int idx = 0; idx < num_threads * allocs_per_thread; ++idx) {
        int* ptr = static_cast<int*>(alloc.get(mem_ids[idx]));
        ASSERT_NE(ptr, nullptr);
        ASSERT_EQ(*ptr, idx * 10);
        alloc.deallocate(mem_ids[idx]);
    }
    ASSERT_EQ(alloc.get_remain_size(), pool_size);
}

TEST_F(WBEAllocAtomicSharedMutexAlignedPoolTest, MixedAllocDeallocTest) {
    const size_t pool_size = WBE_MI_B(1);
    WBE::HeapAllocAtomicSharedMutexAlignedPool alloc(pool_size);
    const int num_threads = 8;
    const int ops_per_thread = 256;
    std::vector<std::thread> threads;
    std::vector<std::vector<WBE::MemID>> thread_mem_ids(num_threads);
    std::atomic<int> alloc_count{0};
    std::atomic<int> dealloc_count{0};
    auto mix_func = [&](int p_thread_idx) {
        std::vector<WBE::MemID>& mems = thread_mem_ids[p_thread_idx];
        for (int i = 0; i < ops_per_thread; ++i) {
            if (i % 2 == 0) {
                WBE::MemID mem = alloc.allocate(8);
                if (mem != WBE::MEM_NULL) {
                    mems.push_back(mem);
                    ++alloc_count;
                }
            } else if (!mems.empty()) {
                alloc.deallocate(mems.back());
                mems.pop_back();
                ++dealloc_count;
            }
        }
        // Deallocate any remaining allocations
        for (auto mem : mems) {
            alloc.deallocate(mem);
            ++dealloc_count;
        }
        mems.clear();
    };
    threads.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(mix_func, t);
    }
    for (auto& th : threads) {
        th.join();
    }
    ASSERT_EQ(alloc.get_remain_size(), pool_size);
    ASSERT_GE(alloc_count, num_threads * ops_per_thread / 2); // At least half ops are allocs
    ASSERT_EQ(dealloc_count, alloc_count);
}

TEST_F(WBEAllocAtomicSharedMutexAlignedPoolTest, MixedAllocDeallocGetTest) {
    const size_t pool_size = WBE_MI_B(1);
    WBE::HeapAllocAtomicSharedMutexAlignedPool alloc(pool_size);
    const int num_threads = 8;
    const int ops_per_thread = 2048;
    std::vector<std::thread> threads;
    std::vector<std::vector<std::pair<WBE::MemID, int>>> thread_mem_ids(num_threads);
    auto mix_func = [&](int p_thread_idx) {
        std::vector<std::pair<WBE::MemID, int>>& mems = thread_mem_ids[p_thread_idx];
        for (int i = 0; i < ops_per_thread; ++i) {
            int op = std::rand() % 3;
            if (op == 0) {
                WBE::MemID mem = alloc.allocate(8);
                ASSERT_TRUE(mem != WBE::MEM_NULL);
                int val = std::rand();
                void* ptr = alloc.get(mem);
                ASSERT_NE(ptr, nullptr);
                *static_cast<int*>(ptr) = val;
                mems.emplace_back(mem, val);
            } else if (op == 1 && !mems.empty()) {
                alloc.deallocate(mems.back().first);
                mems.pop_back();
            } else if (op == 2 && !mems.empty()) {
                void* ptr = alloc.get(mems.back().first);
                ASSERT_NE(ptr, nullptr);
                ASSERT_EQ(*static_cast<int*>(ptr), mems.back().second);
            }
        }
        // Deallocate any remaining allocations
        for (auto mem : mems) {
            alloc.deallocate(mem.first);
        }
        mems.clear();
    };
    threads.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(mix_func, t);
    }
    for (auto& th : threads) {
        th.join();
    }
    ASSERT_EQ(alloc.get_remain_size(), pool_size);
}

TEST_F(WBEAllocAtomicSharedMutexAlignedPoolTest, SynchronizationDeallocationTest) {
    const size_t pool_size = WBE_MI_B(1);
    WBE::HeapAllocAtomicSharedMutexAlignedPool alloc(pool_size);
    const int num_threads = 8;
    const int allocs_per_thread = 128;
    std::vector<WBE::MemID> mem_ids(num_threads * allocs_per_thread);
    // Allocate all memory in single thread for simplicity
    for (int i = 0; i < num_threads * allocs_per_thread; ++i) {
        WBE::MemID mem = alloc.allocate(8);
        ASSERT_NE(mem, WBE::MEM_NULL);
        mem_ids[i] = mem;
    }
    std::vector<std::thread> threads;
    std::atomic<int> dealloc_count{0};
    auto dealloc_func = [&](int p_thread_idx) {
        // Each thread deallocates its own chunk
        for (int i = 0; i < allocs_per_thread; ++i) {
            int idx = p_thread_idx * allocs_per_thread + i;
            alloc.deallocate(mem_ids[idx]);
            ++dealloc_count;
        }
    };
    threads.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(dealloc_func, t);
    }
    for (auto& th : threads) {
        th.join();
    }
    ASSERT_EQ(dealloc_count, num_threads * allocs_per_thread);
    ASSERT_EQ(alloc.get_remain_size(), pool_size);
}

#endif
