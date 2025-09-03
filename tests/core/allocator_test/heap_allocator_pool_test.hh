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
#ifndef __WBE_ALOCATOR_POOL_TEST_HH__
#define __WBE_ALOCATOR_POOL_TEST_HH__

#include "core/allocator/heap_allocator_pool.hh"
#include "global/global.hh"
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(WBEAllocPoolTest, TraitTest) {
    ASSERT_TRUE(WBE::AllocatorTrait<WBE::HeapAllocatorPool>::IS_POOL);
    ASSERT_TRUE(WBE::AllocatorTrait<WBE::HeapAllocatorPool>::IS_LIMITED_SIZE);
    ASSERT_FALSE(WBE::AllocatorTrait<WBE::HeapAllocatorPool>::IS_GURANTEED_CONTINUOUS);
    ASSERT_FALSE(WBE::AllocatorTrait<WBE::HeapAllocatorPool>::IS_ALLOC_FIXED_SIZE);
    ASSERT_FALSE(WBE::AllocatorTrait<WBE::HeapAllocatorPool>::IS_ATOMIC);
}

TEST(WBEAllocPoolTest, ToString) {
    WBE::HeapAllocatorPool pool(1024);
    std::string exp_1 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":0,\"free_chunk_layout\":[{\"begin\":0,\"size\":1024}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
    WBE::MemID mem1 = pool.allocate(2);
    std::string exp_2 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":1,\"free_chunk_layout\":[{\"begin\":2,\"size\":1022}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp_2);
    WBE::MemID mem2 = pool.allocate(8);
    std::string exp_3 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":2,\"free_chunk_layout\":[{\"begin\":10,\"size\":1014}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp_3);
    pool.deallocate(mem1);
    std::string exp_4 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":1,\"free_chunk_layout\":[{\"begin\":0,\"size\":2},{\"begin\":10,\"size\":1014}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp_4);
    pool.deallocate(mem2);
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
    auto mem3 = pool.allocate(1024);
    std::string exp_5 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":1,\"free_chunk_layout\":[]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp_5);
    pool.deallocate(mem3);
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
}

TEST(WBEAllocPoolTest, RemoveIdleFront) {
    WBE::HeapAllocatorPool pool(1024);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(4);
    WBE::MemID mem4 = pool.allocate(4);
    pool.deallocate(mem3);
    std::string exp1 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":3,\"free_chunk_layout\":[{\"begin\":12,\"size\":4},{\"begin\":20,\"size\":1004}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem2);
    std::string exp2 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":2,\"free_chunk_layout\":[{\"begin\":4,\"size\":12},{\"begin\":20,\"size\":1004}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem4);
    std::string exp3 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":1,\"free_chunk_layout\":[{\"begin\":4,\"size\":1020}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":0,\"free_chunk_layout\":[{\"begin\":0,\"size\":1024}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
}

TEST(WBEAllocPoolTest, RemoveIdleBack) {
    WBE::HeapAllocatorPool pool(1024);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(4);
    WBE::MemID mem4 = pool.allocate(12);
    pool.deallocate(mem2);
    std::string exp1 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":3,\"free_chunk_layout\":[{\"begin\":4,\"size\":8},{\"begin\":28,\"size\":996}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem3);
    std::string exp2 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":2,\"free_chunk_layout\":[{\"begin\":4,\"size\":12},{\"begin\":28,\"size\":996}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem4);
    std::string exp3 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":1,\"free_chunk_layout\":[{\"begin\":4,\"size\":1020}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":0,\"free_chunk_layout\":[{\"begin\":0,\"size\":1024}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
}

TEST(WBEAllocPoolTest, RemoveIdleMiddle) {
    WBE::HeapAllocatorPool pool(1024);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(4);
    WBE::MemID mem4 = pool.allocate(12);
    WBE::MemID mem5 = pool.allocate(128);
    pool.deallocate(mem2);
    std::string exp1 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":4,\"free_chunk_layout\":[{\"begin\":4,\"size\":8},{\"begin\":156,\"size\":868}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem4);
    std::string exp2 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":3,\"free_chunk_layout\":[{\"begin\":4,\"size\":8},{\"begin\":16,\"size\":12},{\"begin\":156,\"size\":868}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem3);
    std::string exp3 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":2,\"free_chunk_layout\":[{\"begin\":4,\"size\":24},{\"begin\":156,\"size\":868}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":1,\"free_chunk_layout\":[{\"begin\":0,\"size\":28},{\"begin\":156,\"size\":868}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
    pool.deallocate(mem5);
    std::string exp5 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":0,\"free_chunk_layout\":[{\"begin\":0,\"size\":1024}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp5);
}

