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
#include "fake_render_graph.hh"
#include "function/render/global_render_cache.hh"
#include "function/render/i_render_graph_bindings.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_graph_instance_cache.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>

namespace WhiteBirdEngine {

namespace {

class TestBindings final : public IRenderGraphBindings {
public:
    explicit TestBindings(std::string_view p_instance_key) : IRenderGraphBindings(p_instance_key) {
    }
};

class WBERenderGraphInstanceCacheTest : public ::testing::Test {
protected:
    Ref<FakeRenderGraph> acquire(std::string_view p_definition_key, const Ref<IRenderGraphBindings>& p_bindings) {
        return RenderGraphInstanceCache::acquire<FakeRenderGraph>(cache, p_definition_key, p_bindings, [this, &p_bindings] {
            ++compile_count;
            return make_ref<FakeRenderGraph>(&allocator, &allocator, p_bindings);
        });
    }

    RenderAllocator allocator{WBE_KI_B(64)};
    GlobalRenderCache cache{&allocator};
    uint32_t compile_count = 0;
};

} // namespace

TEST(WBERenderGraphBindingsTest, RequireANonEmptyInstanceKey) {
    EXPECT_THROW(TestBindings(""), std::runtime_error);
    EXPECT_EQ(TestBindings("main").get_instance_key(), "main");
}

TEST(WBERenderGraphInstanceCacheKeyTest, KeysAreUnambiguous) {
    EXPECT_NE(RenderGraphInstanceCache::make_cache_key("a:b", "c"), RenderGraphInstanceCache::make_cache_key("a", "b:c"));
    EXPECT_EQ(RenderGraphInstanceCache::make_cache_key("main", "left"), RenderGraphInstanceCache::make_cache_key("main", "left"));
}

TEST_F(WBERenderGraphInstanceCacheTest, ReusesTheLiveInstanceOfTheSameIdentityAndBindings) {
    Ref<IRenderGraphBindings> bindings = make_ref<TestBindings>(&allocator, "main");
    Ref<FakeRenderGraph> first = acquire("deferred", bindings);
    Ref<FakeRenderGraph> second = acquire("deferred", bindings);
    EXPECT_TRUE(first == second);
    EXPECT_EQ(compile_count, 1U);
    EXPECT_TRUE(first->get_bindings() == bindings);
    // The cache names the graph after its key.
    EXPECT_EQ(first->get_debug_name(), RenderGraphInstanceCache::make_cache_key("deferred", "main"));
}

TEST_F(WBERenderGraphInstanceCacheTest, IsolatesInstanceKeysAndDefinitions) {
    Ref<IRenderGraphBindings> left = make_ref<TestBindings>(&allocator, "left");
    Ref<IRenderGraphBindings> right = make_ref<TestBindings>(&allocator, "right");
    Ref<FakeRenderGraph> left_graph = acquire("deferred", left);
    Ref<FakeRenderGraph> right_graph = acquire("deferred", right);
    Ref<FakeRenderGraph> other_definition = acquire("forward", left);
    EXPECT_FALSE(left_graph == right_graph);
    EXPECT_FALSE(left_graph == other_definition);
    EXPECT_EQ(compile_count, 3U);
    // Registrations are state of each instance.
    EXPECT_FALSE(right_graph->contains_registered_resource("mesh"));
}

TEST_F(WBERenderGraphInstanceCacheTest, RejectsALiveIdentityAcquiredWithOtherBindings) {
    Ref<IRenderGraphBindings> bindings = make_ref<TestBindings>(&allocator, "main");
    Ref<IRenderGraphBindings> other = make_ref<TestBindings>(&allocator, "main");
    Ref<FakeRenderGraph> graph = acquire("deferred", bindings);
    EXPECT_THROW((void)acquire("deferred", other), std::runtime_error);
    EXPECT_EQ(compile_count, 1U);
    EXPECT_TRUE(acquire("deferred", bindings) == graph);
}

TEST_F(WBERenderGraphInstanceCacheTest, RecompilesExpiredInstances) {
    Ref<IRenderGraphBindings> bindings = make_ref<TestBindings>(&allocator, "main");
    acquire("deferred", bindings);
    // The cache holds instances weakly, so the released instance expired; other bindings may now take its identity.
    Ref<IRenderGraphBindings> other = make_ref<TestBindings>(&allocator, "main");
    Ref<FakeRenderGraph> graph = acquire("deferred", other);
    EXPECT_EQ(compile_count, 2U);
    EXPECT_TRUE(graph->get_bindings() == other);
}

TEST_F(WBERenderGraphInstanceCacheTest, FailedCompilationsAreNotCached) {
    Ref<IRenderGraphBindings> bindings = make_ref<TestBindings>(&allocator, "main");
    EXPECT_THROW((void)RenderGraphInstanceCache::acquire<FakeRenderGraph>(cache,
                     "deferred",
                     bindings,
                     []() -> Ref<FakeRenderGraph> {
                         throw std::runtime_error("compilation failed");
                     }),
        std::runtime_error);
    EXPECT_THROW((void)RenderGraphInstanceCache::acquire<FakeRenderGraph>(cache,
                     "deferred",
                     bindings,
                     [] {
                         return Ref<FakeRenderGraph>();
                     }),
        std::runtime_error);
    EXPECT_FALSE(cache.contains(RenderGraphInstanceCache::make_cache_key("deferred", "main"), TypeUUID<FakeRenderGraph>::UUID));
    Ref<FakeRenderGraph> graph = acquire("deferred", bindings);
    EXPECT_EQ(compile_count, 1U);
    EXPECT_TRUE(cache.try_get<FakeRenderGraph>(RenderGraphInstanceCache::make_cache_key("deferred", "main")) == graph);
}

TEST_F(WBERenderGraphInstanceCacheTest, RejectsEmptyDefinitionsAndNullBindings) {
    Ref<IRenderGraphBindings> bindings = make_ref<TestBindings>(&allocator, "main");
    EXPECT_THROW((void)acquire("", bindings), std::runtime_error);
    EXPECT_THROW((void)acquire("deferred", Ref<IRenderGraphBindings>()), std::runtime_error);
    EXPECT_EQ(compile_count, 0U);
}

TEST_F(WBERenderGraphInstanceCacheTest, GraphsAreCachedAsRenderObjects) {
    Ref<IRenderGraphBindings> bindings = make_ref<TestBindings>(&allocator, "main");
    Ref<FakeRenderGraph> graph = acquire("deferred", bindings);
    Ref<IRenderObject> object =
        cache.try_get(RenderGraphInstanceCache::make_cache_key("deferred", "main"), TypeUUID<FakeRenderGraph>::UUID);
    EXPECT_TRUE(object == graph);
    EXPECT_EQ(object->get_type_uuid(), TypeUUID<FakeRenderGraph>::UUID);
}

} // namespace WhiteBirdEngine
