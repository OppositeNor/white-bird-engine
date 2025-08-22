#ifndef __WBE_STACK_ALLOCATOR_TEST_HH__
#define __WBE_STACK_ALLOCATOR_TEST_HH__

#include "core/allocator/stack_allocator.hh"
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(StackAllocator, General) {

    int val = 0;

    class TestClass {
    public:
        TestClass(int* p_val)
            : pval(p_val) {
            *p_val = 4;
            test_val = 5;
        }

        ~TestClass() {
            *pval = 2;
        }

        int test_val;
        int *pval = nullptr;
    };

    WBE::StackAllocator allocator;
    WBE::MemID test_class_mem_id = WBE::create_stack_obj<TestClass>(allocator, &val);
    ASSERT_EQ(val, 4);
    ASSERT_EQ(allocator.get_obj<TestClass>(test_class_mem_id)->test_val, 5);
    allocator.get_obj<TestClass>(test_class_mem_id)->test_val = 10;
    ASSERT_EQ(allocator.get_obj<TestClass>(test_class_mem_id)->test_val, 10);
    WBE::pop_stack_obj<TestClass>(allocator);
    ASSERT_EQ(val, 2);
}

#endif
