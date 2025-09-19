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
#ifndef __WBE_STL_ALLOCATOR_TEST_HH__
#define __WBE_STL_ALLOCATOR_TEST_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "global/global.hh"
#include "global/stl_allocator.hh"
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace WBE = WhiteBirdEngine;

TEST(WBEStlAllocatorTest, Basic) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::STLAllocator<int, WBE::HeapAllocatorAlignedPool, false> allocator(WBE::global_allocator());
    std::string expected = "{\"type\":\"HeapAllocatorAlignedPool\",\"total_size\":131072,\"free_chunk_layout\":[{\"begin\":0,\"size\":131072}]}";
    ASSERT_EQ(static_cast<std::string>(*(WBE::global_allocator())), expected);
    int* p = allocator.allocate(10);
    expected = "{\"type\":\"HeapAllocatorAlignedPool\",\"total_size\":131072,\"free_chunk_layout\":[{\"begin\":48,\"size\":131024}]}";
    ASSERT_EQ(static_cast<std::string>(*(WBE::global_allocator())), expected);
    int* p1 = allocator.allocate(20);


    expected = "{\"type\":\"HeapAllocatorAlignedPool\",\"total_size\":131072,\"free_chunk_layout\":[{\"begin\":136,\"size\":130936}]}";
    ASSERT_EQ(static_cast<std::string>(*(WBE::global_allocator())), expected);
    allocator.deallocate(p, 10);
    expected = "{\"type\":\"HeapAllocatorAlignedPool\",\"total_size\":131072,\"free_chunk_layout\":[{\"begin\":0,\"size\":48},{\"begin\":136,\"size\":130936}]}";
    ASSERT_EQ(static_cast<std::string>(*(WBE::global_allocator())), expected);
    allocator.deallocate(p1, 20);
    expected = "{\"type\":\"HeapAllocatorAlignedPool\",\"total_size\":131072,\"free_chunk_layout\":[{\"begin\":0,\"size\":131072}]}";
    ASSERT_EQ(static_cast<std::string>(*(WBE::global_allocator())), expected);
}

#endif
