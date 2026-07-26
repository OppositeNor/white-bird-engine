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
#ifndef WBE_FILE_MEMORY_CHUNK_TEST_HH
#define WBE_FILE_MEMORY_CHUNK_TEST_HH

#include "core/allocator/heap_allocator_atomic_mutex_aligned_pool_impl_list.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/memory_chunk.hh"
#include "utils/defs.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>

namespace WBE = WhiteBirdEngine;

TEST(WBEMemoryChunkTest, CopyMoveAndAssignmentTrackReferences) {
    WBE::MemoryChunk chunk(WBE_KI_B(1));
    ASSERT_TRUE(chunk.is_valid());
    ASSERT_EQ(chunk.get_size(), WBE_KI_B(1));
    ASSERT_EQ(chunk.get_reference_count(), 1);

    WBE::MemoryChunk copied(chunk);
    ASSERT_EQ(chunk.get_reference_count(), 2);
    ASSERT_EQ(copied.get_reference_count(), 2);
    ASSERT_EQ(chunk.get(), copied.get());

    WBE::MemoryChunk copy_assigned;
    copy_assigned = chunk;
    ASSERT_EQ(chunk.get_reference_count(), 3);
    ASSERT_EQ(copy_assigned.get(), chunk.get());

    WBE::MemoryChunk moved(std::move(copied));
    ASSERT_FALSE(copied.is_valid());
    ASSERT_EQ(chunk.get_reference_count(), 3);
    ASSERT_EQ(moved.get(), chunk.get());

    WBE::MemoryChunk move_assigned;
    move_assigned = std::move(copy_assigned);
    ASSERT_FALSE(copy_assigned.is_valid());
    ASSERT_EQ(chunk.get_reference_count(), 3);
    ASSERT_EQ(move_assigned.get(), chunk.get());
}

TEST(WBEMemoryChunkTest, ValidatesOccupiedRanges) {
    WBE::MemoryChunk chunk(WBE_KI_B(1));

    ASSERT_EQ(chunk.get_occupied_start(0, 128), chunk.get());
    ASSERT_EQ(chunk.get_occupied_start(128, 128), chunk.get() + 128);
    ASSERT_THROW(chunk.get_occupied_start(WBE_KI_B(1), 1), std::runtime_error);
    ASSERT_THROW(chunk.get_occupied_start(WBE_KI_B(1) + 1, 0), std::runtime_error);
}

TEST(WBEMemoryChunkTest, AllocatorsCanOccupyDifferentRangesInOneChunk) {
    WBE::MemoryChunk chunk(WBE_KI_B(2));
    ASSERT_EQ(chunk.get_reference_count(), 1);

    {
        WBE::HeapAllocatorAtomicMutexAlignedPoolImplicitList first_pool(chunk, 0, WBE_KI_B(1));
        WBE::HeapAllocatorAtomicMutexAlignedPoolImplicitList second_pool(chunk, WBE_KI_B(1), WBE_KI_B(1));
        ASSERT_EQ(chunk.get_reference_count(), 3);

        WBE::MemID first_mem = first_pool.allocate(64);
        WBE::MemID second_mem = second_pool.allocate(64);
        ASSERT_GE(first_mem, reinterpret_cast<WBE::MemID>(chunk.get()));
        ASSERT_LT(first_mem, reinterpret_cast<WBE::MemID>(chunk.get() + WBE_KI_B(1)));
        ASSERT_GE(second_mem, reinterpret_cast<WBE::MemID>(chunk.get() + WBE_KI_B(1)));
        ASSERT_LT(second_mem, reinterpret_cast<WBE::MemID>(chunk.get() + WBE_KI_B(2)));

        first_pool.deallocate(first_mem);
        second_pool.deallocate(second_mem);
    }

    ASSERT_EQ(chunk.get_reference_count(), 1);
}

#endif
