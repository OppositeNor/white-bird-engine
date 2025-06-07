#include "heap_allocator_pool_test.h"
#include "heap_allocator_fixed_size_pool_test.h"
#include "core/allocator/heap_allocator_ram.h"

TEST(AllocatorTest, TraitTest) {

    ASSERT_FALSE(WBE::FixedSizeHeapAllocatorConcept<WBE::HeapAllocatorRam>);
    ASSERT_TRUE(WBE::FixedSizeHeapAllocatorConcept<WBE::HeapAllocatorFixedSizePool<4>>);

}
