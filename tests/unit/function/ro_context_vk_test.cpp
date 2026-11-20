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
#include "core/parser/parser_json.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/debug_data_vk.hh"
#include "function/rend_vk/i_rend_object_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/roc_multislots_vk.hh"
#include "function/rend_vk/roc_single_vk.hh"
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

DebugData make_desc_row(
    std::string_view p_resource_type, std::string_view p_desc_type, std::string_view p_heap, uint32_t p_heap_index, bool p_has_address) {
    DebugData row;
    row.set_value("resource_type", std::string(p_resource_type));
    row.set_value("desc_type", std::string(p_desc_type));
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

class TestContextResourceVK final : public IRendObjectVK {
public:
    TestContextResourceVK(const RendContextVK* p_rend_data, uint32_t p_context_id)
        : IRendObjectVK(p_rend_data), context_id(p_context_id) {
    }
    virtual ~TestContextResourceVK() override = default;
    TestContextResourceVK(const TestContextResourceVK&) = delete;
    TestContextResourceVK(TestContextResourceVK&&) noexcept = delete;
    TestContextResourceVK& operator=(const TestContextResourceVK&) = delete;
    TestContextResourceVK& operator=(TestContextResourceVK&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override;

    uint32_t context_id;
};

class TestDescResourceVK final : public IRendObjectVK {
public:
    TestDescResourceVK(const RendContextVK* p_rend_data, DebugDataArray p_rows)
        : IRendObjectVK(p_rend_data), rows(p_rend_data->alloc) {
        rows.reserve(p_rows.size());
        for (DebugData& row : p_rows) {
            rows.emplace_back(std::move(row));
        }
    }
    virtual ~TestDescResourceVK() override = default;
    TestDescResourceVK(const TestDescResourceVK&) = delete;
    TestDescResourceVK(TestDescResourceVK&&) noexcept = delete;
    TestDescResourceVK& operator=(const TestDescResourceVK&) = delete;
    TestDescResourceVK& operator=(TestDescResourceVK&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override;

    virtual DebugDataRows desc_debug_data() const override {
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
struct TypeUUID<TestDescResourceVK> {
    using Type = TestDescResourceVK;
    static constexpr TypeID UUID = static_hash("TestDescResourceVK");
};

TypeID TestContextResourceVK::get_type_uuid() const {
    return TypeUUID<TestContextResourceVK>::UUID;
}

TypeID TestDescResourceVK::get_type_uuid() const {
    return TypeUUID<TestDescResourceVK>::UUID;
}

namespace {

// Factory of one TestContextResourceVK per slot, identified by p_first_id plus the slot index.
RendObjectContainerVK::Factory make_context_factory(
    RendAlloc* p_alloc, const RendContextVK* p_rend_data, uint32_t p_first_id = 0) {
    return [p_alloc, p_rend_data, p_first_id](uint32_t p_slot) {
        return make_ref<TestContextResourceVK>(p_alloc, p_rend_data, p_first_id + p_slot).template dynamic_cast_ref<IRendObjectVK>();
    };
}

// Factory of TestDescResourceVK objects without descriptor rows.
RendObjectContainerVK::Factory make_empty_desc_factory(RendAlloc* p_alloc, const RendContextVK* p_rend_data) {
    return [p_alloc, p_rend_data](uint32_t /*p_slot*/) {
        return make_ref<TestDescResourceVK>(p_alloc, p_rend_data, DebugDataArray{}).template dynamic_cast_ref<IRendObjectVK>();
    };
}

} // namespace

TEST(WBEROCMultislotsVKTest, CreatesOneResourcePerSlotAndDefaultsToFirstSlot) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    rend_data.frames_in_flight = 3;

    ROCMultislotsVK context(&alloc, 3, ROCMultislotsVK::Factory{[&rend_data, &alloc](uint32_t p_slot) {
        return make_ref<TestContextResourceVK>(&alloc, &rend_data, p_slot).template dynamic_cast_ref<IRendObjectVK>();
    }});

    EXPECT_EQ(context.get_resource_type_id(), TypeUUID<TestContextResourceVK>::UUID);
    EXPECT_EQ(context.get_slot_count(), 3U);

    auto default_resource = context.get().dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(default_resource, MEM_NULL);
    EXPECT_EQ(default_resource->get_type_uuid(), TypeUUID<TestContextResourceVK>::UUID);
    EXPECT_EQ(default_resource->context_id, 0U);

    auto explicit_resource = context.get(2).dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(explicit_resource, MEM_NULL);
    EXPECT_EQ(explicit_resource->context_id, 2U);
}

TEST(WBEROCMultislotsVKTest, ThrowsWhenFactoryIsMissingOrReturnsNull) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);

    EXPECT_THROW(ROCMultislotsVK(&alloc, 2, ROCMultislotsVK::Factory{}), std::runtime_error);
    EXPECT_THROW(ROCMultislotsVK(&alloc, 2, ROCMultislotsVK::Factory{[](uint32_t /*p_slot*/) {
        return Ref<IRendObjectVK>(MEM_NULL);
    }}),
        std::runtime_error);
    EXPECT_THROW(ROCMultislotsVK(nullptr, 2, make_context_factory(&alloc, &rend_data)), std::runtime_error);
    EXPECT_THROW(ROCMultislotsVK(&alloc, 0, make_context_factory(&alloc, &rend_data)), std::runtime_error);
}

TEST(WBEROCMultislotsVKTest, UpdatesOneSlotIndependently) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);

    ROCMultislotsVK context(&alloc, 3, make_context_factory(&alloc, &rend_data, 100));
    auto slot_zero_resource =
        make_ref<TestContextResourceVK>(&alloc, &rend_data, 0).template dynamic_cast_ref<IRendObjectVK>();
    auto slot_two_resource =
        make_ref<TestContextResourceVK>(&alloc, &rend_data, 2).template dynamic_cast_ref<IRendObjectVK>();

    context.set(0, slot_zero_resource);
    context.set(2, slot_two_resource);
    context.clear(1);

    auto default_resource = context.get().dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(default_resource, MEM_NULL);
    EXPECT_EQ(default_resource->context_id, 0U);

    auto explicit_resource = context.get(0).dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(explicit_resource, MEM_NULL);
    EXPECT_EQ(explicit_resource->context_id, 0U);

    EXPECT_EQ(context.get(1), MEM_NULL);
}

