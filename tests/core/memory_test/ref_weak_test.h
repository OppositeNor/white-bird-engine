#ifndef __WBE_REF_WEAK_TEST_H__
#define __WBE_REF_WEAK_TEST_H__

#include "core/allocator/heap_allocator_pool.h"
#include "core/memory/reference_weak.h"
#include <gtest/gtest.h>
#include <sstream>

namespace WBE = WhiteBirdEngine;

class MockClass {
public:
    MockClass(std::stringstream& p_ss)
        : ss(&p_ss), test_val(0) {
        (*ss) << "Construct.\n";
    }

    virtual ~MockClass() {
        (*ss) << "Destruct.\n";
    }

    virtual void set_test_val(int p_test_val) {
        (*ss) << "Set test val: " << p_test_val << ".\n";
        test_val = p_test_val;
    }

    std::stringstream* ss;
    int test_val;
};

TEST(WBERefWeakTest, General) {
    WBE::HeapAllocatorPool pool_allocator(1024);
    std::stringstream test_ss;
    {
        WBE::Ref<MockClass> ref = WBE::Ref<MockClass>::make_ref(&pool_allocator, test_ss);
        {
            WBE::RefWeak<MockClass> ref_weak = ref;
            ASSERT_TRUE(ref_weak.is_valid());
            ASSERT_EQ(ref_weak.lock()->test_val, 0);
            ref_weak.lock()->set_test_val(5);
            ASSERT_EQ(ref_weak.lock()->test_val, 5);
        }
        ASSERT_EQ(test_ss.str(), std::string(
            "Construct.\n"
            "Set test val: 5.\n"
        ));
        ASSERT_EQ((*ref).test_val, 5);
    }
    ASSERT_EQ(test_ss.str(), std::string(
        "Construct.\n"
        "Set test val: 5.\n"
        "Destruct.\n"
    ));
}

TEST(WBERefWeakTest, Valid) {
    WBE::RefWeak<int> ref_weak;
    ASSERT_FALSE(ref_weak.is_valid());
    WBE::HeapAllocatorPool pool_allocator(1024);
    {
        WBE::Ref<int> ref = WBE::Ref<int>::make_ref(&pool_allocator, 3);
        ref_weak = ref;
        ASSERT_TRUE(ref_weak.is_valid());
    }
    ASSERT_FALSE(ref_weak.is_valid());
}

class MockClassChild : public MockClass {
public:
    MockClassChild(std::stringstream& p_ss)
        : MockClass(p_ss) {
        (*ss) << "Child construct.\n";
    }

    virtual ~MockClassChild() override {
        (*ss) << "Child destruct.\n";
    }

    virtual void set_test_val(int p_test_val) override {
        (*ss) << "Child set test val: " << p_test_val << ".\n";
        test_val = p_test_val;
    }

};

TEST(WBERefWeakTest, CopyMove) {
    WBE::HeapAllocatorPool pool_allocator(1024);
    std::stringstream test_ss;
    WBE::RefWeak<MockClassChild> ref_weak_child;
    {
        WBE::Ref<MockClassChild> ref = WBE::Ref<MockClassChild>::make_ref(&pool_allocator, test_ss);
        ref_weak_child = ref;
        {
            WBE::RefWeak<MockClass> ref_weak = ref;
            ASSERT_EQ(ref->test_val, 0);
            ref_weak.lock()->set_test_val(5);
            ASSERT_EQ(test_ss.str(), std::string(
                "Construct.\n"
                "Child construct.\n"
                "Child set test val: 5.\n"
            ));
            ASSERT_EQ(ref->test_val, 5);
        }
        ref_weak_child.lock()->set_test_val(10);
        ASSERT_EQ(test_ss.str(), std::string(
            "Construct.\n"
            "Child construct.\n"
            "Child set test val: 5.\n"
            "Child set test val: 10.\n"
        ));
        ASSERT_EQ(ref->test_val, 10);
    }
    ASSERT_EQ(test_ss.str(), std::string(
        "Construct.\n"
        "Child construct.\n"
        "Child set test val: 5.\n"
        "Child set test val: 10.\n"
        "Child destruct.\n"
        "Destruct.\n"
    ));
    ASSERT_FALSE(ref_weak_child.is_valid());
    
}

#endif
