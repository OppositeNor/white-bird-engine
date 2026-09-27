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
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "glm/ext/vector_float3.hpp"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {

class RenderDatabaseTestRenderObject final : public IRenderObject {
public:
    RenderDatabaseTestRenderObject() = default;
    ~RenderDatabaseTestRenderObject() override = default;
    RenderDatabaseTestRenderObject(const RenderDatabaseTestRenderObject&) = delete;
    RenderDatabaseTestRenderObject(RenderDatabaseTestRenderObject&&) noexcept = delete;
    RenderDatabaseTestRenderObject& operator=(const RenderDatabaseTestRenderObject&) = delete;
    RenderDatabaseTestRenderObject& operator=(RenderDatabaseTestRenderObject&&) noexcept = delete;

    TypeID get_type_uuid() const override;
};

TypeID RenderDatabaseTestRenderObject::get_type_uuid() const {
    return static_hash("RenderDatabaseTestRenderObject");
}

class RenderDatabaseTestResourceStorage {
public:
    RenderDatabaseTestResourceStorage() = default;
    ~RenderDatabaseTestResourceStorage() = default;
    RenderDatabaseTestResourceStorage(const RenderDatabaseTestResourceStorage&) = delete;
    RenderDatabaseTestResourceStorage(RenderDatabaseTestResourceStorage&&) noexcept = delete;
    RenderDatabaseTestResourceStorage& operator=(const RenderDatabaseTestResourceStorage&) = delete;
    RenderDatabaseTestResourceStorage& operator=(RenderDatabaseTestResourceStorage&&) noexcept = delete;

    template <typename T>
    RefRes<T> make_object() {
        MemID memory_id = allocator.allocate(sizeof(T));
        auto* object = static_cast<T*>(allocator.get(memory_id));
        std::construct_at(object);
        return RefRes<T>(memory_id, &allocator);
    }

    template <typename T>
    RefRes<T> make_array(std::span<const T> p_values) {
        MemID memory_id = allocator.allocate(sizeof(T) * p_values.size());
        auto* values = static_cast<T*>(allocator.get(memory_id));
        for (size_t i = 0; i < p_values.size(); ++i) {
            std::construct_at(values + i, p_values[i]);
        }
        return RefRes<T>(memory_id, &allocator);
    }

    RefRes<char> make_string(std::string_view p_value) {
        MemID memory_id = allocator.allocate(p_value.size() + 1);
        auto* value = static_cast<char*>(allocator.get(memory_id));
        std::memcpy(value, p_value.data(), p_value.size());
        value[p_value.size()] = '\0';
        return RefRes<char>(memory_id, &allocator);
    }

private:
    StackAllocator allocator{WBE_KI_B(64)};
};

template <typename T, size_t Size>
std::vector<uint8_t> make_vertex_bytes(const std::array<T, Size>& p_values) {
    std::vector<uint8_t> result(sizeof(p_values));
    std::memcpy(result.data(), p_values.data(), result.size());
    return result;
}

void append_bytes(std::vector<uint8_t>& p_target, const std::vector<uint8_t>& p_source) {
    p_target.insert(p_target.end(), p_source.begin(), p_source.end());
}

RUBinaryView make_binary_view(RenderDatabaseTestResourceStorage& p_storage, const std::vector<uint8_t>& p_bytes) {
    RefRes<RUBinary> binary = p_storage.make_object<RUBinary>();
    binary->data = p_storage.make_array<uint8_t>(p_bytes).force_cast<char>();
    binary->size = p_bytes.size();
    return RUBinaryView{.binary = binary, .start = 0, .size = p_bytes.size()};
}

RUVertexView make_vertex_data(RenderDatabaseTestResourceStorage& p_storage,
    std::string_view p_role,
    RUVertexAttributeFormat p_format,
    size_t p_stride,
    size_t p_vertex_count,
    const std::vector<uint8_t>& p_vertices) {
    RefRes<RUVertexView::Attribute> attribute = p_storage.make_object<RUVertexView::Attribute>();
    attribute->role.name = p_storage.make_string(p_role);
    attribute->role.hash = dynam_hash(p_role);
    attribute->format = p_format;
    attribute->size = p_stride;

    RUVertexView result;
    result.binary = make_binary_view(p_storage, p_vertices);
    result.attributes = attribute;
    result.attribute_count = 1;
    result.vertex_stride = p_stride;
    result.vertex_count = p_vertex_count;
    return result;
}

RUIndexView make_index_data(RenderDatabaseTestResourceStorage& p_storage, std::span<const uint32_t> p_indices) {
    std::vector<uint8_t> bytes(sizeof(uint32_t) * p_indices.size());
    if (!bytes.empty()) {
        std::memcpy(bytes.data(), p_indices.data(), bytes.size());
    }
    RUIndexView result;
    result.binary = make_binary_view(p_storage, bytes);
    result.index_count = p_indices.size();
    return result;
}

RefRes<RUSubmesh3D> make_submesh(RenderDatabaseTestResourceStorage& p_storage,
    RefRes<RUMaterial> p_material,
    RUVertexView p_vertex_data,
    const std::array<uint32_t, 3>& p_indices) {
    RefRes<RUSubmesh3D> result = p_storage.make_object<RUSubmesh3D>();
    result->material = p_material;
    result->vertices = p_vertex_data;
    result->indices = make_index_data(p_storage, p_indices);
    return result;
}

RefRes<RUMaterial> make_material(
    RenderDatabaseTestResourceStorage& p_storage, const char* p_resource_key, RefRes<RUGraphicsPipeline> p_pipeline) {
    RefRes<RUMaterial> result = p_storage.make_object<RUMaterial>();
    result->resource_key = p_resource_key;
    result->graphics_pipeline = p_pipeline;
    return result;
}

RefRes<RUGraphicsPipeline> make_pipeline(RenderDatabaseTestResourceStorage& p_storage, const char* p_resource_key) {
    RefRes<RUGraphicsPipeline> result = p_storage.make_object<RUGraphicsPipeline>();
    result->resource_key = p_resource_key;
    return result;
}

class WBERenderDatabaseTest : public ::testing::Test {
protected:
    RenderAllocator render_allocator{WBE_KI_B(128)};
    RenderDatabaseTestResourceStorage resource_storage;
    RenderDatabase render_database{&render_allocator};
};

TEST_F(WBERenderDatabaseTest, RegistersCompiledRenderResourceUsingConcreteObjectType) {
    Ref<RenderDatabaseTestRenderObject> render_object = make_ref<RenderDatabaseTestRenderObject>(&render_allocator);

    render_database.register_compiled_render_resource("test_resource", render_object);

    EXPECT_TRUE(render_database.contains_compiled_render_resource("test_resource", render_object->get_type_uuid()));
    EXPECT_EQ(render_database.get_compiled_render_resource("test_resource", render_object->get_type_uuid()), render_object);
}

TEST_F(WBERenderDatabaseTest, StoresStaticGeometrySetReferences) {
    RefRes<RUStaticOpaqueSet> opaque_set = resource_storage.make_object<RUStaticOpaqueSet>();
    RefRes<RUStaticOpaqueDoubleSidedSet> opaque_double_sided_set = resource_storage.make_object<RUStaticOpaqueDoubleSidedSet>();
    RefRes<RUStaticMaskedSet> masked_set = resource_storage.make_object<RUStaticMaskedSet>();

    render_database.set_static_opaque_set(opaque_set);
    render_database.set_static_opaque_double_sided_set(opaque_double_sided_set);
    render_database.set_static_masked_set(masked_set);

    EXPECT_EQ(render_database.get_static_opaque_set(), opaque_set);
    EXPECT_EQ(render_database.get_static_opaque_double_sided_set(), opaque_double_sided_set);
    EXPECT_EQ(render_database.get_static_masked_set(), masked_set);

    render_database.set_static_opaque_set(MEM_NULL);
    render_database.set_static_opaque_double_sided_set(MEM_NULL);
    render_database.set_static_masked_set(MEM_NULL);
    EXPECT_EQ(render_database.get_static_opaque_set(), MEM_NULL);
    EXPECT_EQ(render_database.get_static_opaque_double_sided_set(), MEM_NULL);
    EXPECT_EQ(render_database.get_static_masked_set(), MEM_NULL);
}

TEST_F(WBERenderDatabaseTest, FlattensDynamicLayoutAwareDrawGroupsAndReusesMaterials) {
    RefRes<RUGraphicsPipeline> pipeline_a = make_pipeline(resource_storage, "pipeline_a");
    RefRes<RUGraphicsPipeline> pipeline_b = make_pipeline(resource_storage, "pipeline_b");
    RefRes<RUMaterial> material_a = make_material(resource_storage, "material_a", pipeline_a);
    RefRes<RUMaterial> material_b = make_material(resource_storage, "material_b", pipeline_a);
    RefRes<RUMaterial> material_c = make_material(resource_storage, "material_c", pipeline_b);

    const std::vector<uint8_t> position_vertices_a =
        make_vertex_bytes(std::array<float, 9>{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F});
    const std::vector<uint8_t> position_vertices_b =
        make_vertex_bytes(std::array<float, 9>{10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F, 17.0F, 18.0F});
    const std::vector<uint8_t> position_vertices_c =
        make_vertex_bytes(std::array<float, 9>{19.0F, 20.0F, 21.0F, 22.0F, 23.0F, 24.0F, 25.0F, 26.0F, 27.0F});
    const std::vector<uint8_t> position_vertices_d =
        make_vertex_bytes(std::array<float, 9>{28.0F, 29.0F, 30.0F, 31.0F, 32.0F, 33.0F, 34.0F, 35.0F, 36.0F});
    const std::vector<uint8_t> uv_vertices = make_vertex_bytes(std::array<float, 6>{0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F});

    RUVertexView position_a =
        make_vertex_data(resource_storage, "position", RUVertexAttributeFormat::FLOAT3, sizeof(float) * 3, 3, position_vertices_a);
    RUVertexView position_b =
        make_vertex_data(resource_storage, "position", RUVertexAttributeFormat::FLOAT3, sizeof(float) * 3, 3, position_vertices_b);
    RUVertexView position_c =
        make_vertex_data(resource_storage, "position", RUVertexAttributeFormat::FLOAT3, sizeof(float) * 3, 3, position_vertices_c);
    RUVertexView position_d =
        make_vertex_data(resource_storage, "position", RUVertexAttributeFormat::FLOAT3, sizeof(float) * 3, 3, position_vertices_d);
    RUVertexView uv = make_vertex_data(resource_storage, "uv", RUVertexAttributeFormat::FLOAT2, sizeof(float) * 2, 3, uv_vertices);

    RefRes<RUSubmesh3D> material_a_position_first =
        make_submesh(resource_storage, material_a, position_a, std::array<uint32_t, 3>{0, 1, 2});
    RefRes<RUSubmesh3D> material_a_position_second =
        make_submesh(resource_storage, material_a, position_b, std::array<uint32_t, 3>{0, 2, 1});
    RefRes<RUSubmesh3D> material_a_uv = make_submesh(resource_storage, material_a, uv, std::array<uint32_t, 3>{0, 1, 2});
    RefRes<RUSubmesh3D> material_b_position =
        make_submesh(resource_storage, material_b, position_c, std::array<uint32_t, 3>{0, 1, 2});
    RefRes<RUSubmesh3D> material_c_position =
        make_submesh(resource_storage, material_c, position_d, std::array<uint32_t, 3>{0, 1, 2});

    const std::array<RefRes<RUSubmesh3D>, 5> submeshes{
        material_c_position, material_a_position_first, material_b_position, material_a_uv, material_a_position_second};
    RefRes<RUMesh3D> mesh = resource_storage.make_object<RUMesh3D>();
    mesh->submeshes = resource_storage.make_array<RefRes<RUSubmesh3D>>(submeshes);
    mesh->submesh_count = submeshes.size();

    render_database.set_dynamic_opaque_triangles(mesh.get());
    const RenderDatabase::OpaqueTrianglesInfo& info = render_database.get_dynamic_opaque_triangle_vertices();

    ASSERT_EQ(info.materials.size(), 3U);
    EXPECT_EQ(info.materials[0].material, material_a);
    EXPECT_EQ(info.materials[1].material, material_b);
    EXPECT_EQ(info.materials[2].material, material_c);

    ASSERT_EQ(info.draw_groups.size(), 4U);
    EXPECT_EQ(info.draw_groups[0].material_index, 0U);
    EXPECT_EQ(info.draw_groups[0].vertex_buffer_byte_offset, 0U);
    EXPECT_EQ(info.draw_groups[0].index_sub_region.starting_index, 0U);
    EXPECT_EQ(info.draw_groups[0].index_sub_region.size, 3U);
    EXPECT_EQ(info.draw_groups[0].indirect_command_sub_region.starting_index, 0U);
    EXPECT_EQ(info.draw_groups[0].indirect_command_sub_region.size, 1U);
    EXPECT_EQ(info.draw_groups[0].vertex_layout.vertex_stride, sizeof(float) * 2);
    ASSERT_EQ(info.draw_groups[0].vertex_layout.attributes.size(), 1U);
    EXPECT_EQ(info.draw_groups[0].vertex_layout.attributes[0].role.get_name(), "uv");

    EXPECT_EQ(info.draw_groups[1].material_index, 0U);
    EXPECT_EQ(info.draw_groups[1].vertex_buffer_byte_offset, uv_vertices.size());
    EXPECT_EQ(info.draw_groups[1].index_sub_region.starting_index, 3U);
    EXPECT_EQ(info.draw_groups[1].index_sub_region.size, 6U);
    EXPECT_EQ(info.draw_groups[1].vertex_layout.vertex_stride, sizeof(float) * 3);
    EXPECT_EQ(info.draw_groups[2].material_index, 1U);
    EXPECT_EQ(info.draw_groups[2].vertex_buffer_byte_offset,
        uv_vertices.size() + position_vertices_a.size() + position_vertices_b.size());
    EXPECT_EQ(info.draw_groups[2].index_sub_region.starting_index, 9U);
    EXPECT_EQ(info.draw_groups[3].material_index, 2U);
    EXPECT_EQ(info.draw_groups[3].vertex_buffer_byte_offset,
        uv_vertices.size() + position_vertices_a.size() + position_vertices_b.size() + position_vertices_c.size());
    EXPECT_EQ(info.draw_groups[3].index_sub_region.starting_index, 12U);

    const std::array<uint32_t, 15> expected_indices{0, 1, 2, 0, 1, 2, 3, 5, 4, 0, 1, 2, 0, 1, 2};
    ASSERT_EQ(info.indices.size(), expected_indices.size());
    EXPECT_TRUE(std::equal(info.indices.begin(), info.indices.end(), expected_indices.begin()));

    std::vector<uint8_t> expected_vertices;
    append_bytes(expected_vertices, uv_vertices);
    append_bytes(expected_vertices, position_vertices_a);
    append_bytes(expected_vertices, position_vertices_b);
    append_bytes(expected_vertices, position_vertices_c);
    append_bytes(expected_vertices, position_vertices_d);
    ASSERT_EQ(info.vertices.size(), expected_vertices.size());
    EXPECT_TRUE(std::equal(info.vertices.begin(), info.vertices.end(), expected_vertices.begin()));

    ASSERT_EQ(info.graphics_pipelines.size(), 2U);
    EXPECT_EQ(info.graphics_pipelines[0].graphics_pipeline, pipeline_a);
    EXPECT_EQ(info.graphics_pipelines[0].draw_group_sub_region.starting_index, 0U);
    EXPECT_EQ(info.graphics_pipelines[0].draw_group_sub_region.size, 3U);
    EXPECT_EQ(info.graphics_pipelines[1].graphics_pipeline, pipeline_b);
    EXPECT_EQ(info.graphics_pipelines[1].draw_group_sub_region.starting_index, 3U);
    EXPECT_EQ(info.graphics_pipelines[1].draw_group_sub_region.size, 1U);
    EXPECT_EQ(info.obj_id.size(), submeshes.size());

    const glm::vec3 updated_position(40.0F, 41.0F, 42.0F);
    material_a_position_first->set_vertex_attribute(0, "position", updated_position);
    glm::vec3 flattened_position{};
    std::memcpy(&flattened_position, info.vertices.data() + info.draw_groups[1].vertex_buffer_byte_offset, sizeof(flattened_position));
    EXPECT_NE(flattened_position, updated_position);

    render_database.set_dynamic_opaque_triangles(mesh.get());
    std::memcpy(&flattened_position, info.vertices.data() + info.draw_groups[1].vertex_buffer_byte_offset, sizeof(flattened_position));
    EXPECT_EQ(flattened_position, updated_position);
}

TEST_F(WBERenderDatabaseTest, IgnoresEmptyDynamicIndexGroupsWithoutRegisteringTheirMaterials) {
    RefRes<RUGraphicsPipeline> pipeline = make_pipeline(resource_storage, "pipeline");
    RefRes<RUMaterial> material = make_material(resource_storage, "material", pipeline);
    const std::vector<uint8_t> vertices = make_vertex_bytes(std::array<float, 3>{1.0F, 2.0F, 3.0F});
    RUVertexView vertex_data =
        make_vertex_data(resource_storage, "position", RUVertexAttributeFormat::FLOAT3, sizeof(float) * 3, 1, vertices);
    RefRes<RUSubmesh3D> submesh = resource_storage.make_object<RUSubmesh3D>();
    submesh->material = material;
    submesh->vertices = vertex_data;
    submesh->indices = make_index_data(resource_storage, std::span<const uint32_t>());
    const std::array<RefRes<RUSubmesh3D>, 1> submeshes{submesh};
    RefRes<RUMesh3D> mesh = resource_storage.make_object<RUMesh3D>();
    mesh->submeshes = resource_storage.make_array<RefRes<RUSubmesh3D>>(submeshes);
    mesh->submesh_count = submeshes.size();

    render_database.set_dynamic_opaque_triangles(mesh.get());

    const RenderDatabase::OpaqueTrianglesInfo& info = render_database.get_dynamic_opaque_triangle_vertices();
    EXPECT_TRUE(info.vertices.empty());
    EXPECT_TRUE(info.indices.empty());
    EXPECT_TRUE(info.obj_id.empty());
    EXPECT_TRUE(info.materials.empty());
    EXPECT_TRUE(info.draw_groups.empty());
    EXPECT_TRUE(info.graphics_pipelines.empty());
}

TEST_F(WBERenderDatabaseTest, RejectsOutOfRangeDynamicLocalIndex) {
    RefRes<RUGraphicsPipeline> pipeline = make_pipeline(resource_storage, "pipeline");
    RefRes<RUMaterial> material = make_material(resource_storage, "material", pipeline);
    const std::vector<uint8_t> vertices = make_vertex_bytes(std::array<float, 3>{1.0F, 2.0F, 3.0F});
    RUVertexView vertex_data =
        make_vertex_data(resource_storage, "position", RUVertexAttributeFormat::FLOAT3, sizeof(float) * 3, 1, vertices);
    RefRes<RUSubmesh3D> submesh = make_submesh(resource_storage, material, vertex_data, std::array<uint32_t, 3>{0, 1, 0});
    const std::array<RefRes<RUSubmesh3D>, 1> submeshes{submesh};
    RefRes<RUMesh3D> mesh = resource_storage.make_object<RUMesh3D>();
    mesh->submeshes = resource_storage.make_array<RefRes<RUSubmesh3D>>(submeshes);
    mesh->submesh_count = submeshes.size();

    EXPECT_THROW(render_database.set_dynamic_opaque_triangles(mesh.get()), std::runtime_error);
}

} // namespace WhiteBirdEngine
