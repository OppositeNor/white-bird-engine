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
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "function/render/global_render_cache.hh"
#include "function/render/render_defs.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>
#include <string>

namespace WhiteBirdEngine {

class GlobalRenderCacheTestRenderObject final : public IRenderObject {
public:
    GlobalRenderCacheTestRenderObject() = default;
    virtual ~GlobalRenderCacheTestRenderObject() override = default;
    GlobalRenderCacheTestRenderObject(const GlobalRenderCacheTestRenderObject&) = delete;
    GlobalRenderCacheTestRenderObject(GlobalRenderCacheTestRenderObject&&) noexcept = delete;
    GlobalRenderCacheTestRenderObject& operator=(const GlobalRenderCacheTestRenderObject&) = delete;
    GlobalRenderCacheTestRenderObject& operator=(GlobalRenderCacheTestRenderObject&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override;

    std::string applied_debug_name;

protected:
    virtual void apply_debug_name(std::string_view p_name) override {
        applied_debug_name = p_name;
    }
};

TypeID GlobalRenderCacheTestRenderObject::get_type_uuid() const {
    return static_hash("GlobalRenderCacheTestRenderObject");
}

class WBEGlobalRenderCacheTest : public ::testing::Test {
protected:
    RenderAllocator render_allocator{WBE_KI_B(128)};
    GlobalRenderCache render_cache{&render_allocator};
};

TEST_F(WBEGlobalRenderCacheTest, RegistersCompiledRenderResourceUsingConcreteObjectType) {
    Ref<GlobalRenderCacheTestRenderObject> render_object = make_ref<GlobalRenderCacheTestRenderObject>(&render_allocator);

    render_cache.insert("test_resource", render_object);

    EXPECT_TRUE(render_cache.contains("test_resource", render_object->get_type_uuid()));
    EXPECT_EQ(render_cache.try_get("test_resource", render_object->get_type_uuid()), render_object);
}

TEST_F(WBEGlobalRenderCacheTest, NamesRegisteredObjectsAfterTheirResourceKey) {
    Ref<GlobalRenderCacheTestRenderObject> object = make_ref<GlobalRenderCacheTestRenderObject>(&render_allocator);
    render_cache.insert("sponza#vk_usage_130", object);
    EXPECT_EQ(object->applied_debug_name, "sponza#vk_usage_130");
    EXPECT_EQ(object->get_debug_name(), "sponza#vk_usage_130");
}

TEST_F(WBEGlobalRenderCacheTest, HoldsObjectsWeakly) {
    Ref<GlobalRenderCacheTestRenderObject> object = make_ref<GlobalRenderCacheTestRenderObject>(&render_allocator);
    const TypeID type_id = object->get_type_uuid();
    render_cache.insert("weak_resource", object);
    object = MEM_NULL;
    EXPECT_FALSE(render_cache.contains("weak_resource", type_id));
    EXPECT_EQ(render_cache.try_get("weak_resource", type_id), MEM_NULL);
    EXPECT_EQ(render_cache.prune_expired(), 1U);
}

TEST_F(WBEGlobalRenderCacheTest, SeparatesEntriesByObjectType) {
    Ref<GlobalRenderCacheTestRenderObject> object = make_ref<GlobalRenderCacheTestRenderObject>(&render_allocator);
    render_cache.insert("typed_resource", object);
    EXPECT_FALSE(render_cache.contains("typed_resource", object->get_type_uuid() + 1));
}

TEST_F(WBEGlobalRenderCacheTest, RejectsEmptyKeysAndNullObjects) {
    Ref<GlobalRenderCacheTestRenderObject> object = make_ref<GlobalRenderCacheTestRenderObject>(&render_allocator);
    EXPECT_THROW(render_cache.insert("", object), std::runtime_error);
    EXPECT_THROW(render_cache.insert("null_resource", MEM_NULL), std::runtime_error);
}

} // namespace WhiteBirdEngine
