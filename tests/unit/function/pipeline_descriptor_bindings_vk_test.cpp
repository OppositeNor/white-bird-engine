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
#include "function/render_vk/pipeline_descriptor_bindings_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <stdexcept>
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

TEST(WBEPipelineDescriptorBindingsVKTest, PushesCurrentRendererDefaultValues) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    render_data.command_push_data = capture_push_data;
    render_data.time = 12.5;
    render_data.current_frame_slot = 2;
    ShaderDescriptorHeapLayoutVK layout;
    layout.size = captured_push_data.size();
    layout.default_fields = {
        ShaderDefaultPushFieldVK{.field_path = "time", .value = ShaderDefaultPushValueVK::TIME, .offset = 0},
        ShaderDefaultPushFieldVK{.field_path = "frame_slot", .value = ShaderDefaultPushValueVK::FRAME_SLOT, .offset = 4},
    };
    PipelineDescriptorBindingsVK bindings(&render_data, layout);

    bindings.push(VK_NULL_HANDLE);

    ASSERT_EQ(captured_push_data_size, captured_push_data.size());
    float time = 0.0F;
    std::memcpy(&time, captured_push_data.data(), sizeof(time));
    EXPECT_FLOAT_EQ(time, 12.5F);
    uint32_t frame_slot = 0;
    std::memcpy(&frame_slot, captured_push_data.data() + sizeof(time), sizeof(frame_slot));
    EXPECT_EQ(frame_slot, 2U);

    render_data.time = 23.25;
    render_data.current_frame_slot = 3;
    bindings.push(VK_NULL_HANDLE);
    std::memcpy(&time, captured_push_data.data(), sizeof(time));
    EXPECT_FLOAT_EQ(time, 23.25F);
    std::memcpy(&frame_slot, captured_push_data.data() + sizeof(time), sizeof(frame_slot));
    EXPECT_EQ(frame_slot, 3U);
}

namespace {

ShaderDescriptorHeapLayoutVK make_mapping_layout() {
    ShaderDescriptorHeapLayoutVK layout;
    layout.size = 16;
    layout.fields = {
        {.field_path = "first", .type = ShaderDescriptorHeapTypeVK::STORAGE_BUFFER, .resource_id = "source", .offset = 0},
        {.field_path = "second", .type = ShaderDescriptorHeapTypeVK::STORAGE_BUFFER, .resource_id = "target", .offset = 8},
    };
    return layout;
}

void add_mapping_resource(
    RenderAllocator* p_allocator, Map<std::string, Ref<RendResVK>, RenderAllocator>& p_resources, std::string_view p_id, RendResTID p_type) {
    auto resource = make_ref<RendResVK>(p_allocator);
    resource->res_id = p_id;
    resource->res_type_id = p_type;
    resource->container = make_ref<ROCMultislotsVK>(p_allocator, p_allocator, 2, false);
    p_resources.emplace(std::string(p_id), resource);
}

} // namespace

TEST(WBEPipelineDescriptorBindingsVKTest, RejectsEmptyDuplicateAndUnknownMappingSources) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    PipelineResourceMapVK map(&allocator);
    EXPECT_THROW(map.bind("", "target"), std::runtime_error);
    EXPECT_THROW(map.bind("source", ""), std::runtime_error);
    map.bind("source", "target");
    EXPECT_THROW(map.bind("source", "other"), std::runtime_error);
    map.bind("unknown", "target");
    EXPECT_THROW(PipelineDescriptorBindingsVK(&render_data, make_mapping_layout(), map), std::runtime_error);
}

TEST(WBEPipelineDescriptorBindingsVKTest, ExactSingleStepMappingsAreIsolatedAndSnapshotConfiguration) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    auto layout = make_mapping_layout();
    PipelineResourceMapVK map(&allocator);
    map.bind("source", "target").bind("target", "final");
    PipelineDescriptorBindingsVK mapped(&render_data, layout, map);
    PipelineDescriptorBindingsVK defaults(&render_data, layout);
    Map<std::string, Ref<RendResVK>, RenderAllocator> resources(&allocator);
    add_mapping_resource(&allocator, resources, "target", TypeUUID<ROBufferVK>::UUID);
    add_mapping_resource(&allocator, resources, "final", TypeUUID<ROBufferVK>::UUID);
    RendResViewVK view(&resources);
    EXPECT_NO_THROW(mapped.bind_resources(view));
    EXPECT_THROW(defaults.bind_resources(view), std::runtime_error);
    resources.erase("target");
    EXPECT_THROW(mapped.bind_resources(view), std::runtime_error); // source must not chain through target to final.
    EXPECT_EQ(layout.fields[0].resource_id, "source");
    EXPECT_EQ(layout.fields[1].resource_id, "target");
    map.bind("unreflected_later_change", "final");
    add_mapping_resource(&allocator, resources, "target", TypeUUID<ROBufferVK>::UUID);
    EXPECT_NO_THROW(mapped.bind_resources(view));
    add_mapping_resource(&allocator, resources, "source", TypeUUID<ROBufferVK>::UUID);
    EXPECT_NO_THROW(defaults.bind_resources(view));
}

TEST(WBEPipelineDescriptorBindingsVKTest, RejectsMissingIncompatibleAndNullMappedTargets) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    auto layout = make_mapping_layout();
    layout.fields.resize(1);
    PipelineResourceMapVK map(&allocator);
    map.bind("source", "mapped");
    PipelineDescriptorBindingsVK bindings(&render_data, layout, map);
    Map<std::string, Ref<RendResVK>, RenderAllocator> resources(&allocator);
    RendResViewVK view(&resources);
    EXPECT_THROW(bindings.bind_resources(view), std::runtime_error);
    add_mapping_resource(&allocator, resources, "mapped", TypeUUID<ROImageViewVK>::UUID);
    EXPECT_THROW(bindings.bind_resources(view), std::runtime_error);
    resources.at("mapped")->res_type_id = TypeUUID<ROBufferVK>::UUID;
    EXPECT_NO_THROW(bindings.bind_resources(view));
    render_data.current_frame_slot = 1;
    EXPECT_THROW(bindings.push(VK_NULL_HANDLE), std::runtime_error);
    resources.at("mapped")->container = MEM_NULL;
    EXPECT_THROW(bindings.bind_resources(view), std::runtime_error);
}

} // namespace WhiteBirdEngine
