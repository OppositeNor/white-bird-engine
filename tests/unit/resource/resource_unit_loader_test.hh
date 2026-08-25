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
#ifndef WBE_FILE_RESOURCE_POPULATOR_TEST_HH
#define WBE_FILE_RESOURCE_POPULATOR_TEST_HH

#include "core/allocator/i_allocator.hh"
#include "core/allocator/stack_allocator.hh"
#include "core/engine_core.hh"
#include "core/parser/parser_json.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_vk_resource_unit_loader.hh"
#include "function/scene/scene_resource_unit_loader.hh"
#include "glm/ext/vector_float4.hpp"
#include "glm/ext/vector_int2.hpp"
#include "glm/ext/vector_int3.hpp"
#include "glm/ext/vector_int4.hpp"
#include "glm/ext/vector_uint2.hpp"
#include "glm/ext/vector_uint3.hpp"
#include "glm/ext/vector_uint4.hpp"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include "resource/resource_utils.hh"
#include "resource_test_utils.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace WBE = WhiteBirdEngine;

namespace ResourceUnitLoaderTestSupport {

struct TrivialTestResourceUnit : public WBE::ResourceUnit {
    TrivialTestResourceUnit() = default;

    explicit TrivialTestResourceUnit(int p_value) : value(p_value) {
    }

    int value = 0;
};

struct NonTrivialTestResourceUnit : public WBE::ResourceUnit {
    NonTrivialTestResourceUnit(int* p_construct_count, int* p_destroy_count, int p_value)
        : construct_count(p_construct_count), destroy_count(p_destroy_count), value(p_value) {
        if (construct_count != nullptr) {
            ++(*construct_count);
        }
    }

    ~NonTrivialTestResourceUnit() {
        if (destroy_count != nullptr) {
            ++(*destroy_count);
        }
    }

    NonTrivialTestResourceUnit(const NonTrivialTestResourceUnit&) = default;
    NonTrivialTestResourceUnit(NonTrivialTestResourceUnit&&) noexcept = default;
    NonTrivialTestResourceUnit& operator=(const NonTrivialTestResourceUnit&) = default;
    NonTrivialTestResourceUnit& operator=(NonTrivialTestResourceUnit&&) noexcept = default;

    int* construct_count = nullptr;
    int* destroy_count = nullptr;
    int value = 0;
};

struct OrderedDestroyTestResourceUnit : public WBE::ResourceUnit {
    OrderedDestroyTestResourceUnit(std::vector<int>* p_destroy_order, int p_id) : destroy_order(p_destroy_order), id(p_id) {
    }

    ~OrderedDestroyTestResourceUnit() {
        if (destroy_order != nullptr) {
            destroy_order->push_back(id);
        }
    }

    OrderedDestroyTestResourceUnit(const OrderedDestroyTestResourceUnit&) = default;
    OrderedDestroyTestResourceUnit(OrderedDestroyTestResourceUnit&&) noexcept = default;
    OrderedDestroyTestResourceUnit& operator=(const OrderedDestroyTestResourceUnit&) = default;
    OrderedDestroyTestResourceUnit& operator=(OrderedDestroyTestResourceUnit&&) noexcept = default;

    std::vector<int>* destroy_order = nullptr;
    int id = -1;
};

} // namespace ResourceUnitLoaderTestSupport

class ResourcePopulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
        text_scheme.path = "test_text_files/test_file_utf8.txt";
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<MockResourceChunk> create_submesh_test_chunk(
        WBE::StackAllocator& p_stack_allocator, std::ostream& p_stream, WBE::RefRes<WBE::RUSubmesh3D>& p_captured_submesh) {
        return MockResourceChunk::Builder()
            .set_push_data_callback([&p_stack_allocator](size_t p_size, const void* p_data) -> WBE::RefRes<char> {
                if (p_size == 0) {
                    return WBE::MEM_NULL;
                }
                WBE::MemID mem_id = p_stack_allocator.allocate(p_size);
                std::memcpy(p_stack_allocator.get(mem_id), p_data, p_size);
                return WBE::RefRes<char>(mem_id, &p_stack_allocator);
            })
            .set_reserve_res_unit_storage_callback(
                [&p_stack_allocator, &p_captured_submesh](std::string_view p_id, size_t p_size) -> WBE::RefRes<char> {
                    WBE::MemID mem_id = p_stack_allocator.allocate(p_size);
                    WBE::RefRes<char> resource(mem_id, &p_stack_allocator);
                    if (p_id == "loaded_submesh") {
                        p_captured_submesh = resource.force_cast<WBE::RUSubmesh3D>();
                    }
                    return resource;
                })
            .build(p_stream);
    }

    WBE::RUSchemaSubmesh3D make_triangle_submesh_scheme() {
        WBE::RUSchemaSubmesh3D scheme;
        scheme.id = "loaded_submesh";
        scheme.type = "submesh";
        scheme.material_id = "unused_material";
        scheme.geometry_path = "assets/meshes/triangle.submesh.0.geometry.bin";
        scheme.geometry_sections = {
            {.slot = "position", .start = 0, .size = 36, .type = "vec3"},
            {.slot = "uv", .start = 36, .size = 24, .type = "vec2"},
            {.slot = "index", .start = 60, .size = 12, .type = "uint32"},
        };
        return scheme;
    }

    std::unique_ptr<WBE::Global> global;
    WBE::RUSchemaText text_scheme;
    std::string test_file_content = R"txt(这是一个测试文本。
English text is also supported.
日本語テキストもサポート。
한국어 텍스트도 지원됩니다.
Текст на русском тоже поддерживается.
Le texte français est également pris en charge.
El texto en español también está soportado.
Der deutsche Text wird ebenfalls unterstützt.
Il testo in italiano è supportato.
Português também é suportado.
النص العربي مدعوم أيضًا.
טקסט בעברית נתמך גם כן.
हिंदी पाठ भी समर्थित है।
ภาษาไทยก็รองรับเช่นกัน。
ქართული ტექსტიც მხარდაჭერილია.
Ελληνικό κείμενο υποστηρίζεται επίσης.
Türkçe metin de desteklenmektedir.
Український текст також підтримується.
Tiếng Việt cũng được hỗ trợ.
Emoji 测试: 😃🚀✨🌍
数学字符: ∀x∈ℝ, x² ≥ 0
组合字符: á é í ó ú
)txt";
};

