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
#ifndef __WBE_HEAP_ALLOCATOR_ATOMIC_ALIGNED_POOL_IMPL_LIST_TEST_HH__
#define __WBE_HEAP_ALLOCATOR_ATOMIC_ALIGNED_POOL_IMPL_LIST_TEST_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_atomic_aligned_pool_impl_list.hh"
#include "global/global.hh"
#include <algorithm>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <thread>
#include <atomic>

namespace WBE = WhiteBirdEngine;

class WBEHeapAllocAtomicAlignedPoolImplicitListTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;

    static constexpr size_t AAPILT_HEADER_SIZE = WBE::HeapAllocatorAtomicAlignedPoolImplicitList::WORD_SIZE;
};

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, IsInPoolAllocatedAndDeallocated) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(128);
    WBE::MemID mem1 = pool.allocate(16);
    WBE::MemID mem2 = pool.allocate(16);
    ASSERT_TRUE(pool.is_in_pool(mem1));
    ASSERT_TRUE(pool.is_in_pool(mem2));
    pool.deallocate(mem1);
    ASSERT_FALSE(pool.is_in_pool(mem1));
    ASSERT_TRUE(pool.is_in_pool(mem2));
    pool.deallocate(mem2);
    ASSERT_FALSE(pool.is_in_pool(mem2));
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, IsInPoolNullAndOutOfRange) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(128);
    ASSERT_FALSE(pool.is_in_pool(WBE::MEM_NULL));
    WBE::MemID mem = pool.allocate(16);
    uintptr_t fake_id = reinterpret_cast<uintptr_t>(mem) + 1024;
    ASSERT_FALSE(pool.is_in_pool(static_cast<WBE::MemID>(fake_id)));
    pool.deallocate(mem);
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, ZeroSizeAllocation) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(128);
    WBE::MemID mem = pool.allocate(0);
    ASSERT_EQ(mem, WBE::MEM_NULL);
    ASSERT_EQ(pool.get_remain_size(), 128);
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, MaxAlignmentAllocation) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(256);
    WBE::MemID mem = pool.allocate(8, 128);
    ASSERT_NE(mem, WBE::MEM_NULL);
    ASSERT_EQ(mem % 128, 0);
    pool.deallocate(mem);
    ASSERT_EQ(pool.get_remain_size(), 256);
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, PoolReuseAfterClear) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(128);
    pool.allocate(32);
    pool.clear();
    ASSERT_EQ(pool.get_remain_size(), 128);
    WBE::MemID mem2 = pool.allocate(64);
    ASSERT_NE(mem2, WBE::MEM_NULL);
    pool.deallocate(mem2);
    ASSERT_EQ(pool.get_remain_size(), 128);
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, FragmentationAndCoalescing) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(128);
    WBE::MemID mem1 = pool.allocate(16);
    WBE::MemID mem2 = pool.allocate(16);
    WBE::MemID mem3 = pool.allocate(16);
    pool.deallocate(mem2);
    ASSERT_LT(pool.get_remain_size(), 128);
    pool.deallocate(mem1);
    pool.deallocate(mem3);
    ASSERT_EQ(pool.get_remain_size(), 128);
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, StressRandomAllocDealloc) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(WBE_MiB(1));
    std::vector<WBE::MemID> mems;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(8, 64);
    for (int i = 0; i < 32; ++i) {
        int sz = dist(rng);
        WBE::MemID mem = pool.allocate(sz);
        if (mem != WBE::MEM_NULL) mems.push_back(mem);
    }
    std::shuffle(mems.begin(), mems.end(), rng);
    for (size_t i = 0; i < mems.size(); ++i) {
        pool.deallocate(mems[i]);
    }
    ASSERT_EQ(pool.get_remain_size(), WBE_MiB(1));
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, RemoveIdleFront) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(1024);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(4);
    WBE::MemID mem4 = pool.allocate(4);
    pool.deallocate(mem1);
    ASSERT_FALSE(pool.is_in_pool(mem1));
    ASSERT_TRUE(pool.is_in_pool(mem2));
    ASSERT_TRUE(pool.is_in_pool(mem3));
    ASSERT_TRUE(pool.is_in_pool(mem4));
    pool.clear();
    ASSERT_EQ(pool.get_remain_size(), 1024);
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, RemoveIdleBack) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(1024);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(4);
    WBE::MemID mem4 = pool.allocate(12);
    pool.deallocate(mem4);
    ASSERT_TRUE(pool.is_in_pool(mem1));
    ASSERT_TRUE(pool.is_in_pool(mem2));
    ASSERT_TRUE(pool.is_in_pool(mem3));
    ASSERT_FALSE(pool.is_in_pool(mem4));
    pool.clear();
    ASSERT_EQ(pool.get_remain_size(), 1024);
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, RemoveIdleMiddle) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(1024);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(4);
    WBE::MemID mem4 = pool.allocate(12);
    WBE::MemID mem5 = pool.allocate(128);
    pool.deallocate(mem3);
    ASSERT_TRUE(pool.is_in_pool(mem1));
    ASSERT_TRUE(pool.is_in_pool(mem2));
    ASSERT_FALSE(pool.is_in_pool(mem3));
    ASSERT_TRUE(pool.is_in_pool(mem4));
    ASSERT_TRUE(pool.is_in_pool(mem5));
    pool.clear();
    ASSERT_EQ(pool.get_remain_size(), 1024);
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, RemoveIdleEnd) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(1024);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(pool.get_remain_size() - AAPILT_HEADER_SIZE);
    pool.deallocate(mem3);
    ASSERT_TRUE(pool.is_in_pool(mem1));
    ASSERT_TRUE(pool.is_in_pool(mem2));
    ASSERT_FALSE(pool.is_in_pool(mem3));
    pool.clear();
    ASSERT_EQ(pool.get_remain_size(), 1024);
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, ConcurrentAllocations) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(WBE_MiB(4));
    constexpr int NUM_THREADS = 8;
    constexpr int ALLOCS_PER_THREAD = 100;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<WBE::MemID>> thread_mems(NUM_THREADS);
    std::atomic<int> success_count{0};
    
    // Barrier to synchronize thread start
    std::barrier sync_point(NUM_THREADS);
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            sync_point.arrive_and_wait();
            std::mt19937 rng(i + 42);
            std::uniform_int_distribution<int> dist(16, 128);
            
            for (int j = 0; j < ALLOCS_PER_THREAD; ++j) {
                int size = dist(rng);
                WBE::MemID mem = pool.allocate(size);
                if (mem != WBE::MEM_NULL) {
                    thread_mems[i].push_back(mem);
                    success_count.fetch_add(1);
                    
                    // Verify the memory is properly aligned and accessible
                    memset(pool.get(mem), 0xAA, size);
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify all allocations are unique and valid
    std::set<WBE::MemID> all_mems;
    for (int i = 0; i < NUM_THREADS; ++i) {
        for (WBE::MemID mem : thread_mems[i]) {
            ASSERT_TRUE(pool.is_in_pool(mem));
            ASSERT_TRUE(all_mems.insert(mem).second); // Should be unique
        }
    }
    
    ASSERT_GT(success_count.load(), 0);
    ASSERT_EQ(all_mems.size(), success_count.load());
    
    // Clean up
    for (int i = 0; i < NUM_THREADS; ++i) {
        for (WBE::MemID mem : thread_mems[i]) {
            pool.deallocate(mem);
        }
    }
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, ConcurrentDeallocations) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(WBE_MiB(2));
    constexpr int NUM_THREADS = 4;
    constexpr int ALLOCS_PER_THREAD = 50;
    
    // First, allocate memory in single-threaded mode
    std::vector<std::vector<WBE::MemID>> thread_mems(NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; ++i) {
        std::mt19937 rng(i + 100);
        std::uniform_int_distribution<int> dist(32, 64);
        for (int j = 0; j < ALLOCS_PER_THREAD; ++j) {
            WBE::MemID mem = pool.allocate(dist(rng));
            if (mem != WBE::MEM_NULL) {
                thread_mems[i].push_back(mem);
            }
        }
    }
    
    size_t initial_remain = pool.get_remain_size();
    
    // Now deallocate concurrently
    std::vector<std::thread> threads;
    std::barrier sync_point(NUM_THREADS);
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            sync_point.arrive_and_wait();
            for (WBE::MemID mem : thread_mems[i]) {
                pool.deallocate(mem);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify all memory was properly deallocated
    for (int i = 0; i < NUM_THREADS; ++i) {
        for (WBE::MemID mem : thread_mems[i]) {
            ASSERT_FALSE(pool.is_in_pool(mem));
        }
    }
    
    // The remaining size should be greater than initial (due to coalescing)
    ASSERT_GE(pool.get_remain_size(), initial_remain);
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, ConcurrentMixedOperations) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(WBE_MiB(8));
    constexpr int NUM_THREADS = 6;
    constexpr int OPERATIONS_PER_THREAD = 200;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<WBE::MemID>> active_mems(NUM_THREADS);
    std::atomic<int> total_operations{0};
    
    std::barrier sync_point(NUM_THREADS);
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            sync_point.arrive_and_wait();
            std::mt19937 rng(i + 200);
            std::uniform_int_distribution<int> size_dist(16, 256);
            std::uniform_real_distribution<float> op_dist(0.0f, 1.0f);
            
            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                bool should_allocate = active_mems[i].empty() || op_dist(rng) < 0.6f;
                
                if (should_allocate) {
                    // Allocate
                    int size = size_dist(rng);
                    WBE::MemID mem = pool.allocate(size);
                    if (mem != WBE::MEM_NULL) {
                        active_mems[i].push_back(mem);
                        // Write pattern to verify memory integrity
                        memset(pool.get(mem), static_cast<int>(i + j) & 0xFF, size);
                    }
                } else {
                    // Deallocate
                    if (!active_mems[i].empty()) {
                        std::uniform_int_distribution<size_t> idx_dist(0, active_mems[i].size() - 1);
                        size_t idx = idx_dist(rng);
                        WBE::MemID mem = active_mems[i][idx];
                        pool.deallocate(mem);
                        active_mems[i].erase(active_mems[i].begin() + idx);
                    }
                }
                total_operations.fetch_add(1);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(total_operations.load(), NUM_THREADS * OPERATIONS_PER_THREAD);
    
    // Verify remaining allocations are valid
    for (int i = 0; i < NUM_THREADS; ++i) {
        for (WBE::MemID mem : active_mems[i]) {
            ASSERT_TRUE(pool.is_in_pool(mem));
        }
    }
    
    // Clean up remaining allocations
    for (int i = 0; i < NUM_THREADS; ++i) {
        for (WBE::MemID mem : active_mems[i]) {
            pool.deallocate(mem);
        }
    }
}

