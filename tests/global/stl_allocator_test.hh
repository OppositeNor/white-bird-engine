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
    WBE::STLAllocator<int, WBE::HeapAllocatorAlignedPool> allocator(WBE::global_allocator());
    int* p = allocator.allocate(10);
    std::string expected = "{\"type\":\"HeapAllocatorAlignedPool\",\"obj_count\":1,\"free_chunk_layout\":[{\"begin\":40,\"size\":131032}]}";
    ASSERT_EQ(static_cast<std::string>(*(WBE::global_allocator())), expected);
    int* p1 = allocator.allocate(20);


    expected = "{\"type\":\"HeapAllocatorAlignedPool\",\"obj_count\":2,\"free_chunk_layout\":[{\"begin\":120,\"size\":130952}]}";
    ASSERT_EQ(static_cast<std::string>(*(WBE::global_allocator())), expected);
    allocator.deallocate(p, 10);
    expected = "{\"type\":\"HeapAllocatorAlignedPool\",\"obj_count\":1,\"free_chunk_layout\":[{\"begin\":0,\"size\":40},{\"begin\":120,\"size\":130952}]}";
    ASSERT_EQ(static_cast<std::string>(*(WBE::global_allocator())), expected);
    allocator.deallocate(p1, 20);
    expected = "{\"type\":\"HeapAllocatorAlignedPool\",\"obj_count\":0,\"free_chunk_layout\":[{\"begin\":0,\"size\":131072}]}";
    ASSERT_EQ(static_cast<std::string>(*(WBE::global_allocator())), expected);
}

#endif
