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
#ifndef __WBE_REF_STRONG_TEST_HH__
#define __WBE_REF_STRONG_TEST_HH__

#include "core/memory/reference_strong.hh"
#include "mock_heap_allocator_aligned.hh"
#include "global/global.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <thread>

namespace WBE = WhiteBirdEngine;

class WBERefStrongTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
};

TEST_F(WBERefStrongTest, Allocation) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<int> ref = WBE::Ref<int>::make_ref(&allocator, 0);
        ASSERT_FALSE(allocator.is_empty());
        ASSERT_EQ(*ref, 0);
        *ref = 1;
        ASSERT_EQ(*ref, 1);
    }
    ASSERT_TRUE(allocator.is_empty());
    {
        WBE::Ref<int> ref;
    }
    ASSERT_TRUE(allocator.is_empty());
}

TEST_F(WBERefStrongTest, CopyMove) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<int> ref = WBE::Ref<int>::make_ref(&allocator, 3);
        ASSERT_FALSE(allocator.is_empty());
        ASSERT_EQ(*ref, 3);
        WBE::Ref<int> ref_copy = ref;
        ASSERT_EQ(*ref_copy, 3);
        ASSERT_EQ(*ref, 3);
        *ref = 5;
        ASSERT_EQ(*ref_copy, 5);
        ASSERT_EQ(*ref, 5);
        WBE::Ref<int> ref_move = std::move(ref);
        ASSERT_EQ(*ref_move, 5);
        ASSERT_EQ(*ref_copy, 5);
        *ref_move = 2;
        ASSERT_EQ(*ref_move, 2);
        ASSERT_EQ(*ref_copy, 2);
    }
    ASSERT_TRUE(allocator.is_empty());
}

TEST_F(WBERefStrongTest, ConstructDestruct) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    int test_val = 0;
    class TestClass {
    public:
        int* modify_val;
        TestClass(int* p_modify_val)
            : modify_val(p_modify_val) {
            ++(*modify_val);
        }
        ~TestClass() {
            *modify_val = 100;
        }
    };
    {
        WBE::Ref<TestClass> ref;
        ASSERT_EQ(test_val, 0);
        ref = WBE::Ref<TestClass>::make_ref(&allocator, &test_val);
        ASSERT_EQ(test_val, 1);
        {
            WBE::Ref<TestClass> ref1 = ref;
            ASSERT_EQ(test_val, 1);
        }
        ASSERT_EQ(test_val, 1);
    }
    ASSERT_TRUE(allocator.is_empty());
    ASSERT_EQ(test_val, 100);
}

TEST_F(WBERefStrongTest, DynamicDispatch) {
    class A {
    public:
        A() = default;
        virtual ~A() {}

        virtual void func() {
            num = 1;
        }

        int num = 0;
    };

    class B : public A {
    public:
        B() = default;
        virtual ~B() override {}
        virtual void func() override {
            num = 2;
        }
    };
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<A> ref = WBE::Ref<A>::make_ref(&allocator);
        ASSERT_EQ(ref->num, 0);
        ref->func();
        ASSERT_EQ(ref->num, 1);
        WBE::Ref<A> ref_b = WBE::Ref<B>::make_ref(&allocator);
        ASSERT_EQ(ref_b->num, 0);
        ref_b->func();
        ASSERT_EQ(ref_b->num, 2);
    }
    ASSERT_TRUE(allocator.is_empty());
}

TEST_F(WBERefStrongTest, NullReference) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    WBE::Ref<int> ref(&allocator, WBE::MEM_NULL);
    ASSERT_EQ(ref, nullptr);
    ASSERT_EQ(ref, (void*)nullptr);
    ASSERT_EQ(ref, WBE::MEM_NULL);
    ASSERT_EQ(ref.get(), nullptr);
    ref = WBE::Ref<int>::make_ref(&allocator, 5);
    ASSERT_NE(ref, nullptr);
    ASSERT_NE(ref, (void*)nullptr);
    ASSERT_NE(ref, WBE::MEM_NULL);
    ASSERT_NE(ref.get(), nullptr);
}

class TestClass {
public:
    TestClass(int* p_val)
        : val(p_val) {
        ++(*p_val);
    }
    ~TestClass() {
        *val = -1;
    }

    int* val;
};

inline int test_val1 = 0;
inline int test_val2 = 0;

inline void multithread_ref_test(WBE::Ref<TestClass> p_ref1, WBE::Ref<TestClass> p_ref2, uint32_t p_iteration_count) {
    std::vector<WBE::Ref<TestClass>> refs;
    WBE::MockHeapAllocatorAligned allocator(1024);
    refs.reserve(5);
    for (uint32_t i = 0; i < 5; ++i) {
        if (rand() & 0x1) {
            refs.push_back(p_ref1);
        }
        else {
            refs.push_back(p_ref2);
        }
    }
    for (uint32_t i = 0; i < p_iteration_count; ++i) {
        uint32_t rand_ref_count = rand() % 5;
        for (uint32_t j = 0; j < rand_ref_count; ++j) {
            uint32_t choice = rand() % 4;
            if (choice == 0) {
                refs[j] = p_ref1;
            }
            else if (choice == 1) {
                refs[j] = p_ref2;
            }
            else if (choice == 2) {
                refs[j] = refs[rand() % refs.size()];
            }
            else {
                int val = 3;
                {
                    WBE::Ref<TestClass> ref = WBE::Ref<TestClass>::make_ref(&allocator, &val);
                    ASSERT_EQ(val, 4);
                }
                ASSERT_EQ(val, -1);
            }
            ASSERT_EQ(test_val1, 1);
            ASSERT_EQ(test_val2, 1);
        }
    }
}

TEST_F(WBERefStrongTest, Multithread) {
    ASSERT_EQ(test_val1, 0);
    ASSERT_EQ(test_val2, 0);
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<TestClass> ref1 = WBE::Ref<TestClass>::make_ref(&allocator, &test_val1);
        WBE::Ref<TestClass> ref2 = WBE::Ref<TestClass>::make_ref(&allocator, &test_val2);
        ASSERT_EQ(test_val1, 1);
        ASSERT_EQ(test_val2, 1);
        std::thread thread1(multithread_ref_test, ref1, ref2, 500000);
        std::thread thread2(multithread_ref_test, ref1, ref2, 500000);
        std::thread thread3(multithread_ref_test, ref1, ref2, 500000);
        std::thread thread4(multithread_ref_test, ref1, ref2, 500000);
        std::thread thread5(multithread_ref_test, ref1, ref2, 500000);
        thread1.join();
        thread2.join();
        thread3.join();
        thread4.join();
        thread5.join();
    }
    ASSERT_EQ(test_val1, -1);
    ASSERT_EQ(test_val2, -1);
}

#endif
