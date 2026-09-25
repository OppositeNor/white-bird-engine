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
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "utils/defs.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
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

} // namespace WhiteBirdEngine
