#ifndef __WBE_REF_STRONG_TEST_H__
#define __WBE_REF_STRONG_TEST_H__

#include "core/allocator/allocator_ram.h"
#include "core/reference/reference_strong.h"
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(WBERefStrongTest, Allocation) {
    WBE::AllocatorRam allocator;
    {
        WBE::Ref<int> ref = WBE::Ref<int>::make_ref(&allocator, 0);
        ASSERT_FALSE(allocator.is_empty());
        ASSERT_EQ(*ref, 0);
        *ref = 1;
        ASSERT_EQ(*ref, 1);
        ASSERT_EQ(allocator.get_allocated().size(), 1);
    }
    ASSERT_TRUE(allocator.is_empty());
}

TEST(WBERefStrongTest, CopyMove) {
    WBE::AllocatorRam allocator;
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

#endif