class RefResHolder {
public:
    RefResHolder() : mem_id(WBE::global_allocator()->allocate(WBE_KI_B(4))), res(mem_id, WBE::global_allocator()) {
    }
    ~RefResHolder() {
        WBE::global_allocator()->deallocate(mem_id);
    }
    RefResHolder(const RefResHolder&) = delete;
    RefResHolder(RefResHolder&&) = delete;
    RefResHolder& operator=(const RefResHolder&) = delete;
    RefResHolder& operator=(RefResHolder&&) = delete;
    WBE::MemID mem_id;
    WBE::RefRes<char> res;
};

TEST_F(ResourcePopulatorTest, SupportedResourceTypesUseStaticReadOnlyStorage) {
    WBE::ResourceUnitLoaderAudio audio_loader;
    WBE::ResourceUnitLoaderBinary binary_loader;
    WBE::ResourceUnitLoaderFont font_loader;
    WBE::ResourceUnitLoaderImage image_loader;
    WBE::ResourceUnitLoaderText text_loader;
    WBE::ResourceUnitLoaderVideo video_loader;
    WBE::ResourceUnitLoaderShader shader_loader;
    WBE::ResourceUnitLoaderGraphicsPipeline graphics_pipeline_loader;
    WBE::ResourceUnitLoaderMaterial material_loader;
    WBE::ResourceUnitLoaderMesh mesh_loader;
    WBE::ResourceUnitLoaderSubmesh submesh_loader;
    WBE::ResourceUnitLoaderComponent component_loader;
    WBE::ResourceUnitLoaderPrefab prefab_loader;
    WBE::ResourceUnitLoaderWorld world_loader;
    WBE::ResourceUnitLoaderScene scene_loader;
    const std::array<const WBE::IResourceUnitLoader*, 15> loaders = {&audio_loader,
        &binary_loader,
        &font_loader,
        &image_loader,
        &text_loader,
        &video_loader,
        &shader_loader,
        &graphics_pipeline_loader,
        &material_loader,
        &mesh_loader,
        &submesh_loader,
        &component_loader,
        &prefab_loader,
        &world_loader,
        &scene_loader};
    const std::array<std::string_view, 15> expected_resource_types = {
        "audio", "binary", "font", "image", "text", "video", "shader", "graphics_pipeline", "material", "mesh", "submesh", "component", "prefab", "world", "scene"};

    for (size_t i = 0; i < loaders.size(); ++i) {
        auto first_resource_types = loaders[i]->get_supported_resource_types();
        auto second_resource_types = loaders[i]->get_supported_resource_types();
        static_assert(std::is_const_v<decltype(first_resource_types)::element_type>);

        ASSERT_EQ(first_resource_types.size(), 1);
        EXPECT_EQ(first_resource_types.front(), expected_resource_types[i]);
        EXPECT_EQ(first_resource_types.data(), second_resource_types.data());
    }
}

TEST_F(ResourcePopulatorTest, SubmeshGeometrySectionsBecomeInterleavedVertexData) {
    WBE::StackAllocator stack_allocator(8192);
    std::stringstream stream;
    WBE::RefRes<WBE::RUSubmesh3D> captured_submesh = WBE::MEM_NULL;
    auto resource_chunk = create_submesh_test_chunk(stack_allocator, stream, captured_submesh);
    WBE::RUSchemaSubmesh3D scheme = make_triangle_submesh_scheme();
    WBE::ResourceUnitLoaderSubmesh loader;

    loader.populate_scheme(*resource_chunk, scheme);

    ASSERT_NE(captured_submesh, nullptr);
    ASSERT_NE(captured_submesh->vertex_data, nullptr);
    const WBE::RUVertexData& vertex_data = *captured_submesh->vertex_data;
    EXPECT_EQ(vertex_data.vertex_count, 3U);
    EXPECT_EQ(vertex_data.vertex_stride, sizeof(float) * 5);
    EXPECT_EQ(vertex_data.vertices_size, sizeof(float) * 15);
    ASSERT_EQ(vertex_data.attribute_count, 2U);
    ASSERT_NE(vertex_data.attributes, nullptr);
    EXPECT_STREQ(vertex_data.attributes->role.name.get(), "position");
    EXPECT_EQ(vertex_data.attributes->role.hash, WBE::dynam_hash("position"));
    EXPECT_EQ(vertex_data.attributes->format, WBE::RUVertexAttributeFormat::FLOAT3);
    EXPECT_EQ(vertex_data.attributes->offset, 0U);
    EXPECT_EQ(vertex_data.attributes->size, sizeof(float) * 3);
    EXPECT_STREQ(vertex_data.attributes.get()[1].role.name.get(), "uv");
    EXPECT_EQ(vertex_data.attributes.get()[1].role.hash, WBE::dynam_hash("uv"));
    EXPECT_EQ(vertex_data.attributes.get()[1].format, WBE::RUVertexAttributeFormat::FLOAT2);
    EXPECT_EQ(vertex_data.attributes.get()[1].offset, sizeof(float) * 3);
    EXPECT_EQ(vertex_data.attributes.get()[1].size, sizeof(float) * 2);

    const std::array<std::array<float, 5>, 3> expected_vertices{{
        {{0.0F, 0.5F, 0.0F, 0.5F, 1.0F}},
        {{-0.5F, -0.5F, 0.0F, 0.0F, 0.0F}},
        {{0.5F, -0.5F, 0.0F, 1.0F, 0.0F}},
    }};
    for (size_t vertex_index = 0; vertex_index < expected_vertices.size(); ++vertex_index) {
        std::array<float, 5> actual_vertex{};
        std::memcpy(actual_vertex.data(), vertex_data.vertices.get() + vertex_index * vertex_data.vertex_stride, vertex_data.vertex_stride);
        for (size_t component_index = 0; component_index < actual_vertex.size(); ++component_index) {
            EXPECT_FLOAT_EQ(actual_vertex[component_index], expected_vertices[vertex_index][component_index]);
        }
    }

    ASSERT_EQ(captured_submesh->index_count, 3U);
    ASSERT_NE(captured_submesh->indices, nullptr);
    EXPECT_EQ(captured_submesh->indices.get()[0], 0U);
    EXPECT_EQ(captured_submesh->indices.get()[1], 1U);
    EXPECT_EQ(captured_submesh->indices.get()[2], 2U);
    EXPECT_EQ(captured_submesh->get_vertex_attribute_offset("uv"), sizeof(float) * 3);
    EXPECT_EQ(captured_submesh->get_vertex_stride(), sizeof(float) * 5);
    EXPECT_EQ(captured_submesh->get_vertices_num(), 3U);

    captured_submesh->set_vertex_attribute(1, "position", glm::vec3(2.0F, 3.0F, 4.0F));
    captured_submesh->set_vertex_attribute(1, "uv", glm::vec2(5.0F, 6.0F));
    std::array<float, 5> changed_vertex{};
    std::memcpy(changed_vertex.data(), vertex_data.vertices.get() + vertex_data.vertex_stride, vertex_data.vertex_stride);
    EXPECT_EQ(changed_vertex, (std::array<float, 5>{{2.0F, 3.0F, 4.0F, 5.0F, 6.0F}}));
    EXPECT_THROW(captured_submesh->set_vertex_attribute(3, "position", glm::vec3(0.0F)), std::runtime_error);
    EXPECT_THROW(captured_submesh->set_vertex_attribute(0, "missing", glm::vec3(0.0F)), std::runtime_error);
    EXPECT_THROW(captured_submesh->set_vertex_attribute(0, "position", glm::vec2(0.0F)), std::runtime_error);

    captured_submesh->swap_vertices(0, 1);
    std::array<float, 5> first_vertex{};
    std::memcpy(first_vertex.data(), vertex_data.vertices.get(), vertex_data.vertex_stride);
    EXPECT_EQ(first_vertex, changed_vertex);
    EXPECT_THROW(captured_submesh->swap_vertices(0, 3), std::runtime_error);
}

