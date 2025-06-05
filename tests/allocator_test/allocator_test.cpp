#include "allocator_pool_test.h"
#include "allocator_fixed_size_pool_test.h"
#include "utils/allocator/allocator_ram.h"

TEST(AllocatorTest, TraitTest) {

    ASSERT_FALSE(WBE::FixedSizeAllocatorConcept<WBE::AllocatorRam>);
    ASSERT_TRUE(WBE::FixedSizeAllocatorConcept<WBE::AllocatorFixedSizePool<4>>);

}
