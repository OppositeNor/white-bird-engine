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
#include "core/alloc/stl_alloc.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/i_bindable_rend_resource_vk.hh"
#include "function/render/i_rend_object.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/rend_graph_desc_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

namespace {

// A render object that imports itself through bind_rend_object() and records the names it is given.
class TestBindableRendObjectVK final : public IRendObject, public IBindableRendResourceVK {
public:
    TestBindableRendObjectVK() = default;
    virtual ~TestBindableRendObjectVK() override = default;
    TestBindableRendObjectVK(const TestBindableRendObjectVK&) = delete;
    TestBindableRendObjectVK(TestBindableRendObjectVK&&) noexcept = delete;
    TestBindableRendObjectVK& operator=(const TestBindableRendObjectVK&) = delete;
    TestBindableRendObjectVK& operator=(TestBindableRendObjectVK&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return static_hash("TestBindableRendObjectVK");
    }

    virtual void bind(RendGraphDescVK& p_graph, std::string_view p_resource_id) const override {
        bind_rend_object(p_graph, p_resource_id, acquire_this_ref());
    }

    uint32_t applied_name_count = 0;

protected:
    virtual void apply_debug_name(std::string_view /*p_name*/) override {
        ++applied_name_count;
    }
};

} // namespace

TEST(WBEBindableRendResourceVKTest, ImportNamesAnUnnamedObjectAfterItsResourceId) {
    RendAlloc alloc(WBE_KI_B(64));
    RendGraphDescVK graph(&alloc);
    Ref<TestBindableRendObjectVK> object = make_ref<TestBindableRendObjectVK>(&alloc);

    object->bind(graph, "svo_node_buffer");
    // A later import under another id keeps the first name, so a shared import is named the same in every graph.
    object->bind(graph, "other_id");

    EXPECT_EQ(object->get_debug_name(), "svo_node_buffer");
    EXPECT_EQ(object->applied_name_count, 1U);
}

TEST(WBEBindableRendResourceVKTest, ImportKeepsAnExistingName) {
    RendAlloc alloc(WBE_KI_B(64));
    RendGraphDescVK graph(&alloc);
    Ref<TestBindableRendObjectVK> object = make_ref<TestBindableRendObjectVK>(&alloc);
    object->set_debug_name("sponza#vk_usage_130");

    object->bind(graph, "mesh");

    EXPECT_EQ(object->get_debug_name(), "sponza#vk_usage_130");
    EXPECT_EQ(object->applied_name_count, 1U);
}

} // namespace WhiteBirdEngine
