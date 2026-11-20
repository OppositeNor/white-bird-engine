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
#include "function/render/i_rend_task.hh"
#include "function/render/rend_defs.hh"
#include "function/render/rend_task_resource_map.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/pipeline_desc_bindings_vk.hh"
#include "function/rend_vk/rend_graph_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_buffer_vk.hh"
#include "function/rend_vk/ro_image_view_vk.hh"
#include "function/rend_vk/roc_multislots_vk.hh"
#include "function/rend_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

std::array<uint8_t, 8> captured_push_data{};
size_t captured_push_data_size = 0;

VKAPI_ATTR void VKAPI_CALL capture_push_data(VkCommandBuffer /*p_command_buffer*/, const VkPushDataInfoEXT* p_push_data_info) {
    captured_push_data_size = p_push_data_info->data.size;
    std::memcpy(captured_push_data.data(), p_push_data_info->data.address, captured_push_data_size);
}

} // namespace

TEST(WBEPipelineDescBindingsVKTest, PushesCurrentRendererDefaultValues) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    rend_data.command_push_data = capture_push_data;
    rend_data.time = 12.5;
    rend_data.current_frame_slot = 2;
    ShaderDescHeapLayoutVK layout;
    layout.size = captured_push_data.size();
    layout.default_fields = {
        ShaderDefaultPushFieldVK{.field_path = "time", .value = ShaderDefaultPushValueVK::TIME, .offset = 0},
        ShaderDefaultPushFieldVK{.field_path = "frame_slot", .value = ShaderDefaultPushValueVK::FRAME_SLOT, .offset = 4},
    };
    PipelineDescBindingsVK bindings(&rend_data, layout);

    bindings.push(VK_NULL_HANDLE);

    ASSERT_EQ(captured_push_data_size, captured_push_data.size());
    float time = 0.0F;
    std::memcpy(&time, captured_push_data.data(), sizeof(time));
    EXPECT_FLOAT_EQ(time, 12.5F);
    uint32_t frame_slot = 0;
    std::memcpy(&frame_slot, captured_push_data.data() + sizeof(time), sizeof(frame_slot));
    EXPECT_EQ(frame_slot, 2U);

    rend_data.time = 23.25;
    rend_data.current_frame_slot = 3;
    bindings.push(VK_NULL_HANDLE);
    std::memcpy(&time, captured_push_data.data(), sizeof(time));
    EXPECT_FLOAT_EQ(time, 23.25F);
    std::memcpy(&frame_slot, captured_push_data.data() + sizeof(time), sizeof(frame_slot));
    EXPECT_EQ(frame_slot, 3U);
}

TEST(WBEPipelineDescBindingsVKTest, PushesTaskSuppliedCustomValues) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    rend_data.command_push_data = capture_push_data;
    ShaderDescHeapLayoutVK layout;
    layout.size = captured_push_data.size();
    layout.default_fields = {
        ShaderDefaultPushFieldVK{.field_path = "level",
            .value = ShaderDefaultPushValueVK::CUSTOM,
            .name = "level",
            .type = ShaderPushConstantTypeVK::UINT32,
            .offset = 0},
        ShaderDefaultPushFieldVK{.field_path = "scale",
            .value = ShaderDefaultPushValueVK::CUSTOM,
            .name = "scale",
            .type = ShaderPushConstantTypeVK::FLOAT32,
            .offset = 4},
    };
    PipelineDescBindingsVK bindings(&rend_data, layout);

    // Every declared custom value must be set before the first push.
    EXPECT_THROW(bindings.push(VK_NULL_HANDLE), std::runtime_error);
    bindings.set_push_constant("level", PushConstantValueVK::from_uint32(7));
    EXPECT_THROW(bindings.push(VK_NULL_HANDLE), std::runtime_error);
    EXPECT_THROW(bindings.set_push_constant("scale", PushConstantValueVK::from_int32(1)), std::runtime_error);
    EXPECT_THROW(bindings.set_push_constant("missing", PushConstantValueVK::from_uint32(1)), std::runtime_error);
    EXPECT_FALSE(bindings.try_set_push_constant("missing", PushConstantValueVK::from_uint32(1)));
    EXPECT_TRUE(bindings.try_set_push_constant("scale", PushConstantValueVK::from_float32(0.5F)));

    bindings.push(VK_NULL_HANDLE);

    ASSERT_EQ(captured_push_data_size, captured_push_data.size());
    uint32_t level = 0;
    std::memcpy(&level, captured_push_data.data(), sizeof(level));
    EXPECT_EQ(level, 7U);
    float scale = 0.0F;
    std::memcpy(&scale, captured_push_data.data() + sizeof(level), sizeof(scale));
    EXPECT_FLOAT_EQ(scale, 0.5F);

    // Values persist across pushes until they are set again.
    bindings.set_push_constant("level", PushConstantValueVK::from_uint32(9));
    bindings.push(VK_NULL_HANDLE);
    std::memcpy(&level, captured_push_data.data(), sizeof(level));
    EXPECT_EQ(level, 9U);
    std::memcpy(&scale, captured_push_data.data() + sizeof(level), sizeof(scale));
    EXPECT_FLOAT_EQ(scale, 0.5F);
}