TEST_F(ResourcePopulatorTest, SubmeshGeometrySupportsEveryVertexAttributeFormat) {
    struct FormatCase {
        std::string_view type;
        WBE::RUVertexAttributeFormat format;
        size_t element_size;
        size_t section_start;
    };
    const std::array<FormatCase, 12> format_cases{{
        {.type = "float", .format = WBE::RUVertexAttributeFormat::FLOAT, .element_size = 4, .section_start = 0},
        {.type = "vec2", .format = WBE::RUVertexAttributeFormat::FLOAT2, .element_size = 8, .section_start = 36},
        {.type = "vec3", .format = WBE::RUVertexAttributeFormat::FLOAT3, .element_size = 12, .section_start = 0},
        {.type = "vec4", .format = WBE::RUVertexAttributeFormat::FLOAT4, .element_size = 16, .section_start = 0},
        {.type = "int", .format = WBE::RUVertexAttributeFormat::INT, .element_size = 4, .section_start = 0},
        {.type = "ivec2", .format = WBE::RUVertexAttributeFormat::INT2, .element_size = 8, .section_start = 36},
        {.type = "ivec3", .format = WBE::RUVertexAttributeFormat::INT3, .element_size = 12, .section_start = 0},
        {.type = "ivec4", .format = WBE::RUVertexAttributeFormat::INT4, .element_size = 16, .section_start = 0},
        {.type = "uint", .format = WBE::RUVertexAttributeFormat::UINT, .element_size = 4, .section_start = 0},
        {.type = "uvec2", .format = WBE::RUVertexAttributeFormat::UINT2, .element_size = 8, .section_start = 36},
        {.type = "uvec3", .format = WBE::RUVertexAttributeFormat::UINT3, .element_size = 12, .section_start = 0},
        {.type = "uvec4", .format = WBE::RUVertexAttributeFormat::UINT4, .element_size = 16, .section_start = 0},
    }};
    WBE::ResourceUnitLoaderSubmesh loader;

    for (const FormatCase& format_case : format_cases) {
        WBE::StackAllocator stack_allocator(8192);
        std::stringstream stream;
        WBE::RefRes<WBE::RUSubmesh3D> captured_submesh = WBE::MEM_NULL;
        auto resource_chunk = create_submesh_test_chunk(stack_allocator, stream, captured_submesh);
        WBE::RUSchemaSubmesh3D scheme = make_triangle_submesh_scheme();
        scheme.geometry_sections = {
            {.slot = "custom_role",
                .start = format_case.section_start,
                .size = format_case.element_size * 3,
                .type = std::string(format_case.type)},
            {.slot = "index", .start = 60, .size = 12, .type = "uint32"},
        };

        loader.populate_scheme(*resource_chunk, scheme);

        ASSERT_NE(captured_submesh, nullptr);
        ASSERT_NE(captured_submesh->vertex_data, nullptr);
        ASSERT_EQ(captured_submesh->vertex_data->attribute_count, 1U);
        EXPECT_EQ(captured_submesh->vertex_data->attributes->format, format_case.format);
        EXPECT_EQ(captured_submesh->vertex_data->attributes->size, format_case.element_size);
        EXPECT_EQ(captured_submesh->vertex_data->vertex_stride, format_case.element_size);
        EXPECT_EQ(captured_submesh->vertex_data->vertex_count, 3U);

        switch (format_case.format) {
        case WBE::RUVertexAttributeFormat::FLOAT:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", 1.0F));
            break;
        case WBE::RUVertexAttributeFormat::FLOAT2:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", glm::vec2(1.0F)));
            break;
        case WBE::RUVertexAttributeFormat::FLOAT3:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", glm::vec3(1.0F)));
            break;
        case WBE::RUVertexAttributeFormat::FLOAT4:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", glm::vec4(1.0F)));
            break;
        case WBE::RUVertexAttributeFormat::INT:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", int32_t{1}));
            break;
        case WBE::RUVertexAttributeFormat::INT2:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", glm::ivec2(1)));
            break;
        case WBE::RUVertexAttributeFormat::INT3:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", glm::ivec3(1)));
            break;
        case WBE::RUVertexAttributeFormat::INT4:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", glm::ivec4(1)));
            break;
        case WBE::RUVertexAttributeFormat::UINT:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", uint32_t{1}));
            break;
        case WBE::RUVertexAttributeFormat::UINT2:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", glm::uvec2(1U)));
            break;
        case WBE::RUVertexAttributeFormat::UINT3:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", glm::uvec3(1U)));
            break;
        case WBE::RUVertexAttributeFormat::UINT4:
            EXPECT_NO_THROW(captured_submesh->set_vertex_attribute(0, "custom_role", glm::uvec4(1U)));
            break;
        }
    }
}

