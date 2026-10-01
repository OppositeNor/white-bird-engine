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
#include "core/allocator/stack_allocator.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/mesh_draw_batch_planner_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <array>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace WhiteBirdEngine {

static ShaderMetadata make_batch_metadata() {
    ShaderMetadataTypeField position;
    position.name = "position";
    position.type.kind = "vector";
    position.type.element_scalar_type = "float32";
    position.type.element_count = 3;
    position.binding.kind = "varyingInput";
    position.binding.index = 2;
    ShaderMetadataUserAttribute role;
    role.name = "WBE_VERTEX_INPUT";
    position.user_attributes.push_back(role);
    ShaderMetadataParameter input;
    input.type.fields.push_back(position);
    ShaderMetadataEntryPoint entry;
    entry.name = "vertex_main";
    entry.stage = "vertex";
    entry.parameters.push_back(input);
    ShaderMetadata metadata;
    metadata.entry_points.push_back(entry);
    return metadata;
}

static RUStaticGeometrySubmesh make_batch_submesh(
    StackAllocator& p_storage, size_t p_vertex_start, size_t p_index_start, bool p_extended = false, bool p_position_last = false) {
    const size_t attribute_count = p_extended ? 2 : 1;
    MemID attributes_memory = p_storage.allocate(sizeof(RUVertexView::Attribute) * attribute_count);
    auto* attributes = static_cast<RUVertexView::Attribute*>(p_storage.get(attributes_memory));
    for (size_t i = 0; i < attribute_count; ++i) {
        const std::string_view name = i == (p_position_last ? 1U : 0U) ? "position" : "normal";
        MemID name_memory = p_storage.allocate(name.size() + 1);
        auto* name_data = static_cast<char*>(p_storage.get(name_memory));
        std::memcpy(name_data, name.data(), name.size());
        name_data[name.size()] = '\0';
        std::construct_at(attributes + i,
            RUVertexView::Attribute{
                .role = RUShaderRole(RefRes<char>(name_memory, &p_storage), dynam_hash(name)),
                .format = RUVertexAttributeFormat::FLOAT3,
                .offset = i * 12,
                .size = 12,
            });
    }
    RUStaticGeometrySubmesh submesh;
    submesh.vertices.attributes = RefRes<RUVertexView::Attribute>(attributes_memory, &p_storage);
    submesh.vertices.attribute_count = attribute_count;
    submesh.vertices.vertex_stride = attribute_count * 12;
    submesh.vertices.vertex_count = 3;
    submesh.vertices.binary.start = p_vertex_start;
    submesh.vertices.binary.size = submesh.vertices.vertex_stride * 3;
    submesh.indices.binary.start = p_index_start;
    submesh.indices.index_count = 3;
    submesh.instance_count = 1;
    return submesh;
}

TEST(WBEMeshDrawBatchPlannerVKTest, GroupsNonAdjacentLayoutsAndPreservesInstanceAndIndexRanges) {
    StackAllocator storage(WBE_KI_B(16));
    RenderAllocator allocator(WBE_KI_B(128));
    std::array submeshes{
        make_batch_submesh(storage, 120, 0), make_batch_submesh(storage, 240, 12, true), make_batch_submesh(storage, 12, 24)};
    submeshes[2].instance_count = 4;
    const std::array<uint32_t, 3> first_instances{7, 20, 31};
    auto result =
        MeshDrawBatchPlannerVK().build(&allocator, submeshes, first_instances, make_batch_metadata(), "vertex_main", 100);
    ASSERT_EQ(result.commands.size(), 3U);
    ASSERT_EQ(result.batches.size(), 2U);
    EXPECT_EQ(result.batches[0].first_draw, 0U);
    EXPECT_EQ(result.batches[0].draw_count, 2U);
    EXPECT_EQ(result.batches[0].vertex_buffer_offset, 12U);
    EXPECT_EQ(result.batches[1].first_draw, 2U);
    EXPECT_EQ(result.batches[1].draw_count, 1U);
    EXPECT_EQ(result.commands[0].vertexOffset, 9);
    EXPECT_EQ(result.commands[1].vertexOffset, 0);
    EXPECT_EQ(result.commands[0].firstInstance, 7U);
    EXPECT_EQ(result.commands[1].firstInstance, 31U);
    EXPECT_EQ(result.commands[2].firstInstance, 20U);
    EXPECT_EQ(result.commands[1].instanceCount, 4U);
    EXPECT_EQ(result.commands[1].indexCount, 3U);
    EXPECT_EQ(result.commands[1].firstIndex, 6U);
    EXPECT_EQ(result.commands[2].firstIndex, 3U);
}

