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

TEST(WBEAllocAlignedPoolTest, ToString) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024, AAPT_HEADER_SIZE);
    std::string exp_1 = behv_test({
        {false, 1024}
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
    WBE::MemID mem1 = pool.allocate(2, 1);
    std::string exp_2 = behv_test({
        {true, 2},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp_2);
    WBE::MemID mem2 = pool.allocate(8, 1);
    std::string exp_3 = behv_test({
        {true, 2},
        {true, 8},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp_3);
    pool.deallocate(mem1);
    std::string exp_4 = behv_test({
        {false, 2 + AAPT_HEADER_SIZE},
        {true, 8},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp_4);
    pool.deallocate(mem2);
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
    auto mem3 = pool.allocate(1024 - AAPT_HEADER_SIZE, 1);
    std::string exp_5 = behv_test({
        {true, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp_5);
    pool.deallocate(mem3);
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleFront) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024, AAPT_HEADER_SIZE);
    WBE::MemID mem1 = pool.allocate(4, 1);
    WBE::MemID mem2 = pool.allocate(8, 1);
    WBE::MemID mem3 = pool.allocate(4, 1);
    WBE::MemID mem4 = pool.allocate(4, 1);
    std::string exp0 = behv_test({
        {true, 4},
        {true, 8},
        {true, 4},
        {true, 4},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp0);
    pool.deallocate(mem3);
    std::string exp1 = behv_test({
        {true, 4},
        {true, 8},
        {false, 4 + AAPT_HEADER_SIZE},
        {true, 4},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem2);
    std::string exp2 = behv_test({
        {true, 4},
        {false, 12 + AAPT_HEADER_SIZE * 2},
        {true, 4},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem4);
    std::string exp3 = behv_test({
        {true, 4},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = behv_test({
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleBack) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024, AAPT_HEADER_SIZE);
    WBE::MemID mem1 = pool.allocate(4, 1);
    WBE::MemID mem2 = pool.allocate(8, 1);
    WBE::MemID mem3 = pool.allocate(4, 1);
    WBE::MemID mem4 = pool.allocate(12, 1);
    pool.deallocate(mem2);
    std::string exp1 = behv_test({
        {true, 4},
        {false, 8 + AAPT_HEADER_SIZE},
        {true, 4},
        {true, 12},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem3);
    std::string exp2 = behv_test({
        {true, 4},
        {false, 12 + AAPT_HEADER_SIZE * 2},
        {true, 12},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem4);
    std::string exp3 = behv_test({
        {true, 4},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = behv_test({
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleMiddle) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024, AAPT_HEADER_SIZE);
    WBE::MemID mem1 = pool.allocate(4, 1);
    WBE::MemID mem2 = pool.allocate(8, 1);
    WBE::MemID mem3 = pool.allocate(4, 1);
    WBE::MemID mem4 = pool.allocate(12, 1);
    WBE::MemID mem5 = pool.allocate(128, 1);
    pool.deallocate(mem2);
    std::string exp1 = behv_test({
        {true, 4},
        {false, 8 + AAPT_HEADER_SIZE},
        {true, 4},
        {true, 12},
        {true, 128},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem4);
    std::string exp2 = behv_test({
        {true, 4},
        {false, 8 + AAPT_HEADER_SIZE},
        {true, 4},
        {false, 12 + AAPT_HEADER_SIZE},
        {true, 128},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem3);
    std::string exp3 = behv_test({
        {true, 4},
        {false, 24 + AAPT_HEADER_SIZE * 3},
        {true, 128},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = behv_test({
        {false, 28 + AAPT_HEADER_SIZE * 4},
        {true, 128},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
    pool.deallocate(mem5);
    std::string exp5 = behv_test({
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp5);
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleEnd) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024, AAPT_HEADER_SIZE);
    WBE::MemID mem1 = pool.allocate(4, 1);
    WBE::MemID mem2 = pool.allocate(8, 1);
    WBE::MemID mem3 = pool.allocate(pool.get_remain_size() - AAPT_HEADER_SIZE, 1);
    std::string exp1 = behv_test({
        {true, 4},
        {true, 8},
        {true, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem3);
    std::string exp2 = behv_test({
        {true, 4},
        {true, 8},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem2);
    std::string exp3 = behv_test({
        {true, 4},
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = behv_test({
        {false, -1},
    });
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
}

TEST(WBEAllocAlignedPoolTest, RemainSize) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WhiteBirdEngine::HeapAllocatorAlignedPool pool(1024);
    WBE::MemID mem = pool.allocate(2, 1);
    ASSERT_EQ(pool.get_remain_size(), 1022 - AAPT_HEADER_SIZE);
    pool.deallocate(mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
    WBE::MemID all_mem = pool.allocate(1024 - AAPT_HEADER_SIZE, 1);
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

TEST(WBEAllocAlignedPoolTest, WithDiffInstances) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    class TestBase {
    public:
        TestBase() {
            (void)placeholder;
        }
        uint32_t placeholder;
    };
    class TestChild1 : public TestBase {
    public:
        TestChild1() {
            (void)child_placeholder;
        }
        uint32_t child_placeholder;
    };
    class TestChild2 : public TestBase {
    public:
        TestChild2() {
            (void)child_placeholder1;
            (void)child_placeholder2;
        }
        uint32_t child_placeholder1;
        uint32_t child_placeholder2;
    };

    WhiteBirdEngine::HeapAllocatorAlignedPool allocator = WhiteBirdEngine::HeapAllocatorAlignedPool(1024);
    WBE::MemID test_obj = WhiteBirdEngine::create_obj_align<TestBase>(allocator);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestBase) - AAPT_HEADER_SIZE);
    WBE::MemID test_obj1 = WhiteBirdEngine::create_obj_align<TestChild1>(allocator);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestBase) - sizeof(TestChild1) - AAPT_HEADER_SIZE * 2);
    WBE::MemID test_obj2 = WhiteBirdEngine::create_obj_align<TestChild2>(allocator);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestBase) - sizeof(TestChild1) - sizeof(TestChild2) - AAPT_HEADER_SIZE * 3);
    WhiteBirdEngine::destroy_obj<TestBase>(allocator, test_obj);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestChild1) - sizeof(TestChild2) - AAPT_HEADER_SIZE * 2);
    WhiteBirdEngine::destroy_obj<TestChild1>(allocator, test_obj1);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestChild2) - AAPT_HEADER_SIZE);
    WhiteBirdEngine::destroy_obj<TestChild2>(allocator, test_obj2);
    ASSERT_EQ(allocator.get_remain_size(), 1024);
}

TEST(WBEAllocAlignedPoolTest, AlignmentTest) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WhiteBirdEngine::HeapAllocatorAlignedPool allocator = WhiteBirdEngine::HeapAllocatorAlignedPool(2048);
    WBE::MemID mem1 = allocator.allocate(1, 1);
    ASSERT_EQ(mem1 % 1, 0);
    WBE::MemID mem2 = allocator.allocate(1, 2);
    ASSERT_EQ(mem2 % 2, 0);
    WBE::MemID mem3 = allocator.allocate(1, 4);
    ASSERT_EQ(mem3 % 4, 0);
    WBE::MemID mem4 = allocator.allocate(1, 8);
    ASSERT_EQ(mem4 % 8, 0);
    WBE::MemID mem5 = allocator.allocate(1, 16);
    ASSERT_EQ(mem5 % 16, 0);
    WBE::MemID mem6 = allocator.allocate(1, 32);
    ASSERT_EQ(mem6 % 32, 0);
    WBE::MemID mem7 = allocator.allocate(1, 64);
    ASSERT_EQ(mem7 % 64, 0);
    WBE::MemID mem8 = allocator.allocate(1, 128);
    ASSERT_EQ(mem8 % 128, 0);
    WBE::MemID mem9 = allocator.allocate(1, 256);
    ASSERT_EQ(mem9 % 256, 0);
    WBE::MemID mem10 = allocator.allocate(1, 512);
    ASSERT_EQ(mem10 % 512, 0);

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
    const int num_allocs = 256; // 256 * 4 = 1024
    WhiteBirdEngine::HeapAllocatorAlignedPool allocator = WhiteBirdEngine::HeapAllocatorAlignedPool(1024 + AAPT_HEADER_SIZE * num_allocs);
    WBEAllocPoolBehavTestClass behv_test("HeapAllocatorAlignedPool", 1024 + AAPT_HEADER_SIZE * num_allocs, AAPT_HEADER_SIZE);
    WBE::MemID mems[num_allocs];
    for (int i = 0; i < num_allocs; ++i) {
        mems[i] = allocator.allocate(4, 4);
        ASSERT_NE(mems[i], WBE::MEM_NULL);
        ASSERT_EQ(mems[i] % 4, 0);
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
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 336);
    allocator.deallocate(mem1);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 336);
    WBE::MemID mem3 = allocator.allocate(50, 4);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 336);
    allocator.deallocate(mem2);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 336);
    allocator.deallocate(mem3);
    ASSERT_EQ(allocator.get_internal_fragmentation_tracker(), 336);
}

#endif