TEST_F(ResourcePopulatorTest, SubmeshGeometryRejectsInvalidLayouts) {
    WBE::ResourceUnitLoaderSubmesh loader;
    auto expect_invalid = [this, &loader](std::vector<WBE::RUSchemaSubmesh3DGeometrySection> p_sections) {
        WBE::StackAllocator stack_allocator(8192);
        std::stringstream stream;
        WBE::RefRes<WBE::RUSubmesh3D> captured_submesh = WBE::MEM_NULL;
        auto resource_chunk = create_submesh_test_chunk(stack_allocator, stream, captured_submesh);
        WBE::RUSchemaSubmesh3D scheme = make_triangle_submesh_scheme();
        scheme.geometry_sections = std::move(p_sections);
        EXPECT_THROW(loader.populate_scheme(*resource_chunk, scheme), std::runtime_error);
    };

    expect_invalid({
        {.slot = "position", .start = 0, .size = 36, .type = "vec3"},
        {.slot = "position", .start = 36, .size = 24, .type = "vec2"},
        {.slot = "index", .start = 60, .size = 12, .type = "uint32"},
    });
    static_assert(WBE::dynam_hash("role_002876ae4c05a223") == WBE::dynam_hash("role_2ffa43e2b7d24f56"));
    expect_invalid({
        {.slot = "role_002876ae4c05a223", .start = 0, .size = 36, .type = "vec3"},
        {.slot = "role_2ffa43e2b7d24f56", .start = 0, .size = 36, .type = "vec3"},
        {.slot = "index", .start = 60, .size = 12, .type = "uint32"},
    });
    expect_invalid({
        {.slot = "position", .start = 0, .size = 36, .type = "vec3"},
        {.slot = "uv", .start = 36, .size = 16, .type = "vec2"},
        {.slot = "index", .start = 60, .size = 12, .type = "uint32"},
    });
    expect_invalid({
        {.slot = "position", .start = 0, .size = 36, .type = "double"},
        {.slot = "index", .start = 60, .size = 12, .type = "uint32"},
    });
    expect_invalid({
        {.slot = "index", .start = 60, .size = 12, .type = "uint32"},
    });
    expect_invalid({
        {.slot = "position", .start = 0, .size = 36, .type = "vec3"},
    });
    expect_invalid({
        {.slot = "position", .start = 72, .size = 12, .type = "vec3"},
        {.slot = "index", .start = 60, .size = 12, .type = "uint32"},
    });
    expect_invalid({
        {.slot = "position", .start = 0, .size = 36, .type = "vec3"},
        {.slot = "index", .start = 60, .size = 12, .type = "uint"},
    });
    expect_invalid({
        {.slot = "position", .start = 0, .size = 36, .type = "vec3"},
        {.slot = "index", .start = 0, .size = 12, .type = "uint32"},
    });
    expect_invalid({
        {.slot = "", .start = 0, .size = 36, .type = "vec3"},
        {.slot = "index", .start = 60, .size = 12, .type = "uint32"},
    });
}

TEST_F(ResourcePopulatorTest, EmplaceResUnitRegistersDestructorForNonTrivialResourceUnit) {
    std::stringstream stream;
    WBE::StackAllocator stack_allocator(1024);
    void* registered_resource = nullptr;
    void (*registered_destructor)(void*) = nullptr;
    int construct_count = 0;
    int destroy_count = 0;

    auto res_chunk =
        MockResourceChunk::Builder()
            .set_reserve_res_unit_storage_callback([&stack_allocator](std::string_view, size_t p_size) -> WBE::RefRes<char> {
                WBE::MemID mem_id = stack_allocator.allocate(p_size);
                return WBE::RefRes<char>(mem_id, &stack_allocator);
            })
            .set_register_res_unit_destructor_callback(
                [&registered_resource, &registered_destructor](void* p_resource, void (*p_destructor)(void*)) {
                    registered_resource = p_resource;
                    registered_destructor = p_destructor;
                })
            .build(stream);

    auto resource = res_chunk->emplace_res_unit<ResourceUnitLoaderTestSupport::NonTrivialTestResourceUnit>(
        "typed_resource", &construct_count, &destroy_count, 17);

    ASSERT_NE(resource, nullptr);
    ASSERT_NE(resource->resource_key, nullptr);
    EXPECT_STREQ(resource->resource_key, "typed_resource");
    EXPECT_EQ(resource->value, 17);
    EXPECT_EQ(construct_count, 1);
    ASSERT_NE(registered_resource, nullptr);
    ASSERT_NE(registered_destructor, nullptr);
    EXPECT_EQ(registered_resource, resource.get());
    EXPECT_EQ(destroy_count, 0);

    registered_destructor(registered_resource);
    EXPECT_EQ(destroy_count, 1);
}