namespace {

ShaderDescHeapLayoutVK make_mapping_layout() {
    ShaderDescHeapLayoutVK layout;
    layout.size = 16;
    layout.fields = {
        {.field_path = "first", .type = ShaderDescHeapTypeVK::STORAGE_BUFFER, .resource_id = "source", .offset = 0},
        {.field_path = "second", .type = ShaderDescHeapTypeVK::STORAGE_BUFFER, .resource_id = "target", .offset = 8},
    };
    return layout;
}

class TestMappingResourceVK final : public IRendObjectVK {
public:
    explicit TestMappingResourceVK(const RendContextVK* p_rend_data) : IRendObjectVK(p_rend_data) {
    }
    virtual ~TestMappingResourceVK() override = default;
    TestMappingResourceVK(const TestMappingResourceVK&) = delete;
    TestMappingResourceVK(TestMappingResourceVK&&) noexcept = delete;
    TestMappingResourceVK& operator=(const TestMappingResourceVK&) = delete;
    TestMappingResourceVK& operator=(TestMappingResourceVK&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return TypeUUID<ROBufferVK>::UUID;
    }
};

// Add a resource whose two slots are empty, so that pushing its descriptor fails.
void add_mapping_resource(RendAlloc* p_alloc,
    const RendContextVK* p_rend_data,
    Map<std::string, Ref<RendResVK>, RendAlloc>& p_resources,
    std::string_view p_id,
    RendResTID p_type) {
    auto resource = make_ref<RendResVK>(p_alloc);
    resource->res_id = p_id;
    resource->res_type_id = p_type;
    auto container = make_ref<ROCMultislotsVK>(
        p_alloc,
        p_alloc,
        2,
        [p_alloc, p_rend_data](uint32_t /*p_slot*/) {
            return make_ref<TestMappingResourceVK>(p_alloc, p_rend_data).dynamic_cast_ref<IRendObjectVK>();
        },
        false);
    container->clear();
    resource->container = container;
    p_resources.emplace(std::string(p_id), resource);
}

} // namespace

TEST(WBEPipelineDescBindingsVKTest, ResourceMapRejectsEmptyAndDuplicateSourcesAndTracksUse) {
    RendAlloc alloc(WBE_KI_B(64));
    RendTaskResourceMap map(&alloc);
    EXPECT_THROW(map.bind("", "target"), std::runtime_error);
    EXPECT_THROW(map.bind("source", ""), std::runtime_error);
    map.bind("source", "target").bind("target", "final");
    EXPECT_THROW(map.bind("source", "other"), std::runtime_error);

    EXPECT_THROW(map.validate_all_used("task"), std::runtime_error);
    // Resolution is a single exact step and does not chain through other entries.
    EXPECT_EQ(map.resolve("source"), "target");
    EXPECT_EQ(map.resolve("unmapped"), "unmapped");
    EXPECT_THROW(map.validate_all_used("task"), std::runtime_error);
    EXPECT_EQ(map.resolve("target"), "final");
    EXPECT_NO_THROW(map.validate_all_used("task"));
}