TEST_F(WBEHeapAllocAtomicAlignedPoolImplicitListTest, ConcurrentStressWithAlignment) {
    WBE::HeapAllocatorAtomicAlignedPoolImplicitList pool(WBE_MiB(4));
    constexpr int NUM_THREADS = 4;
    constexpr int STRESS_ITERATIONS = 100;
    
    std::vector<std::thread> threads;
    std::atomic<int> alignment_failures{0};
    std::atomic<int> allocation_count{0};
    
    std::barrier sync_point(NUM_THREADS);
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {

            sync_point.arrive_and_wait();
            std::mt19937 rng(i + 300);
            std::uniform_int_distribution<int> size_dist(8, 64);
            std::vector<size_t> alignments = {1, 2, 4, 8, 16, 32, 64};
            std::uniform_int_distribution<size_t> align_dist(0, alignments.size() - 1);
            
            std::vector<WBE::MemID> mems;
            
            for (int j = 0; j < STRESS_ITERATIONS; ++j) {
                int size = size_dist(rng);
                size_t alignment = alignments[align_dist(rng)] * AAPILT_HEADER_SIZE;
                WBE::MemID mem = pool.allocate(size, alignment);
                EXPECT_NE(mem, WBE::MEM_NULL);
                EXPECT_TRUE(pool.is_in_pool(mem));

                allocation_count.fetch_add(1);

                // Verify alignment
                if (mem % alignment != 0) {
                    alignment_failures.fetch_add(1);
                }

                mems.push_back(mem);

                // Test memory access
                memset(pool.get(mem), i & 0xFF, size);

                for (WBE::MemID mem : mems) {
                    EXPECT_TRUE(pool.is_in_pool(mem));
                }
                
                // Occasionally deallocate some memory
                if (!mems.empty() && (j % 10 == 0)) {
                    std::uniform_int_distribution<size_t> idx_dist(0, mems.size() - 1);
                    size_t idx = idx_dist(rng);
                    pool.deallocate(mems[idx]);
                    mems.erase(mems.begin() + idx);
                }
            }
            
            // Clean up remaining allocations
            for (WBE::MemID mem : mems) {
                pool.deallocate(mem);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(alignment_failures.load(), 0);
    ASSERT_GT(allocation_count.load(), 0);
}

#endif
