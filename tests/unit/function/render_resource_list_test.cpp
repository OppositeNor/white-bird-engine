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
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <gtest/gtest.h>
#include <stdexcept>

namespace WhiteBirdEngine {

namespace {

class RenderResourceListTestObject final : public IRenderObject {
public:
    RenderResourceListTestObject() = default;
    ~RenderResourceListTestObject() override = default;
    RenderResourceListTestObject(const RenderResourceListTestObject&) = delete;
    RenderResourceListTestObject(RenderResourceListTestObject&&) noexcept = delete;
    RenderResourceListTestObject& operator=(const RenderResourceListTestObject&) = delete;
    RenderResourceListTestObject& operator=(RenderResourceListTestObject&&) noexcept = delete;

    TypeID get_type_uuid() const override {
        return static_hash("RenderResourceListTestObject");
    }
};

} // namespace

TEST(WBERenderResourceListTest, RetainsResourcesInInsertionOrder) {
    RenderAllocator allocator(WBE_KI_B(64));
    auto first = make_ref<RenderResourceListTestObject>(&allocator).dynamic_cast_ref<IRenderObject>();
    auto second = make_ref<RenderResourceListTestObject>(&allocator).dynamic_cast_ref<IRenderObject>();
    RenderResourceList resources(&allocator);

    resources.add_resource(first).add_resource(second);

    ASSERT_EQ(resources.get_resource_count(), 2U);
    EXPECT_EQ(resources.acquire_resource(0), first);
    EXPECT_EQ(resources.acquire_resource(1), second);
}

TEST(WBERenderResourceListTest, RejectsInvalidResourceAccess) {
    RenderAllocator allocator(WBE_KI_B(64));
    RenderResourceList resources(&allocator);

    EXPECT_THROW(resources.add_resource(MEM_NULL), std::runtime_error);
    EXPECT_THROW(resources.acquire_resource(0), std::runtime_error);
}

} // namespace WhiteBirdEngine
