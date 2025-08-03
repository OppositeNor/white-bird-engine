#include "heap_allocator_pool_test.hh"
#include "heap_allocator_fixed_size_pool_test.hh"
#include "stack_allocator_test.hh"
#include "core/allocator/heap_allocator_ram.hh"
#include "core/ecs/ecs.hh"

TEST(AllocatorTest, TraitTest) {

    ASSERT_FALSE(WBE::ComponentsChunkAllocatorConcept<WBE::HeapAllocatorRAM>);
    ASSERT_TRUE(WBE::ComponentsChunkAllocatorConcept<WBE::HeapAllocatorFixedSizePool<4>>);

}
