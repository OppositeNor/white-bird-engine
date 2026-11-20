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
#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/rend_resource_list_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <gtest/gtest.h>
#include <stdexcept>

namespace WhiteBirdEngine {

namespace {

class RendResourceListVKTestObject final : public IRendObject {
public:
    RendResourceListVKTestObject() = default;
    virtual ~RendResourceListVKTestObject() override = default;
    RendResourceListVKTestObject(const RendResourceListVKTestObject&) = delete;
    RendResourceListVKTestObject(RendResourceListVKTestObject&&) noexcept = delete;
    RendResourceListVKTestObject& operator=(const RendResourceListVKTestObject&) = delete;
    RendResourceListVKTestObject& operator=(RendResourceListVKTestObject&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return static_hash("RendResourceListVKTestObject");
    }
};

} // namespace

TEST(WBERendResourceListVKTest, RetainsResourcesInInsertionOrder) {
    RendAlloc alloc(WBE_KI_B(64));
    auto first = make_ref<RendResourceListVKTestObject>(&alloc).dynamic_cast_ref<IRendObject>();
    auto second = make_ref<RendResourceListVKTestObject>(&alloc).dynamic_cast_ref<IRendObject>();
    RendResourceListVK resources(&alloc);

    resources.add_resource(first).add_resource(second);

    ASSERT_EQ(resources.get_resource_count(), 2U);
    EXPECT_EQ(resources.acquire_resource(0), first);
    EXPECT_EQ(resources.acquire_resource(1), second);
}

TEST(WBERendResourceListVKTest, RejectsInvalidResourceAccess) {
    RendAlloc alloc(WBE_KI_B(64));
    RendResourceListVK resources(&alloc);

    EXPECT_THROW(resources.add_resource(MEM_NULL), std::runtime_error);
    EXPECT_THROW(resources.acquire_resource(0), std::runtime_error);
}

} // namespace WhiteBirdEngine