TEST(WBEPipelineDescBindingsVKTest, BindsReflectedResourcesThroughTheTaskResourceMap) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    auto layout = make_mapping_layout();
    layout.fields.resize(1);
    PipelineDescBindingsVK bindings(&rend_data, layout);
    RendTaskResourceMap map(&alloc);
    map.bind("source", "mapped");
    Map<std::string, Ref<RendResVK>, RendAlloc> resources(&alloc);
    RendResViewVK view(&resources, &map);
    EXPECT_THROW(bindings.bind_resources(view), std::runtime_error);
    add_mapping_resource(&alloc, &rend_data, resources, "mapped", TypeUUID<ROImageViewVK>::UUID);
    EXPECT_THROW(bindings.bind_resources(view), std::runtime_error);
    resources.at("mapped")->res_type_id = TypeUUID<ROBufferVK>::UUID;
    EXPECT_NO_THROW(bindings.bind_resources(view));
    EXPECT_NO_THROW(map.validate_all_used("task"));
    rend_data.current_frame_slot = 1;
    EXPECT_THROW(bindings.push(VK_NULL_HANDLE), std::runtime_error);
    resources.at("mapped")->container = MEM_NULL;
    EXPECT_THROW(bindings.bind_resources(view), std::runtime_error);
    // Without the map the reflected identifier is looked up directly.
    EXPECT_THROW(bindings.bind_resources(RendResViewVK(&resources)), std::runtime_error);
}

TEST(WBEPipelineDescBindingsVKTest, AppendsReflectedAccessesWithTheirReflectedIdentifiers) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    ShaderDescHeapLayoutVK layout;
    layout.size = 40;
    layout.fields = {
        {.field_path = "uniform", .type = ShaderDescHeapTypeVK::UNIFORM_BUFFER, .resource_id = "camera", .offset = 0},
        {.field_path = "input", .type = ShaderDescHeapTypeVK::STORAGE_BUFFER, .resource_id = "input", .offset = 8, .access = ShaderDescAccessVK::READ},
        {.field_path = "output", .type = ShaderDescHeapTypeVK::STORAGE_IMAGE, .resource_id = "output", .offset = 16, .access = ShaderDescAccessVK::WRITE},
        {.field_path = "state", .type = ShaderDescHeapTypeVK::STORAGE_BUFFER, .resource_id = "state", .offset = 24, .access = ShaderDescAccessVK::READ_WRITE},
        {.field_path = "sampler", .type = ShaderDescHeapTypeVK::SAMPLER, .resource_id = "sampler", .offset = 32},
    };
    PipelineDescBindingsVK bindings(&rend_data, layout);
    Vector<ResAccessVK, RendAlloc> reads(&alloc);
    Vector<ResAccessVK, RendAlloc> writes(&alloc);

    bindings.append_accesses(reads, writes);

    ASSERT_EQ(reads.size(), 3U);
    EXPECT_EQ(reads[0].res_id, "camera");
    EXPECT_EQ(reads[0].usage, RendResUsageVK::UNIFORM_BUFFER);
    EXPECT_EQ(reads[1].res_id, "input");
    EXPECT_EQ(reads[1].usage, RendResUsageVK::STORAGE_BUFFER);
    EXPECT_EQ(reads[2].res_id, "state");
    ASSERT_EQ(writes.size(), 2U);
    EXPECT_EQ(writes[0].res_id, "output");
    EXPECT_EQ(writes[0].usage, RendResUsageVK::STORAGE_IMAGE);
    EXPECT_EQ(writes[1].res_id, "state");
}

} // namespace WhiteBirdEngine
