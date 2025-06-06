#ifndef __WBE_REF_STRONG_TEST_H__
#define __WBE_REF_STRONG_TEST_H__

#include "core/allocator/allocator_ram.h"
#include "core/reference/reference_strong.h"
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(WBERefStrongTest, Allocation) {
    WBE::AllocatorRam allocator;
    WBE::Ref<int> ref = WBE::Ref<int>::make_ref(&allocator, 0);
    ASSERT_EQ(*ref, 0);
    *ref = 1;
    ASSERT_EQ(*ref, 1);
}

#endif
