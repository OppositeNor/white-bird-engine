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
#include "function/render/render_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/roc_per_frame_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>

namespace WhiteBirdEngine {

class TestContextResourceVK final : public RenderObjectVK {
public:
    TestContextResourceVK(const BaseRenderDataVK* p_render_data, uint32_t p_context_id)
        : RenderObjectVK(p_render_data), context_id(p_context_id) {
    }
    virtual ~TestContextResourceVK() override = default;
    TestContextResourceVK(const TestContextResourceVK&) = delete;
    TestContextResourceVK(TestContextResourceVK&&) noexcept = delete;
    TestContextResourceVK& operator=(const TestContextResourceVK&) = delete;
    TestContextResourceVK& operator=(TestContextResourceVK&&) noexcept = delete;

    TypeID get_type_uuid() const override;

    uint32_t context_id;
};

template <>
struct TypeUUID<TestContextResourceVK> {
    using Type = TestContextResourceVK;
    static constexpr TypeID UUID = static_hash("TestContextResourceVK");
};

TypeID TestContextResourceVK::get_type_uuid() const {
    return TypeUUID<TestContextResourceVK>::UUID;
}

TEST(WBEROCPerFrameVKTest, CreatesOneResourcePerFrameAndUsesCurrentFrameSlot) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 3;
    render_data.current_frame_slot = 1;

    ROCPerFrameVK context(&render_data, [&render_data, &allocator](uint32_t p_context_id) {
        return make_ref<TestContextResourceVK>(&allocator, &render_data, p_context_id).template dynamic_cast_ref<RenderObjectVK>();
    });

    EXPECT_EQ(context.get_resource_type_id(), TypeUUID<TestContextResourceVK>::UUID);
    EXPECT_EQ(context.get_context_count(), 3U);

    auto current_resource = context.get().dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(current_resource, MEM_NULL);
    EXPECT_EQ(current_resource->get_type_uuid(), TypeUUID<TestContextResourceVK>::UUID);
    EXPECT_EQ(current_resource->context_id, 1U);

    auto explicit_resource = context.get(2).dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(explicit_resource, MEM_NULL);
    EXPECT_EQ(explicit_resource->context_id, 2U);
}

TEST(WBEROCPerFrameVKTest, ThrowsWhenFactoryIsMissingOrReturnsNull) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 2;

    EXPECT_THROW(ROCPerFrameVK(&render_data, {}), std::runtime_error);
    EXPECT_THROW(ROCPerFrameVK(&render_data,
                     [](uint32_t /*p_context_id*/) {
                         return Ref<RenderObjectVK>(MEM_NULL);
                     }),
        std::runtime_error);
}

TEST(WBEROCPerFrameVKTest, UpdatesOneFrameSlotIndependently) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 3;
    render_data.current_frame_slot = 2;

    ROCPerFrameVK context(&render_data);
    auto frame_zero_resource =
        make_ref<TestContextResourceVK>(&allocator, &render_data, 0).template dynamic_cast_ref<RenderObjectVK>();
    auto frame_two_resource =
        make_ref<TestContextResourceVK>(&allocator, &render_data, 2).template dynamic_cast_ref<RenderObjectVK>();

    context.set(0, frame_zero_resource);
    context.set(2, frame_two_resource);

    auto current_resource = context.get().dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(current_resource, MEM_NULL);
    EXPECT_EQ(current_resource->context_id, 2U);

    auto explicit_resource = context.get(0).dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(explicit_resource, MEM_NULL);
    EXPECT_EQ(explicit_resource->context_id, 0U);

    EXPECT_EQ(context.get(1), MEM_NULL);
}

TEST(WBEROCPerFrameVKTest, RejectsEmptyPublishedBufferUpdate) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    ROCPerFrameVK context(&render_data);

    EXPECT_THROW(context.publish_buffer_update(nullptr, 0, 1), std::runtime_error);
    uint8_t value = 0;
    EXPECT_THROW(context.publish_buffer_update(&value, 0, 0), std::runtime_error);
}

TEST(WBEROCSingleVKTest, ReturnsStoredResource) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);

    auto resource = make_ref<TestContextResourceVK>(&allocator, &render_data, 7).template dynamic_cast_ref<RenderObjectVK>();
    ROCSingleVK container(resource);

    EXPECT_EQ(container.get_resource_type_id(), TypeUUID<TestContextResourceVK>::UUID);

    auto stored_resource = container.get().dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(stored_resource, MEM_NULL);
    EXPECT_EQ(stored_resource->context_id, 7U);
}

} // namespace WhiteBirdEngine