TEST(WBEROCMultislotsVKTest, NoFlushModeStillSupportsPerSlotReplacement) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);

    ROCMultislotsVK context(&alloc, 2, make_context_factory(&alloc, &rend_data, 100), false);
    auto slot_zero_resource =
        make_ref<TestContextResourceVK>(&alloc, &rend_data, 10).template dynamic_cast_ref<IRendObjectVK>();
    auto slot_one_resource =
        make_ref<TestContextResourceVK>(&alloc, &rend_data, 11).template dynamic_cast_ref<IRendObjectVK>();
    auto replacement_resource =
        make_ref<TestContextResourceVK>(&alloc, &rend_data, 42).template dynamic_cast_ref<IRendObjectVK>();

    context.set(0, slot_zero_resource);
    context.set(1, slot_one_resource);
    context.set(1, replacement_resource);

    auto default_resource = context.get().dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(default_resource, MEM_NULL);
    EXPECT_EQ(default_resource->context_id, 10U);

    auto other_resource = context.get(0).dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(other_resource, MEM_NULL);
    EXPECT_EQ(other_resource->context_id, 10U);
}

TEST(WBEROCMultislotsVKTest, RejectsEmptyPublishedBufferUpdate) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    ROCMultislotsVK context(&alloc, 2, make_context_factory(&alloc, &rend_data));

    EXPECT_THROW(context.publish_buffer_update(nullptr, 0, 1), std::runtime_error);
    uint8_t value = 0;
    EXPECT_THROW(context.publish_buffer_update(&value, 0, 0), std::runtime_error);
}

TEST(WBEROCSingleVKTest, ReturnsStoredResource) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);

    auto resource = make_ref<TestContextResourceVK>(&alloc, &rend_data, 7).template dynamic_cast_ref<IRendObjectVK>();
    ROCSingleVK container([resource](uint32_t /*p_slot*/) {
        return resource;
    });

    EXPECT_EQ(container.get_resource_type_id(), TypeUUID<TestContextResourceVK>::UUID);

    auto stored_resource = container.get().dynamic_cast_ref<TestContextResourceVK>();
    ASSERT_NE(stored_resource, MEM_NULL);
    EXPECT_EQ(stored_resource->context_id, 7U);
}

TEST(WBEROCSingleVKTest, ToStringOmitsSlotIndexAndPreservesMultipleDescs) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    auto resource = make_ref<TestDescResourceVK>(&alloc,
        &rend_data,
        DebugDataArray{make_desc_row("ROBufferVK", "uniform_buffer", "resource", 13, true),
            make_desc_row("ROImageViewVK", "sampled_image", "resource", 17, false)})
                        .template dynamic_cast_ref<IRendObjectVK>();

    ROCSingleVK container([resource](uint32_t /*p_slot*/) {
        return resource;
    });
    DebugDataArray rows = parse_json_array(container.to_string());

    ASSERT_EQ(rows.size(), 2U);
    EXPECT_FALSE(rows[0].contains("slot_index"));
    EXPECT_EQ(rows[0].get_value<std::string>("resource_type"), "ROBufferVK");
    EXPECT_EQ(rows[0].get_value<std::string>("desc_type"), "uniform_buffer");
    EXPECT_EQ(rows[0].get_value<std::string>("device_address"), "0x1234");
    EXPECT_EQ(rows[1].get_value<std::string>("resource_type"), "ROImageViewVK");
    EXPECT_EQ(rows[1].get_value<std::string>("desc_type"), "sampled_image");
    EXPECT_FALSE(rows[1].contains("slot_index"));
    EXPECT_EQ(rows[1].get_value<std::nullptr_t>("device_address"), nullptr);
}

