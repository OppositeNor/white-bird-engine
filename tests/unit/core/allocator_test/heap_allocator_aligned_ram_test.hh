/* Copyright 2026 OppositeNor

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
#ifndef WBE_FILE_HEAP_ALLOCATOR_ALIGNED_RAM_TEST_HH
#define WBE_FILE_HEAP_ALLOCATOR_ALIGNED_RAM_TEST_HH

#include "core/allocator/heap_allocator_aligned_ram.hh"
#include "core/allocator/i_allocator.hh"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <string>

namespace WBE = WhiteBirdEngine;

TEST(WBEHeapAllocatorAlignedRAMTest, ZeroSizeReturnsNull) {
    WBE::HeapAllocatorAlignedRAM allocator;

    EXPECT_EQ(allocator.allocate(0), WBE::MEM_NULL);
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEHeapAllocatorAlignedRAMTest, AllocationsHonorAlignmentAndRoundUpSize) {
    WBE::HeapAllocatorAlignedRAM allocator;

    for (size_t alignment : {8, 16, 64, 256, 4096}) {
        WBE::MemID memory = allocator.allocate(3, alignment);
        ASSERT_NE(memory, WBE::MEM_NULL);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(allocator.get(memory)) % alignment, 0U) << alignment;
        EXPECT_EQ(allocator.get_allocated_data_size(memory), alignment) << alignment;
        allocator.deallocate(memory);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEHeapAllocatorAlignedRAMTest, TracksLiveAllocations) {
    WBE::HeapAllocatorAlignedRAM allocator;
    WBE::MemID first = allocator.allocate(32, 16);
    WBE::MemID second = allocator.allocate(100, 32);

    EXPECT_EQ(allocator.obj_count(), 2U);
    EXPECT_FALSE(allocator.is_empty());
    EXPECT_TRUE(allocator.get_allocated().contains(first));
    EXPECT_TRUE(allocator.get_allocated().contains(second));
    EXPECT_EQ(allocator.get_allocated_data_size(second), 128U);

    allocator.deallocate(first);
    EXPECT_EQ(allocator.obj_count(), 1U);
    EXPECT_FALSE(allocator.get_allocated().contains(first));
    allocator.deallocate(second);
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEHeapAllocatorAlignedRAMTest, MemoryIsWritable) {
    WBE::HeapAllocatorAlignedRAM allocator;
    WBE::MemID memory = allocator.allocate(64, 64);
    auto* bytes = static_cast<uint8_t*>(allocator.get(memory));

    std::memset(bytes, 0xAB, 64);
    EXPECT_EQ(bytes[0], 0xAB);
    EXPECT_EQ(bytes[63], 0xAB);
    allocator.deallocate(memory);
}

TEST(WBEHeapAllocatorAlignedRAMTest, ToStringListsAllocations) {
    WBE::HeapAllocatorAlignedRAM allocator;
    EXPECT_EQ(static_cast<std::string>(allocator), R"({"type":"HeapAllocatorAlignedRAM","obj_count":0,"allocated":[]})");

    WBE::MemID memory = allocator.allocate(10, 16);
    const std::string expected = std::string(R"({"type":"HeapAllocatorAlignedRAM","obj_count":1,"allocated":[{"mem_id":)") +
                                 std::to_string(memory) + R"(,"size":16}]})";
    EXPECT_EQ(static_cast<std::string>(allocator), expected);
    allocator.deallocate(memory);
}

#endif
