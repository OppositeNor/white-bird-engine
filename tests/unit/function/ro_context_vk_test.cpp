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
#include "core/parser/parser_json.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/roc_per_frame_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

namespace {

DebugDataArray parse_json_array(std::string_view p_json) {
    ParserJSON parser;
    parser.parse_from_buffer(p_json);
    return parser.get<DebugDataArray>();
}

DebugData make_descriptor_row(
    std::string_view p_resource_type, std::string_view p_descriptor_type, std::string_view p_heap, uint32_t p_heap_index, bool p_has_address) {
    DebugData row;
    row.set_value("resource_type", std::string(p_resource_type));
    row.set_value("descriptor_type", std::string(p_descriptor_type));
    row.set_value("heap", std::string(p_heap));
    row.set_value("heap_index", p_heap_index);
    if (p_has_address) {
        row.set_value("device_address", "0x1234");
    } else {
        row.set_value("device_address", nullptr);
    }
    return row;
}

} // namespace

class TestContextResourceVK final : public IRenderObjectVK {
public:
    TestContextResourceVK(const BaseRenderDataVK* p_render_data, uint32_t p_context_id)
        : IRenderObjectVK(p_render_data), context_id(p_context_id) {
    }
    virtual ~TestContextResourceVK() override = default;
    TestContextResourceVK(const TestContextResourceVK&) = delete;
    TestContextResourceVK(TestContextResourceVK&&) noexcept = delete;
    TestContextResourceVK& operator=(const TestContextResourceVK&) = delete;
    TestContextResourceVK& operator=(TestContextResourceVK&&) noexcept = delete;

    TypeID get_type_uuid() const override;

    uint32_t context_id;
};

class TestDescriptorResourceVK final : public IRenderObjectVK {
public:
    TestDescriptorResourceVK(const BaseRenderDataVK* p_render_data, DebugDataArray p_rows)
        : IRenderObjectVK(p_render_data), rows(p_render_data->allocator) {
        rows.reserve(p_rows.size());
        for (DebugData& row : p_rows) {
            rows.emplace_back(std::move(row));
        }
    }
    virtual ~TestDescriptorResourceVK() override = default;
    TestDescriptorResourceVK(const TestDescriptorResourceVK&) = delete;
    TestDescriptorResourceVK(TestDescriptorResourceVK&&) noexcept = delete;
    TestDescriptorResourceVK& operator=(const TestDescriptorResourceVK&) = delete;
    TestDescriptorResourceVK& operator=(TestDescriptorResourceVK&&) noexcept = delete;

    TypeID get_type_uuid() const override;

    DebugDataRows descriptor_debug_data() const override {
        return rows;
    }

private:
    DebugDataRows rows;
};

template <>
struct TypeUUID<TestContextResourceVK> {
    using Type = TestContextResourceVK;
    static constexpr TypeID UUID = static_hash("TestContextResourceVK");
};

template <>
struct TypeUUID<TestDescriptorResourceVK> {
    using Type = TestDescriptorResourceVK;
    static constexpr TypeID UUID = static_hash("TestDescriptorResourceVK");
};

TypeID TestContextResourceVK::get_type_uuid() const {
    return TypeUUID<TestContextResourceVK>::UUID;
}

TypeID TestDescriptorResourceVK::get_type_uuid() const {
    return TypeUUID<TestDescriptorResourceVK>::UUID;
}

TEST(WBEROCPerFrameVKTest, CreatesOneResourcePerFrameAndUsesCurrentFrameSlot) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 3;
    render_data.current_frame_slot = 1;

    ROCPerFrameVK context(&render_data, ROCPerFrameVK::Factory{[&render_data, &allocator](uint32_t p_context_id) {
        return make_ref<TestContextResourceVK>(&allocator, &render_data, p_context_id).template dynamic_cast_ref<IRenderObjectVK>();
    }});

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

    EXPECT_THROW(ROCPerFrameVK(&render_data, ROCPerFrameVK::Factory{}), std::runtime_error);
    EXPECT_THROW(ROCPerFrameVK(&render_data, ROCPerFrameVK::Factory{[](uint32_t /*p_context_id*/) {
        return Ref<IRenderObjectVK>(MEM_NULL);
    }}),
        std::runtime_error);
}

TEST(WBEROCPerFrameVKTest, UpdatesOneFrameSlotIndependently) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 3;
    render_data.current_frame_slot = 2;

    ROCPerFrameVK context(&render_data);
    auto frame_zero_resource =
        make_ref<TestContextResourceVK>(&allocator, &render_data, 0).template dynamic_cast_ref<IRenderObjectVK>();
    auto frame_two_resource =
        make_ref<TestContextResourceVK>(&allocator, &render_data, 2).template dynamic_cast_ref<IRenderObjectVK>();

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

