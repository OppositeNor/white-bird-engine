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
#include "core/alloc/i_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/global_rend_cache.hh"
#include "function/render/rend_defs.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>
#include <string>

namespace WhiteBirdEngine {

class GlobalRendCacheTestRendObject final : public IRendObject {
public:
    GlobalRendCacheTestRendObject() = default;
    virtual ~GlobalRendCacheTestRendObject() override = default;
    GlobalRendCacheTestRendObject(const GlobalRendCacheTestRendObject&) = delete;
    GlobalRendCacheTestRendObject(GlobalRendCacheTestRendObject&&) noexcept = delete;
    GlobalRendCacheTestRendObject& operator=(const GlobalRendCacheTestRendObject&) = delete;
    GlobalRendCacheTestRendObject& operator=(GlobalRendCacheTestRendObject&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override;

    std::string applied_debug_name;

protected:
    virtual void apply_debug_name(std::string_view p_name) override {
        applied_debug_name = p_name;
    }
};

TypeID GlobalRendCacheTestRendObject::get_type_uuid() const {
    return static_hash("GlobalRendCacheTestRendObject");
}

class WBEGlobalRendCacheTest : public ::testing::Test {
protected:
    RendAlloc rend_alloc{WBE_KI_B(128)};
    GlobalRendCache rend_cache{&rend_alloc};
};

TEST_F(WBEGlobalRendCacheTest, RegistersCompiledRendResourceUsingConcreteObjectType) {
    Ref<GlobalRendCacheTestRendObject> rend_object = make_ref<GlobalRendCacheTestRendObject>(&rend_alloc);

    rend_cache.insert("test_resource", rend_object);

    EXPECT_TRUE(rend_cache.contains("test_resource", rend_object->get_type_uuid()));
    EXPECT_EQ(rend_cache.try_get("test_resource", rend_object->get_type_uuid()), rend_object);
}

TEST_F(WBEGlobalRendCacheTest, NamesRegisteredObjectsAfterTheirResourceKey) {
    Ref<GlobalRendCacheTestRendObject> object = make_ref<GlobalRendCacheTestRendObject>(&rend_alloc);
    rend_cache.insert("sponza#vk_usage_130", object);
    EXPECT_EQ(object->applied_debug_name, "sponza#vk_usage_130");
    EXPECT_EQ(object->get_debug_name(), "sponza#vk_usage_130");
}

TEST_F(WBEGlobalRendCacheTest, HoldsObjectsWeakly) {
    Ref<GlobalRendCacheTestRendObject> object = make_ref<GlobalRendCacheTestRendObject>(&rend_alloc);
    const TypeID type_id = object->get_type_uuid();
    rend_cache.insert("weak_resource", object);
    object = MEM_NULL;
    EXPECT_FALSE(rend_cache.contains("weak_resource", type_id));
    EXPECT_EQ(rend_cache.try_get("weak_resource", type_id), MEM_NULL);
    EXPECT_EQ(rend_cache.prune_expired(), 1U);
}

TEST_F(WBEGlobalRendCacheTest, SeparatesEntriesByObjectType) {
    Ref<GlobalRendCacheTestRendObject> object = make_ref<GlobalRendCacheTestRendObject>(&rend_alloc);
    rend_cache.insert("typed_resource", object);
    EXPECT_FALSE(rend_cache.contains("typed_resource", object->get_type_uuid() + 1));
}

TEST_F(WBEGlobalRendCacheTest, RejectsEmptyKeysAndNullObjects) {
    Ref<GlobalRendCacheTestRendObject> object = make_ref<GlobalRendCacheTestRendObject>(&rend_alloc);
    EXPECT_THROW(rend_cache.insert("", object), std::runtime_error);
    EXPECT_THROW(rend_cache.insert("null_resource", MEM_NULL), std::runtime_error);
}

} // namespace WhiteBirdEngine
