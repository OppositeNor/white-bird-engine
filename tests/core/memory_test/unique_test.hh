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
#ifndef __WBE_UNIQUE_TEST_HH__
#define __WBE_UNIQUE_TEST_HH__

#include "core/memory/unique.hh"
#include "mock_heap_allocator_aligned.hh"
#include <gtest/gtest.h>
#include <stdexcept>

namespace WBE = WhiteBirdEngine;

struct Dummy {
    int x;
    Dummy(int v) : x(v) {}
    ~Dummy() { x = -1; }
};

TEST(WBEUniqueTest, BasicConstructionAndAccess) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    allocator.clear_call_log();
    
    // Create object using allocator directly
    WBE::MemID id = WBE::create_obj_align<Dummy>(allocator, 42);
    WBE::Unique<Dummy> u(&allocator, id);
    
    ASSERT_NE(u.get(), nullptr);
    ASSERT_EQ(u.get()->x, 42);
    
    // Verify get was called
    std::string log = allocator.get_call_log();
    ASSERT_NE(log.find("get"), std::string::npos);
}

TEST(WBEUniqueTest, MoveSemantics) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    
    WBE::MemID id1 = WBE::create_obj_align<Dummy>(allocator, 7);
    WBE::Unique<Dummy> u1(&allocator, id1);
    WBE::Unique<Dummy> u2 = std::move(u1);
    
    // After move, u1 should be null, u2 should have the value
    ASSERT_EQ(u1.get(), nullptr);
    ASSERT_NE(u2.get(), nullptr);
    ASSERT_EQ(u2.get()->x, 7);
}

TEST(WBEUniqueTest, ResetBehavior) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    allocator.clear_call_log();
    
    WBE::MemID id = WBE::create_obj_align<Dummy>(allocator, 123);
    WBE::Unique<Dummy> u(&allocator, id);
    ASSERT_NE(u.get(), nullptr);
    
    allocator.clear_call_log();
    u.reset();
    
    // Reset should call deallocate and set to null
    std::string log = allocator.get_call_log();
    ASSERT_NE(log.find("deallocate"), std::string::npos);
    ASSERT_EQ(u.get(), nullptr);
}

TEST(WBEUniqueTest, ComparisonOperators) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    
    WBE::MemID id = WBE::create_obj_align<Dummy>(allocator, 1);
    WBE::Unique<Dummy> u1(&allocator, id);
    WBE::Unique<Dummy> u2; // Empty unique
    
    ASSERT_FALSE(u1 == nullptr);
    ASSERT_TRUE(u2 == nullptr);
    
    u1.reset();
    ASSERT_TRUE(u1 == nullptr);
    
    // Test exception throwing for invalid comparisons
    bool _;
    ASSERT_THROW(_ = u2 == (void*)0x1234, std::runtime_error);
    ASSERT_THROW(_ = u1 == WBE::MemID(42), std::runtime_error);
}

TEST(WBEUniqueTest, CallLoggingWithMakeUnique) {
    WBE::MockHeapAllocatorAligned allocator(4096);
    allocator.clear_call_log();
    
    // Test static make_unique method
    auto u = WBE::Unique<Dummy>::make_unique(&allocator, 100);
    ASSERT_NE(u.get(), nullptr);
    ASSERT_EQ(u.get()->x, 100);
    
    // Verify allocation and get were called
    std::string log = allocator.get_call_log();
    ASSERT_NE(log.find("allocate"), std::string::npos);
    ASSERT_NE(log.find("get"), std::string::npos);
}

#endif
