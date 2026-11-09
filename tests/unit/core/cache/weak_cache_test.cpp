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
#include "core/allocator/heap_allocator_ram.hh"
#include "core/cache/weak_cache.hh"
#include "core/memory/reference_strong.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

namespace {

using StringWeakCache = WeakCache<std::string, int, HeapAllocatorRAM, HeapAllocatorRAM>;

} // namespace

class WBEWeakCacheTest : public ::testing::Test {
protected:
    HeapAllocatorRAM allocator;
};

TEST_F(WBEWeakCacheTest, ReturnsLiveObjectsAndHeterogeneousLookups) {
    StringWeakCache cache(&allocator);
    Ref<int, HeapAllocatorRAM> value = make_ref<int>(&allocator, 7);
    cache.insert("seven", value);
    EXPECT_TRUE(cache.contains(std::string_view("seven")));
    EXPECT_EQ(cache.try_get(std::string_view("seven")), value);
    EXPECT_EQ(*cache.get(std::string("seven")), 7);
}

TEST_F(WBEWeakCacheTest, DoesNotKeepObjectsAlive) {
    StringWeakCache cache(&allocator);
    Ref<int, HeapAllocatorRAM> value = make_ref<int>(&allocator, 1);
    cache.insert("one", value);
    value = MEM_NULL;
    EXPECT_FALSE(cache.contains(std::string_view("one")));
    EXPECT_EQ(cache.try_get(std::string_view("one")), MEM_NULL);
    EXPECT_THROW((void)cache.get(std::string_view("one")), std::runtime_error);
    EXPECT_EQ(cache.size(), 1U);
    EXPECT_EQ(cache.prune_expired(), 1U);
    EXPECT_EQ(cache.size(), 0U);
}

TEST_F(WBEWeakCacheTest, AcquireCreatesOnlyOnMiss) {
    StringWeakCache cache(&allocator);
    int created = 0;
    const auto factory = [this, &created] {
        ++created;
        return make_ref<int>(&allocator, 3);
    };
    Ref<int, HeapAllocatorRAM> first = cache.acquire("three", factory);
    Ref<int, HeapAllocatorRAM> second = cache.acquire("three", factory);
    EXPECT_EQ(first, second);
    EXPECT_EQ(created, 1);
    first = MEM_NULL;
    second = MEM_NULL;
    Ref<int, HeapAllocatorRAM> third = cache.acquire("three", factory);
    EXPECT_EQ(created, 2);
    EXPECT_EQ(*third, 3);
}

TEST_F(WBEWeakCacheTest, RejectsNullValuesAndFactories) {
    StringWeakCache cache(&allocator);
    EXPECT_THROW(cache.insert("null", MEM_NULL), std::runtime_error);
    EXPECT_THROW((void)cache.acquire("null",
                     [] {
                         return Ref<int, HeapAllocatorRAM>();
                     }),
        std::runtime_error);
}

TEST_F(WBEWeakCacheTest, InsertReplacesAndEraseRemoves) {
    StringWeakCache cache(&allocator);
    Ref<int, HeapAllocatorRAM> first = make_ref<int>(&allocator, 1);
    Ref<int, HeapAllocatorRAM> second = make_ref<int>(&allocator, 2);
    cache.insert("value", first);
    cache.insert("value", second);
    EXPECT_EQ(cache.try_get(std::string_view("value")), second);
    EXPECT_TRUE(cache.erase(std::string_view("value")));
    EXPECT_FALSE(cache.erase(std::string_view("value")));
    EXPECT_FALSE(cache.contains(std::string_view("value")));
}

TEST_F(WBEWeakCacheTest, MoveLeavesSourceEmpty) {
    StringWeakCache cache(&allocator);
    Ref<int, HeapAllocatorRAM> value = make_ref<int>(&allocator, 5);
    cache.insert("five", value);
    StringWeakCache moved(std::move(cache));
    EXPECT_EQ(moved.try_get(std::string_view("five")), value);
    EXPECT_EQ(cache.size(), 0U); // NOLINT(bugprone-use-after-move)
}

TEST_F(WBEWeakCacheTest, RejectsNullAllocator) {
    EXPECT_THROW(StringWeakCache(nullptr), std::runtime_error);
}

} // namespace WhiteBirdEngine
