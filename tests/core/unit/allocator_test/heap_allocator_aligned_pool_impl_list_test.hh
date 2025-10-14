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
#ifndef __WBE_ALOCATOR_ALIGNED_POOL_IMPL_LIST_TEST_HH__
#define __WBE_ALOCATOR_ALIGNED_POOL_IMPL_LIST_TEST_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned_pool_impl_list.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>
#include <random>

namespace WBE = WhiteBirdEngine;

constexpr size_t AAPILT_HEADER_SIZE = WBE::HeapAllocatorAlignedPoolImplicitList::WORD_SIZE;

TEST(WBEAllocAlignedPoolImplicitListTest, IsInPoolAllocatedAndDeallocated) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(128);
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

TEST(WBEAllocAlignedPoolImplicitListTest, IsInPoolNullAndOutOfRange) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(128);
    ASSERT_FALSE(pool.is_in_pool(WBE::MEM_NULL));
    WBE::MemID mem = pool.allocate(16);
    uintptr_t fake_id = reinterpret_cast<uintptr_t>(mem) + 1024;
    ASSERT_FALSE(pool.is_in_pool(static_cast<WBE::MemID>(fake_id)));
    pool.deallocate(mem);
}

TEST(WBEAllocAlignedPoolImplicitListTest, ZeroSizeAllocation) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(128);
    WBE::MemID mem = pool.allocate(0);
    ASSERT_EQ(mem, WBE::MEM_NULL);
    ASSERT_EQ(pool.get_remain_size(), 128);
}

TEST(WBEAllocAlignedPoolImplicitListTest, MaxAlignmentAllocation) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(256);
    WBE::MemID mem = pool.allocate(8, 128);
    ASSERT_NE(mem, WBE::MEM_NULL);
    ASSERT_EQ(mem % 128, 0);
    pool.deallocate(mem);
    ASSERT_EQ(pool.get_remain_size(), 256);
}

TEST(WBEAllocAlignedPoolImplicitListTest, DoubleDeallocation) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(128);
    WBE::MemID mem = pool.allocate(16);
    pool.deallocate(mem);
    pool.deallocate(mem);
    ASSERT_EQ(pool.get_remain_size(), 128);
}

TEST(WBEAllocAlignedPoolImplicitListTest, PoolReuseAfterClear) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(128);
    pool.allocate(32);
    pool.clear();
    ASSERT_EQ(pool.get_remain_size(), 128);
    WBE::MemID mem2 = pool.allocate(64);
    ASSERT_NE(mem2, WBE::MEM_NULL);
    pool.deallocate(mem2);
    ASSERT_EQ(pool.get_remain_size(), 128);
}

TEST(WBEAllocAlignedPoolImplicitListTest, FragmentationAndCoalescing) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(128);
    WBE::MemID mem1 = pool.allocate(16);
    WBE::MemID mem2 = pool.allocate(16);
    WBE::MemID mem3 = pool.allocate(16);
    pool.deallocate(mem2);
    ASSERT_LT(pool.get_remain_size(), 128);
    pool.deallocate(mem1);
    pool.deallocate(mem3);
    ASSERT_EQ(pool.get_remain_size(), 128);
}

TEST(WBEAllocAlignedPoolImplicitListTest, StressRandomAllocDealloc) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(WBE_MiB(1));
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

TEST(WBEAllocAlignedPoolImplicitListTest, RemoveIdleFront) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(1024);
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

TEST(WBEAllocAlignedPoolImplicitListTest, RemoveIdleBack) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(1024);
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

TEST(WBEAllocAlignedPoolImplicitListTest, RemoveIdleMiddle) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(1024);
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

TEST(WBEAllocAlignedPoolImplicitListTest, RemoveIdleEnd) {
    WBE::HeapAllocatorAlignedPoolImplicitList pool(1024);
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

#endif