TEST(WBEROCMultislotsVKTest, ToStringAddsSlotIndexAndSkipsEmptyResources) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);

    ROCMultislotsVK container(&alloc, 3, make_empty_desc_factory(&alloc, &rend_data));
    container.clear(1);
    container.set(0,
        make_ref<TestDescResourceVK>(
            &alloc, &rend_data, DebugDataArray{make_desc_row("ROBufferVK", "uniform_buffer", "resource", 4, true)})
            .template dynamic_cast_ref<IRendObjectVK>());
    container.set(2,
        make_ref<TestDescResourceVK>(
            &alloc, &rend_data, DebugDataArray{make_desc_row("ROSamplerVK", "sampler", "sampler", 1, false)})
            .template dynamic_cast_ref<IRendObjectVK>());

    DebugDataArray rows = parse_json_array(container.to_string());

    ASSERT_EQ(rows.size(), 2U);
    EXPECT_EQ(rows[0].get_value<uint32_t>("slot_index"), 0U);
    EXPECT_EQ(rows[0].get_value<uint32_t>("heap_index"), 4U);
    EXPECT_EQ(rows[1].get_value<uint32_t>("slot_index"), 2U);
    EXPECT_EQ(rows[1].get_value<std::string>("heap"), "sampler");
    EXPECT_EQ(rows[1].get_value<std::nullptr_t>("device_address"), nullptr);
}

TEST(WBEROCMultislotsVKTest, ToStringDoesNotTriggerFlushSynchronization) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);

    ROCMultislotsVK container(&alloc, 1, make_empty_desc_factory(&alloc, &rend_data));
    container.set(0,
        make_ref<TestDescResourceVK>(
            &alloc, &rend_data, DebugDataArray{make_desc_row("ROBufferVK", "uniform_buffer", "resource", 9, true)})
            .template dynamic_cast_ref<IRendObjectVK>());

    uint8_t value = 7;
    container.publish_buffer_update(&value, 0, sizeof(value));

    EXPECT_NO_THROW({
        DebugDataArray rows = parse_json_array(container.to_string());
        ASSERT_EQ(rows.size(), 1U);
        EXPECT_EQ(rows[0].get_value<uint32_t>("slot_index"), 0U);
    });
}

TEST(WBEROCSingleVKTest, EmptyDescResourcesProduceEmptyJsonArray) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    auto resource =
        make_ref<TestDescResourceVK>(&alloc, &rend_data, DebugDataArray{}).template dynamic_cast_ref<IRendObjectVK>();

    ROCSingleVK container([resource](uint32_t /*p_slot*/) {
        return resource;
    });
    DebugDataArray rows = parse_json_array(container.to_string());

    EXPECT_TRUE(rows.empty());
}

TEST(WBEROCSingleVKTest, RecreateReplacesTheResourceWithANewOneFromTheFactory) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    uint32_t created_count = 0;
    ROCSingleVK container([&alloc, &rend_data, &created_count](uint32_t p_slot) {
        EXPECT_EQ(p_slot, 0U);
        return make_ref<TestContextResourceVK>(&alloc, &rend_data, created_count++).template dynamic_cast_ref<IRendObjectVK>();
    });
    EXPECT_EQ(container.get().dynamic_cast_ref<TestContextResourceVK>()->context_id, 0U);

    container.recreate();

    EXPECT_EQ(created_count, 2U);
    EXPECT_EQ(container.get().dynamic_cast_ref<TestContextResourceVK>()->context_id, 1U);
}

TEST(WBEROCSingleVKTest, ThrowsWhenFactoryIsMissingOrReturnsNull) {
    EXPECT_THROW(ROCSingleVK(RendObjectContainerVK::Factory{}), std::runtime_error);
    EXPECT_THROW(ROCSingleVK([](uint32_t /*p_slot*/) {
        return Ref<IRendObjectVK>(MEM_NULL);
    }),
        std::runtime_error);
}

TEST(WBEROCMultislotsVKTest, RecreateReplacesEverySlotIncludingClearedAndReplacedOnes) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    uint32_t generation = 0;
    ROCMultislotsVK container(&alloc, 3, [&alloc, &rend_data, &generation](uint32_t p_slot) {
        return make_ref<TestContextResourceVK>(&alloc, &rend_data, generation * 10 + p_slot).template dynamic_cast_ref<IRendObjectVK>();
    });
    container.clear(1);
    container.set(2, make_ref<TestContextResourceVK>(&alloc, &rend_data, 99).template dynamic_cast_ref<IRendObjectVK>());

    generation = 1;
    container.recreate();

    for (uint32_t slot = 0; slot < 3; ++slot) {
        auto resource = container.get(slot).dynamic_cast_ref<TestContextResourceVK>();
        ASSERT_NE(resource, MEM_NULL);
        EXPECT_EQ(resource->context_id, 10U + slot);
    }
}

} // namespace WhiteBirdEngine