TEST_F(ResourcePopulatorTest, EmplaceResUnitSkipsDestructorRegistrationForTrivialResourceUnit) {
    std::stringstream stream;
    WBE::StackAllocator stack_allocator(1024);
    int register_destructor_count = 0;

    auto res_chunk =
        MockResourceChunk::Builder()
            .set_reserve_res_unit_storage_callback([&stack_allocator](std::string_view, size_t p_size) -> WBE::RefRes<char> {
                WBE::MemID mem_id = stack_allocator.allocate(p_size);
                return WBE::RefRes<char>(mem_id, &stack_allocator);
            })
            .set_register_res_unit_destructor_callback([&register_destructor_count](void*, void (*)(void*)) {
                ++register_destructor_count;
            })
            .build(stream);

    auto resource = res_chunk->emplace_res_unit<ResourceUnitLoaderTestSupport::TrivialTestResourceUnit>("trivial_resource", 23);

    ASSERT_NE(resource, nullptr);
    ASSERT_NE(resource->resource_key, nullptr);
    EXPECT_STREQ(resource->resource_key, "trivial_resource");
    EXPECT_EQ(resource->value, 23);
    EXPECT_EQ(register_destructor_count, 0);
}

TEST_F(ResourcePopulatorTest, ResourceChunkLoadFileDestroysNonTrivialResourceUnitsInReverseOrder) {
    WBE::StackAllocator stack_allocator(64 * 1024);
    std::vector<int> destroy_order;

    {
        WBE::ResourceChunkLoadFile resource_chunk("res_chunks/empty_resource_chunk.json", stack_allocator);
        auto first_resource = resource_chunk.emplace_res_unit<ResourceUnitLoaderTestSupport::OrderedDestroyTestResourceUnit>(
            "destroy_order_first", &destroy_order, 1);
        auto second_resource = resource_chunk.emplace_res_unit<ResourceUnitLoaderTestSupport::OrderedDestroyTestResourceUnit>(
            "destroy_order_second", &destroy_order, 2);

        ASSERT_NE(first_resource, nullptr);
        ASSERT_NE(second_resource, nullptr);
        EXPECT_TRUE(destroy_order.empty());
    }

    ASSERT_EQ(destroy_order.size(), 2U);
    EXPECT_EQ(destroy_order[0], 2);
    EXPECT_EQ(destroy_order[1], 1);
}

TEST_F(ResourcePopulatorTest, TextResourceGeneral) {
    WBE::ResourceUnitLoaderText loader;
    std::stringstream ss;
    std::string text_content_storage;
    WBE::StackAllocator stack_allocator(1024);
    WBE::RefRes<WBE::RUText> captured_ru_text = WBE::MEM_NULL;
    auto res_chunk = MockResourceChunk::Builder()
                         .set_push_data_callback(std::function<WBE::RefRes<char>(size_t, const void*)>(
                             [&text_content_storage](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
                                 text_content_storage.resize(p_res_size - 1);
                                 std::memcpy(text_content_storage.data(), p_res, p_res_size - 1);
                                 EXPECT_EQ(static_cast<const char*>(p_res)[p_res_size - 1], '\0');
                                 return WBE::MEM_NULL;
                             }))
                         .set_reserve_res_unit_storage_callback(
                             [&captured_ru_text, &stack_allocator](std::string_view, size_t p_size) -> WBE::RefRes<char> {
                                 EXPECT_EQ(p_size, sizeof(WBE::RUText));
                                 WBE::MemID mem_id = stack_allocator.allocate(p_size);
                                 captured_ru_text = WBE::RefRes<char>(mem_id, &stack_allocator).force_cast<WBE::RUText>();
                                 return captured_ru_text.force_cast<char>();
                             })
                         .build(ss);
    loader.populate_scheme(*res_chunk, text_scheme);
    ASSERT_NE(captured_ru_text, nullptr);
    EXPECT_EQ(captured_ru_text->size, test_file_content.size() + 1);
    EXPECT_EQ(text_content_storage, test_file_content);
}

TEST_F(ResourcePopulatorTest, BinaryReousrceGeneral) {
    WBE::ResourceUnitLoaderBinary loader;
    std::stringstream ss;
    WBE::StackAllocator stack_allocator(1024);
    std::vector<uint8_t> binary_data_storage;
    WBE::RefRes<WBE::RUBinary> captured_ru_binary = WBE::MEM_NULL;
    WBE::RUSchemaBinary binary_scheme;
    binary_scheme.path = "assets/images/default_albedo.png";
    auto res_chunk = MockResourceChunk::Builder()
                         .set_push_data_callback([&binary_data_storage](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
                             binary_data_storage.resize(p_res_size);
                             std::memcpy(binary_data_storage.data(), p_res, p_res_size);
                             return WBE::MEM_NULL;
                         })
                         .set_reserve_res_unit_storage_callback(
                             [&captured_ru_binary, &stack_allocator](std::string_view, size_t p_size) -> WBE::RefRes<char> {
                                 EXPECT_EQ(p_size, sizeof(WBE::RUBinary));
                                 WBE::MemID mem_id = stack_allocator.allocate(p_size);
                                 captured_ru_binary = WBE::RefRes<char>(mem_id, &stack_allocator).force_cast<WBE::RUBinary>();
                                 return captured_ru_binary.force_cast<char>();
                             })
                         .build(ss);

    loader.populate_scheme(*res_chunk, binary_scheme);

    auto expected_binary = WBE::load_binary_file(static_cast<std::string>(WBE::get_resource_path(binary_scheme.path)).c_str());
    ASSERT_NE(captured_ru_binary, nullptr);
    EXPECT_EQ(captured_ru_binary->size, expected_binary.size());
    EXPECT_EQ(binary_data_storage.size(), expected_binary.size());
    EXPECT_EQ(binary_data_storage, expected_binary);
}

