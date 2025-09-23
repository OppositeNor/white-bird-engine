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

TEST(StackAllocator, ToString) {
    WBE::StackAllocator allocator(1024);
    std::string exp_1 = "{\"type\":\"StackAllocator\",\"total_size\":1024,\"stack_pointer\":0,\"available\":1024}";
    ASSERT_EQ(static_cast<std::string>(allocator), exp_1);

    WBE::create_stack_obj<int>(allocator, 42);
    std::string exp_2 = "{\"type\":\"StackAllocator\",\"total_size\":1024,\"stack_pointer\":8,\"available\":1016}";
    ASSERT_EQ(static_cast<std::string>(allocator), exp_2);

    WBE::create_stack_obj<double>(allocator, 3.14);
    std::string exp_3 = "{\"type\":\"StackAllocator\",\"total_size\":1024,\"stack_pointer\":16,\"available\":1008}";
    ASSERT_EQ(static_cast<std::string>(allocator), exp_3);

    WBE::pop_stack_obj<double>(allocator);
    ASSERT_EQ(static_cast<std::string>(allocator), exp_2);

    WBE::pop_stack_obj<int>(allocator);
    ASSERT_EQ(static_cast<std::string>(allocator), exp_1);

    allocator.clear();
    ASSERT_EQ(static_cast<std::string>(allocator), exp_1);
}

#endif
