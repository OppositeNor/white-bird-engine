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
#ifndef __WBE_ALOCATOR_ALIGNED_POOL_TEST_HH__
#define __WBE_ALOCATOR_ALIGNED_POOL_TEST_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "global/global.hh"
#include "test_utilities.hh"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>

namespace WBE = WhiteBirdEngine;

constexpr size_t AAPT_HEADER_SIZE = WBE::HeapAllocatorAlignedPool::HEADER_SIZE;

TEST(WBEAllocAlignedPoolTest, TraitTest) {
    ASSERT_TRUE(WBE::AllocatorTrait<WBE::HeapAllocatorAlignedPool>::IS_POOL);
    ASSERT_TRUE(WBE::AllocatorTrait<WBE::HeapAllocatorAlignedPool>::IS_LIMITED_SIZE);
    ASSERT_FALSE(WBE::AllocatorTrait<WBE::HeapAllocatorAlignedPool>::IS_GURANTEED_CONTINUOUS);
    ASSERT_FALSE(WBE::AllocatorTrait<WBE::HeapAllocatorAlignedPool>::IS_ALLOC_FIXED_SIZE);
    ASSERT_FALSE(WBE::AllocatorTrait<WBE::HeapAllocatorAlignedPool>::IS_ATOMIC);
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleFront) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024, AAPT_HEADER_SIZE);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(4);
    WBE::MemID mem4 = pool.allocate(4);
    (void)mem1, (void)mem2, (void)mem3, (void)mem4; pool.clear(); // TODO
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleBack) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024, AAPT_HEADER_SIZE);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(4);
    WBE::MemID mem4 = pool.allocate(12);
    (void)mem1, (void)mem2, (void)mem3, (void)mem4; pool.clear(); // TODO
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleMiddle) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024, AAPT_HEADER_SIZE);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(4);
    WBE::MemID mem4 = pool.allocate(12);
    WBE::MemID mem5 = pool.allocate(128);
    (void)mem1, (void)mem2, (void)mem3, (void)mem4, (void)mem5; pool.clear(); // TODO
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleEnd) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024, AAPT_HEADER_SIZE);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(pool.get_remain_size() - AAPT_HEADER_SIZE);
    (void)mem1, (void)mem2, (void)mem3; pool.clear(); // TODO
}

TEST(WBEAllocAlignedPoolTest, RemainSize) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WhiteBirdEngine::HeapAllocatorAlignedPool pool(1024);
    WBE::MemID mem = pool.allocate(2);
    ASSERT_LE(pool.get_remain_size(), 1024 - 2);
    ASSERT_GE(pool.get_remain_size(), 1024 - 16);
    pool.deallocate(mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
    WBE::MemID all_mem = pool.allocate(1024 - AAPT_HEADER_SIZE);
    ASSERT_EQ(pool.get_remain_size(), 0);
    pool.deallocate(all_mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
}

TEST(WBEAllocAlignedPoolTest, ConstructDestructCall) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    uint32_t test_val = 0;
    class TestClass {
    public:
        TestClass(uint32_t* p_test_val) {
            test_val_ptr = p_test_val;
            *test_val_ptr = 1;
        }
        ~TestClass() {
            *test_val_ptr = 2;
        }
        uint32_t* test_val_ptr;
    };
    WhiteBirdEngine::HeapAllocatorAlignedPool allocator = WhiteBirdEngine::HeapAllocatorAlignedPool();
    WBE::MemID test_obj = WhiteBirdEngine::create_obj_align<TestClass>(allocator, &test_val);
    ASSERT_EQ(test_val, 1);
    WBE::destroy_obj<TestClass>(allocator, test_obj);
    ASSERT_EQ(test_val, 2);
}