TEST_F(ResourcePopulatorTest, ImageResourceGeneral) {
    WBE::ResourceUnitLoaderImage loader;
    std::stringstream ss;
    WBE::StackAllocator stack_allocator(1024);
    std::vector<char> image_data_storage;
    WBE::RefRes<WBE::RUImage> captured_ru_image = WBE::MEM_NULL;
    WBE::RUSchemaImage image_scheme;
    image_scheme.path = "assets/images/test_ppm3.ppm";
    image_scheme.color_space = "srgb";
    auto res_chunk = MockResourceChunk::Builder()
                         .set_push_data_callback(std::function<WBE::RefRes<char>(size_t, const void*)>(
                             [&image_data_storage](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
                                 image_data_storage.resize(p_res_size);
                                 std::memcpy(image_data_storage.data(), p_res, p_res_size);
                                 return WBE::MEM_NULL;
                             }))
                         .set_reserve_res_unit_storage_callback(std::function<WBE::RefRes<char>(std::string_view, size_t)>(
                             [&captured_ru_image, &stack_allocator](std::string_view, size_t p_size) -> WBE::RefRes<char> {
                                 EXPECT_EQ(p_size, sizeof(WBE::RUImage));
                                 WBE::MemID mem_id = stack_allocator.allocate(p_size);
                                 captured_ru_image = WBE::RefRes<char>(mem_id, &stack_allocator).force_cast<WBE::RUImage>();
                                 return captured_ru_image.force_cast<char>();
                             }))
                         .build(ss);
    loader.populate_scheme(*res_chunk, image_scheme);
    ASSERT_NE(captured_ru_image, nullptr);
    EXPECT_EQ(captured_ru_image->width, 16);
    EXPECT_EQ(captured_ru_image->height, 16);
    EXPECT_EQ(captured_ru_image->channel_count, 3);
    EXPECT_EQ(captured_ru_image->color_space, WBE::get_color_space_from_str("srgb"));
    EXPECT_EQ(image_data_storage.size(), 16 * 16 * 3);
    // Check first pixel: should be red (255, 0, 0)
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(0)), 255);
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(1)), 0);
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(2)), 0);
    // Check second pixel: should be red (255, 0, 0)
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(3)), 255);
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(4)), 0);
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(5)), 0);
}

TEST_F(ResourcePopulatorTest, ImageResourceDefaultColorSpace) {
    WBE::ResourceUnitLoaderImage loader;
    std::stringstream ss;
    WBE::StackAllocator stack_allocator(1024);
    std::vector<char> image_data_storage;
    WBE::RefRes<WBE::RUImage> captured_ru_image = WBE::MEM_NULL;
    WBE::RUSchemaImage image_scheme;
    image_scheme.path = "assets/images/test_ppm3.ppm";
    auto res_chunk = MockResourceChunk::Builder()
                         .set_push_data_callback(std::function<WBE::RefRes<char>(size_t, const void*)>(
                             [&image_data_storage](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
                                 image_data_storage.resize(p_res_size);
                                 std::memcpy(image_data_storage.data(), p_res, p_res_size);
                                 return WBE::MEM_NULL;
                             }))
                         .set_reserve_res_unit_storage_callback(std::function<WBE::RefRes<char>(std::string_view, size_t)>(
                             [&captured_ru_image, &stack_allocator](std::string_view, size_t p_size) -> WBE::RefRes<char> {
                                 EXPECT_EQ(p_size, sizeof(WBE::RUImage));
                                 WBE::MemID mem_id = stack_allocator.allocate(p_size);
                                 captured_ru_image = WBE::RefRes<char>(mem_id, &stack_allocator).force_cast<WBE::RUImage>();
                                 return captured_ru_image.force_cast<char>();
                             }))
                         .build(ss);
    loader.populate_scheme(*res_chunk, image_scheme);
    ASSERT_NE(captured_ru_image, nullptr);
    EXPECT_EQ(captured_ru_image->color_space, WBE::get_color_space_from_str("srgb"));
}

TEST_F(ResourcePopulatorTest, AudioResourceGeneral) {
    // TODO: loader is not implemented yet.
}

TEST_F(ResourcePopulatorTest, FontResourceGeneral) {
    // TODO: loader is not implemented yet.
}

