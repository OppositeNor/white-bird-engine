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
#ifndef WBE_FILE_HEAP_ALLOC_ALIGNED_RAM_TEST_HH
#define WBE_FILE_HEAP_ALLOC_ALIGNED_RAM_TEST_HH

#include "core/alloc/heap_alloc_aligned_ram.hh"
#include "core/alloc/i_alloc.hh"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <string>

namespace WBE = WhiteBirdEngine;

TEST(WBEHeapAllocAlignedRAMTest, ZeroSizeReturnsNull) {
    WBE::HeapAllocAlignedRAM alloc;

    EXPECT_EQ(alloc.allocate(0), WBE::MEM_NULL);
    EXPECT_TRUE(alloc.is_empty());
}

TEST(WBEHeapAllocAlignedRAMTest, AllocationsHonorAlignmentAndRoundUpSize) {
    WBE::HeapAllocAlignedRAM alloc;

    for (size_t alignment : {8, 16, 64, 256, 4096}) {
        WBE::MemID memory = alloc.allocate(3, alignment);
        ASSERT_NE(memory, WBE::MEM_NULL);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(alloc.get(memory)) % alignment, 0U) << alignment;
        EXPECT_EQ(alloc.get_allocated_data_size(memory), alignment) << alignment;
        alloc.deallocate(memory);
    }
    EXPECT_TRUE(alloc.is_empty());
}

TEST(WBEHeapAllocAlignedRAMTest, TracksLiveAllocations) {
    WBE::HeapAllocAlignedRAM alloc;
    WBE::MemID first = alloc.allocate(32, 16);
    WBE::MemID second = alloc.allocate(100, 32);

    EXPECT_EQ(alloc.obj_count(), 2U);
    EXPECT_FALSE(alloc.is_empty());
    EXPECT_TRUE(alloc.get_allocated().contains(first));
    EXPECT_TRUE(alloc.get_allocated().contains(second));
    EXPECT_EQ(alloc.get_allocated_data_size(second), 128U);

    alloc.deallocate(first);
    EXPECT_EQ(alloc.obj_count(), 1U);
    EXPECT_FALSE(alloc.get_allocated().contains(first));
    alloc.deallocate(second);
    EXPECT_TRUE(alloc.is_empty());
}

TEST(WBEHeapAllocAlignedRAMTest, MemoryIsWritable) {
    WBE::HeapAllocAlignedRAM alloc;
    WBE::MemID memory = alloc.allocate(64, 64);
    auto* bytes = static_cast<uint8_t*>(alloc.get(memory));

    std::memset(bytes, 0xAB, 64);
    EXPECT_EQ(bytes[0], 0xAB);
    EXPECT_EQ(bytes[63], 0xAB);
    alloc.deallocate(memory);
}

TEST(WBEHeapAllocAlignedRAMTest, ToStringListsAllocations) {
    WBE::HeapAllocAlignedRAM alloc;
    EXPECT_EQ(static_cast<std::string>(alloc), R"({"type":"HeapAllocAlignedRAM","obj_count":0,"allocated":[]})");

    WBE::MemID memory = alloc.allocate(10, 16);
    const std::string expected = std::string(R"({"type":"HeapAllocAlignedRAM","obj_count":1,"allocated":[{"mem_id":)") +
                                 std::to_string(memory) + R"(,"size":16}]})";
    EXPECT_EQ(static_cast<std::string>(alloc), expected);
    alloc.deallocate(memory);
}

#endif