TEST(WBEAllocAlignedPoolTest, AlignmentTest) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WhiteBirdEngine::HeapAllocatorAlignedPool allocator = WhiteBirdEngine::HeapAllocatorAlignedPool(2048);
    WBE::MemID mem1 = allocator.allocate(1, 8);
    ASSERT_EQ(mem1 % 8, 0);
    WBE::MemID mem2 = allocator.allocate(1, 16);
    ASSERT_EQ(mem2 % 16, 0);
    WBE::MemID mem3 = allocator.allocate(1, 32);
    ASSERT_EQ(mem3 % 32, 0);
    WBE::MemID mem4 = allocator.allocate(1, 64);
    ASSERT_EQ(mem4 % 64, 0);
    WBE::MemID mem5 = allocator.allocate(1, 128);
    ASSERT_EQ(mem5 % 128, 0);
    WBE::MemID mem6 = allocator.allocate(1, 256);
    ASSERT_EQ(mem6 % 256, 0);
    WBE::MemID mem7 = allocator.allocate(1, 512);
    ASSERT_EQ(mem7 % 512, 0);
    WBE::MemID mem8 = allocator.allocate(1, 1024);
    ASSERT_EQ(mem8 % 1024, 0);
    WBE::MemID mem9 = allocator.allocate(1, 512);
    ASSERT_EQ(mem9 % 512, 0);
    WBE::MemID mem10 = allocator.allocate(1, 128);
    ASSERT_EQ(mem10 % 128, 0);

    allocator.deallocate(mem1);
    allocator.deallocate(mem2);
    allocator.deallocate(mem3);
    allocator.deallocate(mem4);
    allocator.deallocate(mem5);
    allocator.deallocate(mem6);
    allocator.deallocate(mem7);
    allocator.deallocate(mem8);
    allocator.deallocate(mem9);
    allocator.deallocate(mem10);

    ASSERT_EQ(allocator.get_remain_size(), 2048);
}

TEST(WBEAllocAlignedPoolTest, ManySmallAllocations) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    const int num_allocs = 128;
    WhiteBirdEngine::HeapAllocatorAlignedPool allocator = WhiteBirdEngine::HeapAllocatorAlignedPool(1024 + AAPT_HEADER_SIZE * num_allocs);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024 + AAPT_HEADER_SIZE * num_allocs, AAPT_HEADER_SIZE);
    WBE::MemID mems[num_allocs];
    for (int i = 0; i < num_allocs; ++i) {
        mems[i] = allocator.allocate(8, 8);
        ASSERT_NE(mems[i], WBE::MEM_NULL);
        ASSERT_EQ(mems[i] % 8, 0);
    }
    ASSERT_EQ(allocator.get_remain_size(), 0);
    std::string exp = behv_test({
        {true, -1},
    });
    ASSERT_EQ(static_cast<std::string>(allocator), exp);
    for (int i = 0; i < num_allocs; ++i) {
        allocator.deallocate(mems[i]);
    }
    ASSERT_EQ(allocator.get_remain_size(), 1024 + AAPT_HEADER_SIZE * num_allocs);
}

TEST(WBEAllocAlignedPoolTest, GetMemoryAddress) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WhiteBirdEngine::HeapAllocatorAlignedPool allocator = WhiteBirdEngine::HeapAllocatorAlignedPool(1024);
    WBE::MemID mem1 = allocator.allocate(16, 8);
    WBE::MemID mem2 = allocator.allocate(32, 16);
    WBE::MemID mem3 = allocator.allocate(64, 32);

    void* addr1 = allocator.get(mem1);
    void* addr2 = allocator.get(mem2);
    void* addr3 = allocator.get(mem3);

    ASSERT_NE(addr1, nullptr);
    ASSERT_NE(addr2, nullptr);
    ASSERT_NE(addr3, nullptr);

    ASSERT_EQ(std::bit_cast<WBE::MemID>(addr1), mem1);
    ASSERT_EQ(std::bit_cast<WBE::MemID>(addr2), mem2);
    ASSERT_EQ(std::bit_cast<WBE::MemID>(addr3), mem3);

    allocator.deallocate(mem1);
    allocator.deallocate(mem2);
    allocator.deallocate(mem3);
}

TEST(WBEAllocAlignedPoolTest, GetNullMemoryAddress) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WhiteBirdEngine::HeapAllocatorAlignedPool allocator = WhiteBirdEngine::HeapAllocatorAlignedPool(1024);
    void* addr = allocator.get(WBE::MEM_NULL);
    ASSERT_EQ(addr, nullptr);
}

TEST(WBEAllocAlignedPoolTest, MaxDataSizeTracker) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WhiteBirdEngine::HeapAllocatorAlignedPool allocator = WhiteBirdEngine::HeapAllocatorAlignedPool(1024);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 0);
    WBE::MemID mem1 = allocator.allocate(100, 8);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 112);
    WBE::MemID mem2 = allocator.allocate(200, 16);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 328);
    allocator.deallocate(mem1);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 328);
    WBE::MemID mem3 = allocator.allocate(50, 32);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 328);
    allocator.deallocate(mem2);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 328);
    allocator.deallocate(mem3);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 328);
}

#endif