TEST_F(ResourcePopulatorTest, MaterialResourceGeneral) {
    WBE::ResourceUnitLoaderMaterial loader;
    WBE::StackAllocator stack_allocator(64 * 1024);
    std::stringstream stream;
    WBE::RefRes<WBE::RUMaterial> captured_material;
    std::vector<std::string> created_ref_keys;

    auto push_to_stack = [&stack_allocator](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
        WBE::MemID mem_id = stack_allocator.allocate(p_res_size);
        std::memcpy(stack_allocator.get(mem_id), p_res, p_res_size);
        return WBE::RefRes<char>(mem_id, &stack_allocator);
    };

    WBE::RUGraphicsPipeline graphics_pipeline;
    auto graphics_pipeline_ref = push_to_stack(sizeof(graphics_pipeline), &graphics_pipeline).force_cast<WBE::RUGraphicsPipeline>();
    WBE::RUImage referenced_texture;
    referenced_texture.width = 4;
    referenced_texture.height = 4;
    referenced_texture.channel_count = 3;
    auto referenced_texture_ref = push_to_stack(sizeof(referenced_texture), &referenced_texture).force_cast<WBE::RUImage>();
    std::unordered_map<std::string, WBE::RefRes<char>> reference_resources{
        {"main_pipeline", graphics_pipeline_ref.force_cast<char>()},
        {"shadow_pipeline", graphics_pipeline_ref.force_cast<char>()},
        {"normal_texture", referenced_texture_ref.force_cast<char>()},
    };

    auto res_chunk =
        MockResourceChunk::Builder()
            .set_push_data_callback(std::function<WBE::RefRes<char>(size_t, const void*)>(push_to_stack))
            .set_reserve_res_unit_storage_callback(std::function<WBE::RefRes<char>(std::string_view, size_t)>(
                [&captured_material, &stack_allocator](std::string_view p_res_key, size_t p_size) -> WBE::RefRes<char> {
                    WBE::MemID mem_id = stack_allocator.allocate(p_size);
                    WBE::RefRes<char> saved_resource(mem_id, &stack_allocator);
                    if (p_res_key == "test_material") {
                        captured_material = saved_resource.force_cast<WBE::RUMaterial>();
                    }
                    return saved_resource;
                }))
            .set_create_ref_to_callback([&created_ref_keys, &reference_resources](
                                            std::function<void(WBE::RefRes<char>)> p_assign_ref, std::string_view p_key) {
                created_ref_keys.emplace_back(p_key);
                auto found_resource = reference_resources.find(created_ref_keys.back());
                ASSERT_NE(found_resource, reference_resources.end());
                p_assign_ref(found_resource->second);
            })
            .build(stream);

    std::string json_buffer = R"(
    {
        "id": "test_material",
        "type": "material",
        "graphics_pipeline_ids": ["main_pipeline", "shadow_pipeline"],
        "textures": [
            {
                "texture_role": "normal",
                "texture_id": "normal_texture"
            },
            {
                "texture_role": "albedo",
                "texture": {
                    "id": "albedo_texture",
                    "type": "image",
                    "path": "assets/images/test_ppm3.ppm",
                    "color_space": "srgb",
                    "channel_count": 3
                }
            }
        ]
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);

    loader.populate_scheme_from_data(*res_chunk, parser.get_data());

    ASSERT_NE(captured_material, nullptr);
    ASSERT_NE(captured_material->resource_key, nullptr);
    EXPECT_STREQ(captured_material->resource_key, "test_material");
    EXPECT_EQ(captured_material->graphics_pipeline.get(), graphics_pipeline_ref.get());
    ASSERT_EQ(captured_material->texture_count, 2U);
    ASSERT_NE(captured_material->textures, nullptr);
    EXPECT_STREQ(captured_material->textures.get()[0].role.name.get(), "normal");
    EXPECT_EQ(captured_material->textures.get()[0].role.hash, WBE::dynam_hash("normal"));
    EXPECT_EQ(captured_material->textures.get()[0].texture.get(), referenced_texture_ref.get());
    EXPECT_STREQ(captured_material->textures.get()[1].role.name.get(), "albedo");
    EXPECT_EQ(captured_material->textures.get()[1].role.hash, WBE::dynam_hash("albedo"));
    ASSERT_NE(captured_material->textures.get()[1].texture, nullptr);
    EXPECT_EQ(captured_material->textures.get()[1].texture->width, 16U);
    EXPECT_EQ(captured_material->textures.get()[1].texture->height, 16U);
    EXPECT_EQ(captured_material->textures.get()[1].texture->channel_count, 3U);
    ASSERT_EQ(created_ref_keys.size(), 2U);
    EXPECT_EQ(created_ref_keys[0], "main_pipeline");
    EXPECT_EQ(created_ref_keys[1], "normal_texture");
}

TEST_F(ResourcePopulatorTest, MaterialResourceInlineGraphicsPipeline) {
    WBE::ResourceUnitLoaderMaterial loader;
    WBE::StackAllocator stack_allocator(64 * 1024);
    std::stringstream stream;
    WBE::RefRes<WBE::RUMaterial> captured_material;
    WBE::RefRes<WBE::RUGraphicsPipeline> captured_pipeline;
    std::vector<std::string> created_ref_keys;

    auto push_to_stack = [&stack_allocator](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
        WBE::MemID mem_id = stack_allocator.allocate(p_res_size);
        std::memcpy(stack_allocator.get(mem_id), p_res, p_res_size);
        return WBE::RefRes<char>(mem_id, &stack_allocator);
    };

    WBE::RUShader vertex_shader;
    auto vertex_shader_ref = push_to_stack(sizeof(vertex_shader), &vertex_shader).force_cast<WBE::RUShader>();
    WBE::RUShader fragment_shader;
    auto fragment_shader_ref = push_to_stack(sizeof(fragment_shader), &fragment_shader).force_cast<WBE::RUShader>();
    std::unordered_map<std::string, WBE::RefRes<char>> reference_resources{
        {"vertex_shader", vertex_shader_ref.force_cast<char>()},
        {"fragment_shader", fragment_shader_ref.force_cast<char>()},
    };

    auto res_chunk =
        MockResourceChunk::Builder()
            .set_push_data_callback(std::function<WBE::RefRes<char>(size_t, const void*)>(push_to_stack))
            .set_reserve_res_unit_storage_callback(std::function<WBE::RefRes<char>(std::string_view, size_t)>(
                [&captured_material, &captured_pipeline, &stack_allocator](std::string_view p_res_key, size_t p_size) -> WBE::RefRes<char> {
                    WBE::MemID mem_id = stack_allocator.allocate(p_size);
                    WBE::RefRes<char> saved_resource(mem_id, &stack_allocator);
                    if (p_res_key == "test_material_inline_pipeline") {
                        captured_material = saved_resource.force_cast<WBE::RUMaterial>();
                    }
                    if (p_res_key == "inline_pipeline") {
                        captured_pipeline = saved_resource.force_cast<WBE::RUGraphicsPipeline>();
                    }
                    return saved_resource;
                }))
            .set_create_ref_to_callback([&created_ref_keys, &reference_resources](
                                            std::function<void(WBE::RefRes<char>)> p_assign_ref, std::string_view p_key) {
                created_ref_keys.emplace_back(p_key);
                auto found_resource = reference_resources.find(created_ref_keys.back());
                ASSERT_NE(found_resource, reference_resources.end());
                p_assign_ref(found_resource->second);
            })
            .build(stream);

    std::string json_buffer = R"(
    {
        "id": "test_material_inline_pipeline",
        "type": "material",
        "graphics_pipeline": {
            "id": "inline_pipeline",
            "type": "graphics_pipeline",
            "vertex_stage_id": "vertex_shader",
            "vertex_stage_entry": "vertex_main",
            "fragment_stage_id": "fragment_shader",
            "fragment_stage_entry": "fragment_main",
            "depth_test": false,
            "depth_write": false,
            "cull_backfaces": false
        }
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);

    loader.populate_scheme_from_data(*res_chunk, parser.get_data());

    ASSERT_NE(captured_material, nullptr);
    ASSERT_NE(captured_pipeline, nullptr);
    ASSERT_NE(captured_material->resource_key, nullptr);
    ASSERT_NE(captured_pipeline->resource_key, nullptr);
    EXPECT_STREQ(captured_material->resource_key, "test_material_inline_pipeline");
    EXPECT_STREQ(captured_pipeline->resource_key, "inline_pipeline");
    EXPECT_EQ(captured_material->graphics_pipeline.get(), captured_pipeline.get());
    EXPECT_EQ(captured_material->textures, nullptr);
    EXPECT_EQ(captured_material->texture_count, 0U);
    EXPECT_EQ(captured_pipeline->vertex_stage.get(), vertex_shader_ref.get());
    EXPECT_EQ(captured_pipeline->fragment_stage.get(), fragment_shader_ref.get());
    EXPECT_STREQ(captured_pipeline->vertex_stage_entry.get(), "vertex_main");
    EXPECT_STREQ(captured_pipeline->fragment_stage_entry.get(), "fragment_main");
    EXPECT_FALSE(captured_pipeline->depth_test);
    EXPECT_FALSE(captured_pipeline->depth_write);
    EXPECT_FALSE(captured_pipeline->cull_backfaces);
    ASSERT_EQ(created_ref_keys.size(), 2U);
    EXPECT_EQ(created_ref_keys[0], "vertex_shader");
    EXPECT_EQ(created_ref_keys[1], "fragment_shader");
}

