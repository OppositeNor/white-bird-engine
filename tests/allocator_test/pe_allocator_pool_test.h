#ifndef __PE_AALOCATOR_POOL_TEST_H__
#define __PE_AALOCATOR_POOL_TEST_H__

#include "utils/allocator/allocator.h"
#include "utils/allocator/allocator_pool.h"
#include <gtest/gtest.h>
#include <memory>

TEST(PEAllocPoolTestToString, BasicAssersions) {
    PencilEngine::AllocatorPool pool(1024, 4);
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
    void* mem1 = pool.allocate(2);
    ASSERT_EQ(static_cast<std::string>(pool), exp_2);
    void* mem2 = pool.allocate(8);
    ASSERT_EQ(static_cast<std::string>(pool), exp_3);
    pool.deallocate(mem1);
    ASSERT_EQ(static_cast<std::string>(pool), exp_4);
    pool.deallocate(mem2);
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
    pool.allocate(1024);
    ASSERT_EQ(static_cast<std::string>(pool), exp_5);
}

TEST(PEAllocPoolTestRemainSize, BassicAssersions) {
    PencilEngine::AllocatorPool pool(1024, 4);
    void* mem = pool.allocate(2);
    ASSERT_EQ(pool.get_remain_size(), 1020);
    pool.deallocate(mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
    void* all_mem = pool.allocate(1024);
    ASSERT_EQ(pool.get_remain_size(), 0);
    pool.deallocate(all_mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
}

TEST(PEAllocPoolTestConstructDestructCall, BassicAssersions) {
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
    std::shared_ptr<PencilEngine::AllocatorPool> allocator = std::make_shared<PencilEngine::AllocatorPool>();
    TestClass* test_obj = PencilEngine::create_obj<TestClass>(allocator, &test_val);
    ASSERT_EQ(test_val, 1);
    PencilEngine::destroy_obj(allocator, test_obj);
    ASSERT_EQ(test_val, 2);
}

TEST(PEAllocPoolTestWithDiffInstances, BassicAssersions) {
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

    std::shared_ptr<PencilEngine::AllocatorPool> allocator = std::make_shared<PencilEngine::AllocatorPool>(1024, sizeof(uint32_t));
    TestBase* test_obj = PencilEngine::create_obj<TestBase>(allocator);
    ASSERT_EQ(allocator->get_remain_size(), 1024 - sizeof(TestBase));
    TestBase* test_obj1 = PencilEngine::create_obj<TestChild1>(allocator);
    ASSERT_EQ(allocator->get_remain_size(), 1024 - sizeof(TestBase) - sizeof(TestChild1));
    TestBase* test_obj2 = PencilEngine::create_obj<TestChild2>(allocator);
    ASSERT_EQ(allocator->get_remain_size(), 1024 - sizeof(TestBase) - sizeof(TestChild1) - sizeof(TestChild2));
    PencilEngine::destroy_obj(allocator, test_obj);
    ASSERT_EQ(allocator->get_remain_size(), 1024 - sizeof(TestChild1) - sizeof(TestChild2));
    PencilEngine::destroy_obj(allocator, test_obj1);
    ASSERT_EQ(allocator->get_remain_size(), 1024 - sizeof(TestChild2));
    PencilEngine::destroy_obj(allocator, test_obj2);
    ASSERT_EQ(allocator->get_remain_size(), 1024);
}

TEST(PEAllocPoolTestBinded, BassicAssersions) {
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
    PencilEngine::BindedAllocator binded_allocator(std::make_shared<PencilEngine::AllocatorPool>());
    TestClass* test_obj = binded_allocator.create<TestClass>(&test_val);
    ASSERT_EQ(test_val, 1);
    binded_allocator.destroy(test_obj);
    ASSERT_EQ(test_val, 2);
}


#endif
