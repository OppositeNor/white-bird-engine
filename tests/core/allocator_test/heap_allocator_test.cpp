#include "heap_allocator_pool_test.h"
#include "heap_allocator_fixed_size_pool_test.h"
#include "core/allocator/heap_allocator_ram.h"
#include "core/ecs/ecs.h"

TEST(AllocatorTest, TraitTest) {

    ASSERT_FALSE(WBE::ComponentsChunkAllocatorConcept<WBE::HeapAllocatorRam>);
    ASSERT_TRUE(WBE::ComponentsChunkAllocatorConcept<WBE::HeapAllocatorFixedSizePool<4>>);

}
