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
#include "core/memory/reference_strong.hh"
#include "fake_rend_graph.hh"
#include "function/render/global_rend_cache.hh"
#include "function/render/i_rend_graph_bindings.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/rend_defs.hh"
#include "function/render/rend_graph_instance_cache.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>

namespace WhiteBirdEngine {

namespace {

class TestBindings final : public IRendGraphBindings {
public:
    explicit TestBindings(std::string_view p_instance_key) : IRendGraphBindings(p_instance_key) {
    }
};

class WBERendGraphInstanceCacheTest : public ::testing::Test {
protected:
    Ref<FakeRendGraph> acquire(std::string_view p_definition_key, const Ref<IRendGraphBindings>& p_bindings) {
        return RendGraphInstanceCache::acquire<FakeRendGraph>(cache, p_definition_key, p_bindings, [this, &p_bindings] {
            ++compile_count;
            return make_ref<FakeRendGraph>(&alloc, &alloc, p_bindings);
        });
    }

    RendAlloc alloc{WBE_KI_B(64)};
    GlobalRendCache cache{&alloc};
    uint32_t compile_count = 0;
};

} // namespace

TEST(WBERendGraphBindingsTest, RequireANonEmptyInstanceKey) {
    EXPECT_THROW(TestBindings(""), std::runtime_error);
    EXPECT_EQ(TestBindings("main").get_instance_key(), "main");
}

TEST(WBERendGraphInstanceCacheKeyTest, KeysAreUnambiguous) {
    EXPECT_NE(RendGraphInstanceCache::make_cache_key("a:b", "c"), RendGraphInstanceCache::make_cache_key("a", "b:c"));
    EXPECT_EQ(RendGraphInstanceCache::make_cache_key("main", "left"), RendGraphInstanceCache::make_cache_key("main", "left"));
}

TEST_F(WBERendGraphInstanceCacheTest, ReusesTheLiveInstanceOfTheSameIdentityAndBindings) {
    Ref<IRendGraphBindings> bindings = make_ref<TestBindings>(&alloc, "main");
    Ref<FakeRendGraph> first = acquire("deferred", bindings);
    Ref<FakeRendGraph> second = acquire("deferred", bindings);
    EXPECT_TRUE(first == second);
    EXPECT_EQ(compile_count, 1U);
    EXPECT_TRUE(first->get_bindings() == bindings);
    // The cache names the graph after its key.
    EXPECT_EQ(first->get_debug_name(), RendGraphInstanceCache::make_cache_key("deferred", "main"));
}

TEST_F(WBERendGraphInstanceCacheTest, IsolatesInstanceKeysAndDefinitions) {
    Ref<IRendGraphBindings> left = make_ref<TestBindings>(&alloc, "left");
    Ref<IRendGraphBindings> right = make_ref<TestBindings>(&alloc, "right");
    Ref<FakeRendGraph> left_graph = acquire("deferred", left);
    Ref<FakeRendGraph> right_graph = acquire("deferred", right);
    Ref<FakeRendGraph> other_definition = acquire("forward", left);
    EXPECT_FALSE(left_graph == right_graph);
    EXPECT_FALSE(left_graph == other_definition);
    EXPECT_EQ(compile_count, 3U);
    // Registrations are state of each instance.
    EXPECT_FALSE(right_graph->contains_registered_resource("mesh"));
}

TEST_F(WBERendGraphInstanceCacheTest, RejectsALiveIdentityAcquiredWithOtherBindings) {
    Ref<IRendGraphBindings> bindings = make_ref<TestBindings>(&alloc, "main");
    Ref<IRendGraphBindings> other = make_ref<TestBindings>(&alloc, "main");
    Ref<FakeRendGraph> graph = acquire("deferred", bindings);
    EXPECT_THROW((void)acquire("deferred", other), std::runtime_error);
    EXPECT_EQ(compile_count, 1U);
    EXPECT_TRUE(acquire("deferred", bindings) == graph);
}

TEST_F(WBERendGraphInstanceCacheTest, RecompilesExpiredInstances) {
    Ref<IRendGraphBindings> bindings = make_ref<TestBindings>(&alloc, "main");
    acquire("deferred", bindings);
    // The cache holds instances weakly, so the released instance expired; other bindings may now take its identity.
    Ref<IRendGraphBindings> other = make_ref<TestBindings>(&alloc, "main");
    Ref<FakeRendGraph> graph = acquire("deferred", other);
    EXPECT_EQ(compile_count, 2U);
    EXPECT_TRUE(graph->get_bindings() == other);
}

TEST_F(WBERendGraphInstanceCacheTest, FailedCompilationsAreNotCached) {
    Ref<IRendGraphBindings> bindings = make_ref<TestBindings>(&alloc, "main");
    EXPECT_THROW((void)RendGraphInstanceCache::acquire<FakeRendGraph>(cache,
                     "deferred",
                     bindings,
                     []() -> Ref<FakeRendGraph> {
                         throw std::runtime_error("compilation failed");
                     }),
        std::runtime_error);
    EXPECT_THROW((void)RendGraphInstanceCache::acquire<FakeRendGraph>(cache,
                     "deferred",
                     bindings,
                     [] {
                         return Ref<FakeRendGraph>();
                     }),
        std::runtime_error);
    EXPECT_FALSE(cache.contains(RendGraphInstanceCache::make_cache_key("deferred", "main"), TypeUUID<FakeRendGraph>::UUID));
    Ref<FakeRendGraph> graph = acquire("deferred", bindings);
    EXPECT_EQ(compile_count, 1U);
    EXPECT_TRUE(cache.try_get<FakeRendGraph>(RendGraphInstanceCache::make_cache_key("deferred", "main")) == graph);
}

TEST_F(WBERendGraphInstanceCacheTest, RejectsEmptyDefinitionsAndNullBindings) {
    Ref<IRendGraphBindings> bindings = make_ref<TestBindings>(&alloc, "main");
    EXPECT_THROW((void)acquire("", bindings), std::runtime_error);
    EXPECT_THROW((void)acquire("deferred", Ref<IRendGraphBindings>()), std::runtime_error);
    EXPECT_EQ(compile_count, 0U);
}

TEST_F(WBERendGraphInstanceCacheTest, GraphsAreCachedAsRendObjects) {
    Ref<IRendGraphBindings> bindings = make_ref<TestBindings>(&alloc, "main");
    Ref<FakeRendGraph> graph = acquire("deferred", bindings);
    Ref<IRendObject> object =
        cache.try_get(RendGraphInstanceCache::make_cache_key("deferred", "main"), TypeUUID<FakeRendGraph>::UUID);
    EXPECT_TRUE(object == graph);
    EXPECT_EQ(object->get_type_uuid(), TypeUUID<FakeRendGraph>::UUID);
}

} // namespace WhiteBirdEngine
