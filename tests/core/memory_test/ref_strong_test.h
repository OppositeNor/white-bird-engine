#ifndef __WBE_REF_STRONG_TEST_H__
#define __WBE_REF_STRONG_TEST_H__

#include "core/allocator/heap_allocator_pool.h"
#include "core/allocator/heap_allocator_ram.h"
#include "core/memory/reference_strong.h"
#include <cstdint>
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(WBERefStrongTest, Allocation) {
    WBE::HeapAllocatorRam allocator;
    {
        WBE::Ref<int> ref = WBE::Ref<int>::make_ref(&allocator, 0);
        ASSERT_FALSE(allocator.is_empty());
        ASSERT_EQ(*ref, 0);
        *ref = 1;
        ASSERT_EQ(*ref, 1);
        ASSERT_EQ(allocator.get_allocated().size(), 1);
    }
    ASSERT_TRUE(allocator.is_empty());
    {
        WBE::Ref<int> ref(&allocator);
    }
    ASSERT_TRUE(allocator.is_empty());
}

TEST(WBERefStrongTest, CopyMove) {
    WBE::HeapAllocatorRam allocator;
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
        ASSERT_EQ(allocator.get_allocated().size(), 1);
    }
    ASSERT_TRUE(allocator.is_empty());
}

TEST(WBERefStrongTest, ConstructDestruct) {
    WBE::HeapAllocatorRam allocator;
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
        WBE::Ref<TestClass> ref(&allocator);
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

TEST(WBERefStrongTest, DynamicDispatch) {
    class A {
    public:
        virtual void func() {
            num = 1;
        }

        int num = 0;
    };

    class B : public A {
        virtual void func() override {
            num = 2;
        }
    };
    WBE::HeapAllocatorPool allocator;
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

TEST(WBERefStrongTest, NullReference) {
    WBE::HeapAllocatorPool allocator;
    WBE::Ref<int> ref(&allocator, WBE::MEM_NULL);
    ASSERT_EQ(ref.get(), nullptr);
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
    std::vector<WBE::Ref<TestClass, WBE::HeapAllocator>> refs;
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
            uint32_t choice = rand() % 3;
            if (choice == 0) {
                refs[j] = p_ref1;
            }
            else if (choice == 1) {
                refs[j] = p_ref2;
            }
            else {
                refs[j] = refs[rand() % refs.size()];
            }
            ASSERT_EQ(test_val1, 1);
            ASSERT_EQ(test_val2, 1);
        }
    }
}

TEST(WBERefStrongTest, Multithread) {
    ASSERT_EQ(test_val1, 0);
    ASSERT_EQ(test_val2, 0);
    WBE::HeapAllocatorPool allocator;
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
