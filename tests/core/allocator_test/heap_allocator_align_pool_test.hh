#ifndef __WBE_ALOCATOR_ALIGNED_POOL_TEST_HH__
#define __WBE_ALOCATOR_ALIGNED_POOL_TEST_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(WBEAllocAlignedPoolTest, TraitTest) {
    ASSERT_TRUE(WBE::AllocatorTrait<WBE::HeapAllocatorAlignedPool>::IS_POOL);
    ASSERT_TRUE(WBE::AllocatorTrait<WBE::HeapAllocatorAlignedPool>::IS_LIMITED_SIZE);
    ASSERT_FALSE(WBE::AllocatorTrait<WBE::HeapAllocatorAlignedPool>::IS_GURANTEED_CONTINUOUS);
    ASSERT_FALSE(WBE::AllocatorTrait<WBE::HeapAllocatorAlignedPool>::IS_ALLOC_FIXED_SIZE);
    ASSERT_FALSE(WBE::AllocatorTrait<WBE::HeapAllocatorAlignedPool>::IS_ATOMIC);
}

TEST(WBEAllocAlignedPoolTest, ToString) {
    WBE::HeapAllocatorAlignedPool pool(1024);
    std::string exp_1 = "HeapAllocatorAlignedPool: {obj_count: 0;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 0;\n"
        "size: 1024;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
    WBE::MemID mem1 = pool.allocate(2, 1);
    std::string exp_2 = "HeapAllocatorAlignedPool: {obj_count: 1;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 2;\n"
        "size: 1022;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp_2);
    WBE::MemID mem2 = pool.allocate(8, 1);
    std::string exp_3 = "HeapAllocatorAlignedPool: {obj_count: 2;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 10;\n"
        "size: 1014;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp_3);
    pool.deallocate(mem1);
    std::string exp_4 = "HeapAllocatorAlignedPool: {obj_count: 1;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 0;\n"
        "size: 2;\n"
        "};\n"
        "chunk: {\n"
        "begin: 10;\n"
        "size: 1014;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp_4);
    pool.deallocate(mem2);
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
    auto mem3 = pool.allocate(1024, 1);
    std::string exp_5 = "HeapAllocatorAlignedPool: {obj_count: 1;\n"
        "free_chunk_layout: {\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp_5);
    pool.deallocate(mem3);
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleFront) {
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBE::MemID mem1 = pool.allocate(4, 1);
    WBE::MemID mem2 = pool.allocate(8, 1);
    WBE::MemID mem3 = pool.allocate(4, 1);
    WBE::MemID mem4 = pool.allocate(4, 1);
    pool.deallocate(mem3);
    std::string exp1 = "HeapAllocatorAlignedPool: {obj_count: 3;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 12;\n"
        "size: 4;\n"
        "};\n"
        "chunk: {\n"
        "begin: 20;\n"
        "size: 1004;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem2);
    std::string exp2 = "HeapAllocatorAlignedPool: {obj_count: 2;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 4;\n"
        "size: 12;\n"
        "};\n"
        "chunk: {\n"
        "begin: 20;\n"
        "size: 1004;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem4);
    std::string exp3 = "HeapAllocatorAlignedPool: {obj_count: 1;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 4;\n"
        "size: 1020;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = "HeapAllocatorAlignedPool: {obj_count: 0;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 0;\n"
        "size: 1024;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleBack) {
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBE::MemID mem1 = pool.allocate(4, 1);
    WBE::MemID mem2 = pool.allocate(8, 1);
    WBE::MemID mem3 = pool.allocate(4, 1);
    WBE::MemID mem4 = pool.allocate(12, 1);
    pool.deallocate(mem2);
    std::string exp1 = "HeapAllocatorAlignedPool: {obj_count: 3;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 4;\n"
        "size: 8;\n"
        "};\n"
        "chunk: {\n"
        "begin: 28;\n"
        "size: 996;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem3);
    std::string exp2 = "HeapAllocatorAlignedPool: {obj_count: 2;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 4;\n"
        "size: 12;\n"
        "};\n"
        "chunk: {\n"
        "begin: 28;\n"
        "size: 996;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem4);
    std::string exp3 = "HeapAllocatorAlignedPool: {obj_count: 1;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 4;\n"
        "size: 1020;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = "HeapAllocatorAlignedPool: {obj_count: 0;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 0;\n"
        "size: 1024;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleMiddle) {
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBE::MemID mem1 = pool.allocate(4, 1);
    WBE::MemID mem2 = pool.allocate(8, 1);
    WBE::MemID mem3 = pool.allocate(4, 1);
    WBE::MemID mem4 = pool.allocate(12, 1);
    WBE::MemID mem5 = pool.allocate(128, 1);
    pool.deallocate(mem2);
    std::string exp1 = "HeapAllocatorAlignedPool: {obj_count: 4;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 4;\n"
        "size: 8;\n"
        "};\n"
        "chunk: {\n"
        "begin: 156;\n"
        "size: 868;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem4);
    std::string exp2 = "HeapAllocatorAlignedPool: {obj_count: 3;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 4;\n"
        "size: 8;\n"
        "};\n"
        "chunk: {\n"
        "begin: 16;\n"
        "size: 12;\n"
        "};\n"
        "chunk: {\n"
        "begin: 156;\n"
        "size: 868;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem3);
    std::string exp3 = "HeapAllocatorAlignedPool: {obj_count: 2;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 4;\n"
        "size: 24;\n"
        "};\n"
        "chunk: {\n"
        "begin: 156;\n"
        "size: 868;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = "HeapAllocatorAlignedPool: {obj_count: 1;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 0;\n"
        "size: 28;\n"
        "};\n"
        "chunk: {\n"
        "begin: 156;\n"
        "size: 868;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
    pool.deallocate(mem5);
    std::string exp5 = "HeapAllocatorAlignedPool: {obj_count: 0;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 0;\n"
        "size: 1024;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp5);
}

TEST(WBEAllocAlignedPoolTest, RemoveIdleEnd) {
    WBE::HeapAllocatorAlignedPool pool(1024);
    WBE::MemID mem1 = pool.allocate(4, 1);
    WBE::MemID mem2 = pool.allocate(8, 1);
    WBE::MemID mem3 = pool.allocate(1012, 1);
    std::string exp1 = "HeapAllocatorAlignedPool: {obj_count: 3;\n"
        "free_chunk_layout: {\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp1);
    pool.deallocate(mem3);
    std::string exp2 = "HeapAllocatorAlignedPool: {obj_count: 2;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 12;\n"
        "size: 1012;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp2);
    pool.deallocate(mem2);
    std::string exp3 = "HeapAllocatorAlignedPool: {obj_count: 1;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 4;\n"
        "size: 1020;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp3);
    pool.deallocate(mem1);
    std::string exp4 = "HeapAllocatorAlignedPool: {obj_count: 0;\n"
        "free_chunk_layout: {\n"
        "chunk: {\n"
        "begin: 0;\n"
        "size: 1024;\n"
        "};\n"
        "};\n"
        "};";
    ASSERT_EQ(static_cast<std::string>(pool), exp4);
}

TEST(WBEAllocAlignedPoolTest, RemainSize) {
    WhiteBirdEngine::HeapAllocatorAlignedPool pool(1024);
    WBE::MemID mem = pool.allocate(2, 1);
    ASSERT_EQ(pool.get_remain_size(), 1022);
    pool.deallocate(mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
    WBE::MemID all_mem = pool.allocate(1024, 1);
    ASSERT_EQ(pool.get_remain_size(), 0);
    pool.deallocate(all_mem);
    ASSERT_EQ(pool.get_remain_size(), 1024);
}

TEST(WBEAllocAlignedPoolTest, ConstructDestructCall) {
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
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestBase));
    WBE::MemID test_obj1 = WhiteBirdEngine::create_obj_align<TestChild1>(allocator);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestBase) - sizeof(TestChild1));
    WBE::MemID test_obj2 = WhiteBirdEngine::create_obj_align<TestChild2>(allocator);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestBase) - sizeof(TestChild1) - sizeof(TestChild2));
    WhiteBirdEngine::destroy_obj<TestBase>(allocator, test_obj);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestChild1) - sizeof(TestChild2));
    WhiteBirdEngine::destroy_obj<TestChild1>(allocator, test_obj1);
    ASSERT_EQ(allocator.get_remain_size(), 1024 - sizeof(TestChild2));
    WhiteBirdEngine::destroy_obj<TestChild2>(allocator, test_obj2);
    ASSERT_EQ(allocator.get_remain_size(), 1024);
}

// TODO: test flush


#endif