TEST(WBEAllocPoolTest, RemoveIdleEnd) {
    WBE::HeapAllocatorPool pool(1024);
    WBE::MemID mem1 = pool.allocate(4);
    WBE::MemID mem2 = pool.allocate(8);
    WBE::MemID mem3 = pool.allocate(1012);
    std::string exp1 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":3,\"free_chunk_layout\":[]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem3);
    std::string exp2 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":2,\"free_chunk_layout\":[{\"begin\":12,\"size\":1012}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem2);
    std::string exp3 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":1,\"free_chunk_layout\":[{\"begin\":4,\"size\":1020}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = "{\"type\":\"HeapAllocatorPool\",\"obj_count\":0,\"free_chunk_layout\":[{\"begin\":0,\"size\":1024}]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
}

TEST(WBEAllocPoolTest, RemainSize) {
    WhiteBirdEngine::HeapAllocatorPool pool(1024);
    WBE::MemID mem = pool.allocate(2);
    ASSERT_EQ(pool.get_remain_size(), 1022);
    pool.deallocate(mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
    WBE::MemID all_mem = pool.allocate(1024);
    ASSERT_EQ(pool.get_remain_size(), 0);
    pool.deallocate(all_mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
}

TEST(WBEAllocPoolTest, ConstructDestructCall) {
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
    WhiteBirdEngine::HeapAllocatorPool allocator = WhiteBirdEngine::HeapAllocatorPool();
    WBE::MemID test_obj = WhiteBirdEngine::create_obj<TestClass>(allocator, &test_val);
    ASSERT_EQ(test_val, 1);
    WBE::destroy_obj<TestClass>(allocator, test_obj);
    ASSERT_EQ(test_val, 2);
}

TEST(WBEAllocPoolTest, WithDiffInstances) {
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

    WhiteBirdEngine::HeapAllocatorPool allocator = WhiteBirdEngine::HeapAllocatorPool(1024);
    WBE::MemID test_obj = WhiteBirdEngine::create_obj<TestBase>(allocator);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestBase));
    WBE::MemID test_obj1 = WhiteBirdEngine::create_obj<TestChild1>(allocator);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestBase) - sizeof(TestChild1));
    WBE::MemID test_obj2 = WhiteBirdEngine::create_obj<TestChild2>(allocator);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestBase) - sizeof(TestChild1) - sizeof(TestChild2));
    WhiteBirdEngine::destroy_obj<TestBase>(allocator, test_obj);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestChild1) - sizeof(TestChild2));
    WhiteBirdEngine::destroy_obj<TestChild1>(allocator, test_obj1);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestChild2));
    WhiteBirdEngine::destroy_obj<TestChild2>(allocator, test_obj2);
    ASSERT_EQ(allocator.get_remain_size(), 1024);
}

TEST(WBEAllocPoolTest, GetMemoryAddress) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WhiteBirdEngine::HeapAllocatorPool allocator = WhiteBirdEngine::HeapAllocatorPool(1024);
    WBE::MemID mem1 = allocator.allocate(16);
    WBE::MemID mem2 = allocator.allocate(32);
    WBE::MemID mem3 = allocator.allocate(64);

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

TEST(WBEAllocPoolTest, GetNullMemoryAddress) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WhiteBirdEngine::HeapAllocatorPool allocator = WhiteBirdEngine::HeapAllocatorPool(1024);
    void* addr = allocator.get(WBE::MEM_NULL);
    ASSERT_EQ(addr, nullptr);
}

TEST(WBEAllocPoolTest, MaxDataSizeTracker) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WhiteBirdEngine::HeapAllocatorPool allocator = WhiteBirdEngine::HeapAllocatorPool(1024);
    ASSERT_EQ(allocator.get_max_data_size(), 0);
    WBE::MemID mem1 = allocator.allocate(100);
    ASSERT_EQ(allocator.get_max_data_size(), 100);
    WBE::MemID mem2 = allocator.allocate(200);
    ASSERT_EQ(allocator.get_max_data_size(), 300);
    allocator.deallocate(mem1);
    ASSERT_EQ(allocator.get_max_data_size(), 300);
    WBE::MemID mem3 = allocator.allocate(50);
    ASSERT_EQ(allocator.get_max_data_size(), 300);
    allocator.deallocate(mem2);
    ASSERT_EQ(allocator.get_max_data_size(), 300);
    allocator.deallocate(mem3);
    ASSERT_EQ(allocator.get_max_data_size(), 300);
}

#endif