TEST(WBEROCPerFrameVKTest, NoFlushModeStillSupportsPerSlotReplacement) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.current_frame_slot = 1;

    ROCPerFrameVK context(&render_data, false);
    auto frame_zero_resource =
        make_ref<TestContextResourceVK>(&allocator, &render_data, 10).template dynamic_cast_ref<IRenderObjectVK>();
    auto frame_one_resource =
        make_ref<TestContextResourceVK>(&allocator, &render_data, 11).template dynamic_cast_ref<IRenderObjectVK>();
    auto replacement_resource =
        make_ref<TestContextResourceVK>(&allocator, &render_data, 42).template dynamic_cast_ref<IRenderObjectVK>();

    context.set(0, frame_zero_resource);
    context.set(1, frame_one_resource);
    context.set(1, replacement_resource);

    auto current_resource = context.get().dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(current_resource, MEM_NULL);
    EXPECT_EQ(current_resource->context_id, 42U);

    auto other_resource = context.get(0).dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(other_resource, MEM_NULL);
    EXPECT_EQ(other_resource->context_id, 10U);
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

    auto resource = make_ref<TestContextResourceVK>(&allocator, &render_data, 7).template dynamic_cast_ref<IRenderObjectVK>();
    ROCSingleVK container(resource);

    EXPECT_EQ(container.get_resource_type_id(), TypeUUID<TestContextResourceVK>::UUID);

    auto stored_resource = container.get().dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(stored_resource, MEM_NULL);
    EXPECT_EQ(stored_resource->context_id, 7U);
}

TEST(WBEROCSingleVKTest, ToStringOmitsFrameIndexAndPreservesMultipleDescriptors) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    auto resource = make_ref<TestDescriptorResourceVK>(&allocator,
        &render_data,
        DebugDataArray{make_descriptor_row("ROBufferVK", "uniform_buffer", "resource", 13, true),
            make_descriptor_row("ROImageViewVK", "sampled_image", "resource", 17, false)})
                        .template dynamic_cast_ref<IRenderObjectVK>();

    ROCSingleVK container(resource);
    DebugDataArray rows = parse_json_array(container.to_string());

    ASSERT_EQ(rows.size(), 2U);
    EXPECT_FALSE(rows[0].contains("frame_index"));
    EXPECT_EQ(rows[0].get_value<std::string>("resource_type"), "ROBufferVK");
    EXPECT_EQ(rows[0].get_value<std::string>("descriptor_type"), "uniform_buffer");
    EXPECT_EQ(rows[0].get_value<std::string>("device_address"), "0x1234");
    EXPECT_EQ(rows[1].get_value<std::string>("resource_type"), "ROImageViewVK");
    EXPECT_EQ(rows[1].get_value<std::string>("descriptor_type"), "sampled_image");
    EXPECT_FALSE(rows[1].contains("frame_index"));
    EXPECT_EQ(rows[1].get_value<std::nullptr_t>("device_address"), nullptr);
}

TEST(WBEROCPerFrameVKTest, ToStringAddsFrameIndexAndSkipsEmptyResources) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 3;

    ROCPerFrameVK container(&render_data);
    container.set(0,
        make_ref<TestDescriptorResourceVK>(
            &allocator, &render_data, DebugDataArray{make_descriptor_row("ROBufferVK", "uniform_buffer", "resource", 4, true)})
            .template dynamic_cast_ref<IRenderObjectVK>());
    container.set(2,
        make_ref<TestDescriptorResourceVK>(
            &allocator, &render_data, DebugDataArray{make_descriptor_row("ROSamplerVK", "sampler", "sampler", 1, false)})
            .template dynamic_cast_ref<IRenderObjectVK>());

    DebugDataArray rows = parse_json_array(container.to_string());

    ASSERT_EQ(rows.size(), 2U);
    EXPECT_EQ(rows[0].get_value<uint32_t>("frame_index"), 0U);
    EXPECT_EQ(rows[0].get_value<uint32_t>("heap_index"), 4U);
    EXPECT_EQ(rows[1].get_value<uint32_t>("frame_index"), 2U);
    EXPECT_EQ(rows[1].get_value<std::string>("heap"), "sampler");
    EXPECT_EQ(rows[1].get_value<std::nullptr_t>("device_address"), nullptr);
}

TEST(WBEROCPerFrameVKTest, ToStringDoesNotTriggerFlushSynchronization) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 1;

    ROCPerFrameVK container(&render_data);
    container.set(0,
        make_ref<TestDescriptorResourceVK>(
            &allocator, &render_data, DebugDataArray{make_descriptor_row("ROBufferVK", "uniform_buffer", "resource", 9, true)})
            .template dynamic_cast_ref<IRenderObjectVK>());

    uint8_t value = 7;
    container.publish_buffer_update(&value, 0, sizeof(value));

    EXPECT_NO_THROW({
        DebugDataArray rows = parse_json_array(container.to_string());
        ASSERT_EQ(rows.size(), 1U);
        EXPECT_EQ(rows[0].get_value<uint32_t>("frame_index"), 0U);
    });
}

TEST(WBEROCSingleVKTest, EmptyDescriptorResourcesProduceEmptyJsonArray) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    auto resource =
        make_ref<TestDescriptorResourceVK>(&allocator, &render_data, DebugDataArray{}).template dynamic_cast_ref<IRenderObjectVK>();

    ROCSingleVK container(resource);
    DebugDataArray rows = parse_json_array(container.to_string());

    EXPECT_TRUE(rows.empty());
}

} // namespace WhiteBirdEngine
