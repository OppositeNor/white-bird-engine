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
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_role.hh"
#include "function/render_vk/static_opaque_vertex_batch_planner_vk.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

static ShaderMetadata make_static_opaque_batch_vertex_metadata() {
    ShaderMetadataTypeField position;
    position.name = "position";
    position.type.kind = "vector";
    position.type.element_scalar_type = "float32";
    position.type.element_count = 3;
    position.binding.kind = "varyingInput";
    position.binding.index = 4;
    ShaderMetadataUserAttribute attribute;
    attribute.name = "WBE_VERTEX_INPUT";
    position.user_attributes.push_back(std::move(attribute));

    ShaderMetadataParameter parameter;
    parameter.type.fields.push_back(std::move(position));
    ShaderMetadataEntryPoint entry_point;
    entry_point.name = "vertex_main";
    entry_point.stage = "vertex";
    entry_point.parameters.push_back(std::move(parameter));
    ShaderMetadata result;
    result.entry_points.push_back(std::move(entry_point));
    return result;
}

static RenderDatabase::OpaqueTrianglesInfo::VertexAttribute make_static_opaque_batch_attribute(
    std::string_view p_role, RUVertexAttributeFormat p_format, size_t p_offset, size_t p_size) {
    return RenderDatabase::OpaqueTrianglesInfo::VertexAttribute{
        .role = ShaderRole(p_role), .format = p_format, .offset = p_offset, .size = p_size};
}

TEST(WBEStaticOpaqueVertexBatchPlannerVKTest, PreservesIndirectRangesAndBuildsLayoutSpecificDynamicState) {
    RenderAllocator allocator{WBE_KI_B(128)};
    RenderDatabase::OpaqueTrianglesInfo triangles_info(&allocator);

    RenderDatabase::OpaqueTrianglesInfo::DrawGroup compact_group(&allocator);
    compact_group.vertex_buffer_byte_offset = 32;
    compact_group.indirect_command_sub_region = {.starting_index = 2, .size = 1};
    compact_group.vertex_layout.vertex_stride = sizeof(float) * 3;
    compact_group.vertex_layout.attributes.push_back(
        make_static_opaque_batch_attribute("position", RUVertexAttributeFormat::FLOAT3, 0, sizeof(float) * 3));
    triangles_info.draw_groups.push_back(std::move(compact_group));

    RenderDatabase::OpaqueTrianglesInfo::DrawGroup extended_group(&allocator);
    extended_group.vertex_buffer_byte_offset = 160;
    extended_group.indirect_command_sub_region = {.starting_index = 7, .size = 2};
    extended_group.vertex_layout.vertex_stride = sizeof(float) * 7;
    extended_group.vertex_layout.attributes.push_back(
        make_static_opaque_batch_attribute("color", RUVertexAttributeFormat::FLOAT4, 0, sizeof(float) * 4));
    extended_group.vertex_layout.attributes.push_back(
        make_static_opaque_batch_attribute("position", RUVertexAttributeFormat::FLOAT3, sizeof(float) * 4, sizeof(float) * 3));
    triangles_info.draw_groups.push_back(std::move(extended_group));

    const ShaderMetadata metadata = make_static_opaque_batch_vertex_metadata();
    const auto batches = StaticOpaqueVertexBatchPlannerVK().build(metadata, "vertex_main", triangles_info, 0, 2);
    ASSERT_EQ(batches.size(), 2);

    EXPECT_EQ(batches[0].first_draw, 2U);
    EXPECT_EQ(batches[0].draw_count, 1U);
    EXPECT_EQ(batches[0].vertex_buffer_offset, 32U);
    ASSERT_EQ(batches[0].vertex_input.bindings.size(), 1);
    ASSERT_EQ(batches[0].vertex_input.attributes.size(), 1);
    EXPECT_EQ(batches[0].vertex_input.bindings[0].stride, sizeof(float) * 3);
    EXPECT_EQ(batches[0].vertex_input.attributes[0].location, 4U);
    EXPECT_EQ(batches[0].vertex_input.attributes[0].format, VK_FORMAT_R32G32B32_SFLOAT);
    EXPECT_EQ(batches[0].vertex_input.attributes[0].offset, 0U);

    EXPECT_EQ(batches[1].first_draw, 7U);
    EXPECT_EQ(batches[1].draw_count, 2U);
    EXPECT_EQ(batches[1].vertex_buffer_offset, 160U);
    ASSERT_EQ(batches[1].vertex_input.bindings.size(), 1);
    ASSERT_EQ(batches[1].vertex_input.attributes.size(), 1);
    EXPECT_EQ(batches[1].vertex_input.bindings[0].stride, sizeof(float) * 7);
    EXPECT_EQ(batches[1].vertex_input.attributes[0].location, 4U);
    EXPECT_EQ(batches[1].vertex_input.attributes[0].format, VK_FORMAT_R32G32B32_SFLOAT);
    EXPECT_EQ(batches[1].vertex_input.attributes[0].offset, sizeof(float) * 4);

    EXPECT_THROW(StaticOpaqueVertexBatchPlannerVK().build(metadata, "vertex_main", triangles_info, 1, 2), std::runtime_error);
}

} // namespace WhiteBirdEngine
