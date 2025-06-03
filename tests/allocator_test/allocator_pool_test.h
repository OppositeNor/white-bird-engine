#ifndef __ALOCATOR_POOL_TEST_H__
#define __ALOCATOR_POOL_TEST_H__

#include "utils/allocator/allocator.h"
#include "utils/allocator/allocator_pool.h"
#include <gtest/gtest.h>
#include <memory>

namespace WBE = WhiteBirdEngine;

TEST(WBEAllocPoolTestToString, BasicAssersions) {
    WBE::AllocatorPool pool(1024, 4);
    std::string exp_1 = "AllocatorPool: {obj_count: 0;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 0;\n"
        "size: 1024;\n"
        "};\n"
        "};\n"
        "};";
    std::string exp_2 = "AllocatorPool: {obj_count: 1;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 4;\n"
        "size: 1020;\n"
        "};\n"
        "};\n"
        "};";
    std::string exp_3 = "AllocatorPool: {obj_count: 2;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 12;\n"
        "size: 1012;\n"
        "};\n"
        "};\n"
        "};";
    std::string exp_4 = "AllocatorPool: {obj_count: 1;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 0;\n"
        "size: 4;\n"
        "};\n"
        "chunk: {\n"
        "begin: 12;\n"
        "size: 1012;\n"
        "};\n"
        "};\n"
        "};";
    std::string exp_5 = "AllocatorPool: {obj_count: 1;\n"
        "free_chunk_layout: {\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
    WBE::MemID mem1 = pool.allocate(2);
    ASSERT_EQ(static_cast<std::string>(pool), exp_2);
    WBE::MemID mem2 = pool.allocate(8);
    ASSERT_EQ(static_cast<std::string>(pool), exp_3);
    pool.deallocate(mem1);
    ASSERT_EQ(static_cast<std::string>(pool), exp_4);
    pool.deallocate(mem2);
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
    pool.allocate(1024);
    ASSERT_EQ(static_cast<std::string>(pool), exp_5);
}

TEST(WBEAllocPoolTestRemainSize, BassicAssersions) {
    WhiteBirdEngine::AllocatorPool pool(1024, 4);
    WBE::MemID mem = pool.allocate(2);
    ASSERT_EQ(pool.get_remain_size(), 1020);
    pool.deallocate(mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
    WBE::MemID all_mem = pool.allocate(1024);
    ASSERT_EQ(pool.get_remain_size(), 0);
    pool.deallocate(all_mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
}

TEST(WBEAllocPoolTestConstructDestructCall, BassicAssersions) {
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
    std::shared_ptr<WhiteBirdEngine::AllocatorPool> allocator = std::make_shared<WhiteBirdEngine::AllocatorPool>();
    WBE::MemID test_obj = WhiteBirdEngine::create_obj<TestClass>(allocator, &test_val);
    ASSERT_EQ(test_val, 1);
    WBE::destroy_obj<TestClass>(allocator, test_obj);
    ASSERT_EQ(test_val, 2);
}

TEST(WBEAllocPoolTestWithDiffInstances, BassicAssersions) {
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

    std::shared_ptr<WhiteBirdEngine::AllocatorPool> allocator = std::make_shared<WhiteBirdEngine::AllocatorPool>(1024, sizeof(uint32_t));
    WBE::MemID test_obj = WhiteBirdEngine::create_obj<TestBase>(allocator);
    ASSERT_EQ(allocator->get_remain_size(), 1024 - sizeof(TestBase));
    WBE::MemID test_obj1 = WhiteBirdEngine::create_obj<TestChild1>(allocator);
    ASSERT_EQ(allocator->get_remain_size(), 1024 - sizeof(TestBase) - sizeof(TestChild1));
    WBE::MemID test_obj2 = WhiteBirdEngine::create_obj<TestChild2>(allocator);
    ASSERT_EQ(allocator->get_remain_size(), 1024 - sizeof(TestBase) - sizeof(TestChild1) - sizeof(TestChild2));
    WhiteBirdEngine::destroy_obj<TestBase>(allocator, test_obj);
    ASSERT_EQ(allocator->get_remain_size(), 1024 - sizeof(TestChild1) - sizeof(TestChild2));
    WhiteBirdEngine::destroy_obj<TestChild1>(allocator, test_obj1);
    ASSERT_EQ(allocator->get_remain_size(), 1024 - sizeof(TestChild2));
    WhiteBirdEngine::destroy_obj<TestChild2>(allocator, test_obj2);
    ASSERT_EQ(allocator->get_remain_size(), 1024);
}

// TODO: test flush


#endif