TEST(WBEMeshDrawBatchPlannerVKTest, SplitsAtDeviceDrawLimit) {
    StackAllocator storage(WBE_KI_B(16));
    RenderAllocator allocator(WBE_KI_B(128));
    std::array submeshes{make_batch_submesh(storage, 0, 0), make_batch_submesh(storage, 36, 12), make_batch_submesh(storage, 72, 24)};
    const std::array<uint32_t, 3> first_instances{0, 1, 2};
    auto result =
        MeshDrawBatchPlannerVK().build(&allocator, submeshes, first_instances, make_batch_metadata(), "vertex_main", 2);
    ASSERT_EQ(result.batches.size(), 2U);
    EXPECT_EQ(result.batches[0].draw_count, 2U);
    EXPECT_EQ(result.batches[1].first_draw, 2U);
    EXPECT_EQ(result.batches[1].draw_count, 1U);
    const auto& last = result.batches[1];
    EXPECT_EQ(last.vertex_buffer_offset + static_cast<size_t>(result.commands[2].vertexOffset) * 12, 72U);
}

TEST(WBEMeshDrawBatchPlannerVKTest, SeparatesDifferentAttributeOffsetsAndUnalignedViewOrigins) {
    StackAllocator storage(WBE_KI_B(16));
    RenderAllocator allocator(WBE_KI_B(128));
    std::array submeshes{make_batch_submesh(storage, 0, 0, true),
        make_batch_submesh(storage, 72, 12, true, true),
        make_batch_submesh(storage, 148, 24, true)};
    const std::array<uint32_t, 3> first_instances{0, 1, 2};
    auto result =
        MeshDrawBatchPlannerVK().build(&allocator, submeshes, first_instances, make_batch_metadata(), "vertex_main", 100);
    ASSERT_EQ(result.batches.size(), 3U);
    EXPECT_EQ(result.batches[0].vertex_input.attributes[0].offset, 0U);
    EXPECT_EQ(result.batches[1].vertex_input.attributes[0].offset, 12U);
    EXPECT_EQ(result.batches[2].vertex_buffer_offset, 148U);
}

TEST(WBEMeshDrawBatchPlannerVKTest, SplitsBeforeSignedVertexOffsetOverflow) {
    StackAllocator storage(WBE_KI_B(16));
    RenderAllocator allocator(WBE_KI_B(128));
    const size_t boundary = static_cast<size_t>(std::numeric_limits<int32_t>::max()) * 12;
    std::array submeshes{
        make_batch_submesh(storage, 0, 0), make_batch_submesh(storage, boundary, 12), make_batch_submesh(storage, boundary + 12, 24)};
    const std::array<uint32_t, 3> first_instances{0, 1, 2};
    auto result =
        MeshDrawBatchPlannerVK().build(&allocator, submeshes, first_instances, make_batch_metadata(), "vertex_main", 100);
    ASSERT_EQ(result.batches.size(), 2U);
    EXPECT_EQ(result.commands[1].vertexOffset, std::numeric_limits<int32_t>::max());
    EXPECT_EQ(result.commands[2].vertexOffset, 0);
}

TEST(WBEMeshDrawBatchPlannerVKTest, SkipsEmptyDrawsAndRejectsInvalidArguments) {
    StackAllocator storage(WBE_KI_B(16));
    RenderAllocator allocator(WBE_KI_B(128));
    std::array submeshes{make_batch_submesh(storage, 0, 0), make_batch_submesh(storage, 36, 12)};
    submeshes[0].indices.index_count = 0;
    submeshes[1].instance_count = 0;
    const std::array<uint32_t, 2> first_instances{0, 0};
    auto result =
        MeshDrawBatchPlannerVK().build(&allocator, submeshes, first_instances, make_batch_metadata(), "vertex_main", 100);
    EXPECT_TRUE(result.commands.empty());
    EXPECT_TRUE(result.batches.empty());
    EXPECT_THROW(MeshDrawBatchPlannerVK().build(&allocator, submeshes, first_instances, make_batch_metadata(), "vertex_main", 0),
        std::runtime_error);
    EXPECT_THROW(MeshDrawBatchPlannerVK().build(&allocator, submeshes, {}, make_batch_metadata(), "vertex_main", 100),
        std::runtime_error);
    submeshes[0].indices.index_count = 3;
    submeshes[0].indices.binary.start = 1;
    EXPECT_THROW(MeshDrawBatchPlannerVK().build(&allocator, submeshes, first_instances, make_batch_metadata(), "vertex_main", 100),
        std::runtime_error);
}

} // namespace WhiteBirdEngine
