#ifndef __PE_AALOCATOR_POOL_TEST_H__
#define __PE_AALOCATOR_POOL_TEST_H__

#include "utils/allocator/pe_allocator.h"
#include "utils/allocator/pe_allocator_pool.h"
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
    PencilEngine::delete_obj(allocator, test_obj);
    ASSERT_EQ(test_val, 2);
}


#endif