TEST_F(ResourcePopulatorTest, MaterialTextureSlotValidation) {
    WBE::ResourceUnitLoaderMaterial loader;
    WBE::StackAllocator stack_allocator(64 * 1024);
    std::stringstream stream;

    auto push_to_stack = [&stack_allocator](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
        WBE::MemID mem_id = stack_allocator.allocate(p_res_size);
        std::memcpy(stack_allocator.get(mem_id), p_res, p_res_size);
        return WBE::RefRes<char>(mem_id, &stack_allocator);
    };

    auto res_chunk = MockResourceChunk::Builder()
                         .set_push_data_callback(std::function<WBE::RefRes<char>(size_t, const void*)>(push_to_stack))
                         .set_reserve_res_unit_storage_callback(std::function<WBE::RefRes<char>(std::string_view, size_t)>(
                             [&stack_allocator](std::string_view, size_t p_size) -> WBE::RefRes<char> {
                                 WBE::MemID mem_id = stack_allocator.allocate(p_size);
                                 return WBE::RefRes<char>(mem_id, &stack_allocator);
                             }))
                         .build(stream);

    std::string json_buffer = R"(
    {
        "id": "test_material_missing_texture_binding",
        "type": "material",
        "graphics_pipeline_ids": ["main_pipeline"],
        "textures": [{"texture_role": "albedo"}]
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);

    EXPECT_THROW(loader.populate_scheme_from_data(*res_chunk, parser.get_data()), std::runtime_error);

    std::string arbitrary_texture_role_json = R"(
    {
        "id": "test_arbitrary_texture_role",
        "type": "material",
        "graphics_pipeline_ids": ["main_pipeline"],
        "textures": [{"texture_role": "height", "texture_id": "height_texture"}]
    }
    )";
    WBE::ParserJSON arbitrary_texture_role_parser;
    arbitrary_texture_role_parser.parse_from_buffer(arbitrary_texture_role_json);
    EXPECT_NO_THROW(loader.populate_scheme_from_data(*res_chunk, arbitrary_texture_role_parser.get_data()));

    std::string empty_texture_role_json = R"(
    {
        "id": "test_empty_texture_role",
        "type": "material",
        "graphics_pipeline_ids": ["main_pipeline"],
        "textures": [{"texture_role": "", "texture_id": "empty_texture"}]
    }
    )";
    WBE::ParserJSON empty_texture_role_parser;
    empty_texture_role_parser.parse_from_buffer(empty_texture_role_json);
    EXPECT_THROW(loader.populate_scheme_from_data(*res_chunk, empty_texture_role_parser.get_data()), std::runtime_error);

    std::string duplicate_texture_role_json = R"(
    {
        "id": "test_duplicate_texture_role",
        "type": "material",
        "graphics_pipeline_ids": ["main_pipeline"],
        "textures": [
            {"texture_role": "albedo", "texture_id": "first_albedo"},
            {"texture_role": "albedo", "texture_id": "second_albedo"}
        ]
    }
    )";
    WBE::ParserJSON duplicate_texture_role_parser;
    duplicate_texture_role_parser.parse_from_buffer(duplicate_texture_role_json);
    EXPECT_THROW(loader.populate_scheme_from_data(*res_chunk, duplicate_texture_role_parser.get_data()), std::runtime_error);

    std::string colliding_texture_role_json = R"(
    {
        "id": "test_colliding_texture_role",
        "type": "material",
        "graphics_pipeline_ids": ["main_pipeline"],
        "textures": [
            {"texture_role": "role_002876ae4c05a223", "texture_id": "first_texture"},
            {"texture_role": "role_2ffa43e2b7d24f56", "texture_id": "second_texture"}
        ]
    }
    )";
    static_assert(WBE::dynam_hash("role_002876ae4c05a223") == WBE::dynam_hash("role_2ffa43e2b7d24f56"));
    WBE::ParserJSON colliding_texture_role_parser;
    colliding_texture_role_parser.parse_from_buffer(colliding_texture_role_json);
    EXPECT_THROW(loader.populate_scheme_from_data(*res_chunk, colliding_texture_role_parser.get_data()), std::runtime_error);
}

TEST_F(ResourcePopulatorTest, VideoResourceGeneral) {
    // TODO: loader is not implemented yet.
}

#endif
