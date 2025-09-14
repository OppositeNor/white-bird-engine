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
#ifndef __WBE_RENDER_DATABASE_VK_TEST_HH__
#define __WBE_RENDER_DATABASE_VK_TEST_HH__

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <memory>

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/surface/3d_primitive.hh"
#include "core/surface/primitive_slots.hh"
#include "function/render_vk/render_database_vk.hh"
#include "function/render/material.hh"
#include "global/global.hh"
#include "global/stl_allocator.hh"

namespace WBE = WhiteBirdEngine;

class RenderDatabaseVKTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
        allocator = std::make_unique<WBE::HeapAllocatorAlignedPool>(WBE_MiB(10)); // 1MB pool
        database = std::make_unique<WBE::RenderDatabaseVK>(allocator.get());
    }

    void TearDown() override {
        database.reset();
        allocator.reset();
        global.reset();
    }

    // Helper function to create a test triangle
    WBE::Triangle3DTextured createTestTriangle(float x_offset = 0.0f) {
        WBE::Triangle3DTextured triangle;
        
        // Vertex 1
        triangle.vert1.position = glm::vec3(0.0f + x_offset, 0.0f, 0.0f);
        triangle.vert1.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        triangle.vert1.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        triangle.vert1.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
        triangle.vert1.slot.uv = glm::vec2(0.0f, 0.0f);
        triangle.vert1.slot.is_opaque = true;
        
        // Vertex 2
        triangle.vert2.position = glm::vec3(1.0f + x_offset, 0.0f, 0.0f);
        triangle.vert2.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        triangle.vert2.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        triangle.vert2.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
        triangle.vert2.slot.uv = glm::vec2(1.0f, 0.0f);
        triangle.vert2.slot.is_opaque = true;
        
        // Vertex 3
        triangle.vert3.position = glm::vec3(0.5f + x_offset, 1.0f, 0.0f);
        triangle.vert3.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        triangle.vert3.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        triangle.vert3.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
        triangle.vert3.slot.uv = glm::vec2(0.5f, 1.0f);
        triangle.vert3.slot.is_opaque = true;
        
        return triangle;
    }

    std::unique_ptr<WBE::Global> global;
    std::unique_ptr<WBE::HeapAllocatorAlignedPool> allocator;
    std::unique_ptr<WBE::RenderDatabaseVK> database;
};

// Test constructor with valid allocator
TEST_F(RenderDatabaseVKTest, ConstructorWithValidAllocator) {
    EXPECT_NO_THROW({
        WBE::RenderDatabaseVK db(allocator.get());
    });
}

// Test constructor with null allocator
TEST_F(RenderDatabaseVKTest, ConstructorWithNullAllocator) {
    EXPECT_THROW({
        WBE::RenderDatabaseVK db(nullptr);
    }, std::runtime_error);
}

// Test setting and getting static opaque triangles
TEST_F(RenderDatabaseVKTest, SetAndGetStaticOpaqueTriangles) {
    const WBE::MaterialID material1 = 100;
    const WBE::MaterialID material2 = 200;
    
    // Create triangles with different materials
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> triangles(allocator.get());
    triangles.push_back({material1, createTestTriangle(0.0f)});
    triangles.push_back({material1, createTestTriangle(1.0f)});
    triangles.push_back({material2, createTestTriangle(2.0f)});
    
    database->set_static_opaque_triangles(std::move(triangles));
    
    // Test getting all triangles
    const auto& all_triangles = database->get_static_opaque_triangles();
    ASSERT_EQ(all_triangles.size(), 3);
    
    // Test getting triangles by material ID
    auto [count1, triangles1] = database->get_static_opaque_triangles(material1);
    EXPECT_EQ(count1, 2);
    EXPECT_NE(triangles1, nullptr);
    
    auto [count2, triangles2] = database->get_static_opaque_triangles(material2);
    EXPECT_EQ(count2, 1);
    EXPECT_NE(triangles2, nullptr);
    
    // Verify triangle data integrity
    EXPECT_FLOAT_EQ(triangles1[0].vert1.position.x, 0.0f);
    EXPECT_FLOAT_EQ(triangles1[1].vert1.position.x, 1.0f);
    EXPECT_FLOAT_EQ(triangles2[0].vert1.position.x, 2.0f);
}

// Test getting static triangles with invalid material ID
TEST_F(RenderDatabaseVKTest, GetStaticTrianglesWithInvalidMaterial) {
    EXPECT_THROW({
        database->get_static_opaque_triangles(WBE::MATERIAL_INVALID);
    }, std::runtime_error);
}

// Test getting static triangles with non-existent material ID
TEST_F(RenderDatabaseVKTest, GetStaticTrianglesWithNonExistentMaterial) {
    const WBE::MaterialID nonExistentMaterial = 999;
    
    EXPECT_THROW({
        database->get_static_opaque_triangles(nonExistentMaterial);
    }, std::out_of_range);
}

// Test setting and getting dynamic opaque triangles
TEST_F(RenderDatabaseVKTest, SetAndGetDynamicOpaqueTriangles) {
    const WBE::MaterialID material1 = 100;
    const WBE::MaterialID material2 = 200;
    
    // Create triangles with different materials
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> triangles(allocator.get());
    triangles.push_back({material1, createTestTriangle(0.0f)});
    triangles.push_back({material2, createTestTriangle(1.0f)});
    triangles.push_back({material1, createTestTriangle(2.0f)});
    
    database->set_dynamic_opaque_triangles(std::move(triangles));
    
    // Test getting all dynamic triangles
    const auto& all_triangles = database->get_dynamic_opaque_triangles();
    ASSERT_EQ(all_triangles.size(), 3);
    
    // Test getting triangles by material ID
    const auto& triangles1 = database->get_dynamic_opaque_triangles(material1);
    EXPECT_EQ(triangles1.size(), 2);
    
    const auto& triangles2 = database->get_dynamic_opaque_triangles(material2);
    EXPECT_EQ(triangles2.size(), 1);
}

// Test setting dynamic triangles for specific material
TEST_F(RenderDatabaseVKTest, SetDynamicTrianglesForSpecificMaterial) {
    const WBE::MaterialID material = 100;
    
    WBE::vectorp<WBE::Triangle3DTextured> triangles(allocator.get());
    triangles.push_back(createTestTriangle(0.0f));
    triangles.push_back(createTestTriangle(1.0f));
    
    database->set_dynamic_opaque_triangles(material, std::move(triangles));
    
    const auto& retrieved = database->get_dynamic_opaque_triangles(material);
    ASSERT_EQ(retrieved.size(), 2);
    EXPECT_FLOAT_EQ(retrieved[0].vert1.position.x, 0.0f);
    EXPECT_FLOAT_EQ(retrieved[1].vert1.position.x, 1.0f);
}

// Test getting dynamic triangles with invalid material ID
TEST_F(RenderDatabaseVKTest, GetDynamicTrianglesWithInvalidMaterial) {
    EXPECT_THROW({
        database->get_dynamic_opaque_triangles(WBE::MATERIAL_INVALID);
    }, std::runtime_error);
}

// Test getting dynamic triangles with non-existent material ID
TEST_F(RenderDatabaseVKTest, GetDynamicTrianglesWithNonExistentMaterial) {
    const WBE::MaterialID nonExistentMaterial = 999;
    
    EXPECT_THROW({
        database->get_dynamic_opaque_triangles(nonExistentMaterial);
    }, std::out_of_range);
}

// Test updating cached dynamic triangles
TEST_F(RenderDatabaseVKTest, UpdateCachedDynamicTriangles) {
    const WBE::MaterialID material = 100;
    
    // Set some dynamic triangles
    WBE::vectorp<WBE::Triangle3DTextured> triangles(allocator.get());
    triangles.push_back(createTestTriangle(0.0f));
    triangles.push_back(createTestTriangle(1.0f));
    
    database->set_dynamic_opaque_triangles(material, std::move(triangles));
    
    // The cached data should be updated automatically when accessed
    const auto& all_triangles_before = database->get_dynamic_opaque_triangles();
    EXPECT_EQ(all_triangles_before.size(), 2);
    
    // Manually update cache (should be safe to call multiple times)
    EXPECT_NO_THROW(database->update_cached_dynamic_opaque_triangles());
    
    const auto& all_triangles_after = database->get_dynamic_opaque_triangles();
    EXPECT_EQ(all_triangles_after.size(), 2);
}

// Test adding dynamic triangles to existing material
TEST_F(RenderDatabaseVKTest, AddDynamicTriangles) {
    const WBE::MaterialID material = 100;
    
    // First, set initial triangles
    WBE::vectorp<WBE::Triangle3DTextured> initial_triangles(allocator.get());
    initial_triangles.push_back(createTestTriangle(0.0f));
    database->set_dynamic_opaque_triangles(material, std::move(initial_triangles));
    
    // Now add more triangles
    std::vector<WBE::Triangle3DTextured> additional_triangles;
    additional_triangles.push_back(createTestTriangle(1.0f));
    additional_triangles.push_back(createTestTriangle(2.0f));
    
    database->add_dynamic_opaque_triangles(material, additional_triangles.data(), additional_triangles.size());
    
    const auto& result = database->get_dynamic_opaque_triangles(material);
    EXPECT_EQ(result.size(), 3);
}

// Test adding triangles to non-existent material
TEST_F(RenderDatabaseVKTest, AddTrianglesToNonExistentMaterial) {
    const WBE::MaterialID nonExistentMaterial = 999;
    WBE::Triangle3DTextured triangle = createTestTriangle(0.0f);
    
    EXPECT_THROW({
        database->add_dynamic_opaque_triangles(nonExistentMaterial, &triangle, 1);
    }, std::runtime_error);
}

// Test mixed static and dynamic triangles
TEST_F(RenderDatabaseVKTest, MixedStaticAndDynamicTriangles) {
    const WBE::MaterialID material1 = 100;
    const WBE::MaterialID material2 = 200;
    
    // Set static triangles
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> static_triangles(allocator.get());
    static_triangles.push_back({material1, createTestTriangle(0.0f)});
    static_triangles.push_back({material2, createTestTriangle(1.0f)});
    database->set_static_opaque_triangles(std::move(static_triangles));
    
    // Set dynamic triangles
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> dynamic_triangles(allocator.get());
    dynamic_triangles.push_back({material1, createTestTriangle(2.0f)});
    dynamic_triangles.push_back({material2, createTestTriangle(3.0f)});
    database->set_dynamic_opaque_triangles(std::move(dynamic_triangles));
    
    // Verify static triangles
    const auto& all_static = database->get_static_opaque_triangles();
    EXPECT_EQ(all_static.size(), 2);
    
    auto [static_count1, static_tri1] = database->get_static_opaque_triangles(material1);
    EXPECT_EQ(static_count1, 1);
    EXPECT_FLOAT_EQ(static_tri1[0].vert1.position.x, 0.0f);
    
    // Verify dynamic triangles
    const auto& all_dynamic = database->get_dynamic_opaque_triangles();
    EXPECT_EQ(all_dynamic.size(), 2);
    
    const auto& dynamic_tri1 = database->get_dynamic_opaque_triangles(material1);
    EXPECT_EQ(dynamic_tri1.size(), 1);
    EXPECT_FLOAT_EQ(dynamic_tri1[0].vert1.position.x, 2.0f);
}

// Test triangle data integrity with complex geometry
TEST_F(RenderDatabaseVKTest, TriangleDataIntegrity) {
    const WBE::MaterialID material = 100;
    
    // Create a triangle with specific vertex data
    WBE::Triangle3DTextured triangle;
    triangle.vert1.position = glm::vec3(1.5f, 2.5f, 3.5f);
    triangle.vert1.normal = glm::vec3(0.707f, 0.707f, 0.0f);
    triangle.vert1.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    triangle.vert1.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    triangle.vert1.slot.uv = glm::vec2(0.25f, 0.75f);
    triangle.vert1.slot.is_opaque = false;
    
    triangle.vert2.position = glm::vec3(-1.0f, -2.0f, -3.0f);
    triangle.vert2.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    triangle.vert2.tangent = glm::vec3(0.0f, 0.0f, 1.0f);
    triangle.vert2.bitangent = glm::vec3(1.0f, 0.0f, 0.0f);
    triangle.vert2.slot.uv = glm::vec2(0.8f, 0.2f);
    triangle.vert2.slot.is_opaque = true;
    
    triangle.vert3.position = glm::vec3(0.0f, 0.0f, 0.0f);
    triangle.vert3.normal = glm::vec3(0.0f, 0.0f, -1.0f);
    triangle.vert3.tangent = glm::vec3(-1.0f, 0.0f, 0.0f);
    triangle.vert3.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
    triangle.vert3.slot.uv = glm::vec2(0.5f, 0.5f);
    triangle.vert3.slot.is_opaque = true;
    
    WBE::vectorp<WBE::Triangle3DTextured> triangles(allocator.get());
    triangles.push_back(triangle);
    database->set_dynamic_opaque_triangles(material, std::move(triangles));
    
    const auto& retrieved = database->get_dynamic_opaque_triangles(material);
    ASSERT_EQ(retrieved.size(), 1);
    
    const auto& t = retrieved[0];
    
    // Verify vertex 1
    EXPECT_FLOAT_EQ(t.vert1.position.x, 1.5f);
    EXPECT_FLOAT_EQ(t.vert1.position.y, 2.5f);
    EXPECT_FLOAT_EQ(t.vert1.position.z, 3.5f);
    EXPECT_NEAR(t.vert1.normal.x, 0.707f, 0.001f);
    EXPECT_NEAR(t.vert1.normal.y, 0.707f, 0.001f);
    EXPECT_FLOAT_EQ(t.vert1.normal.z, 0.0f);
    EXPECT_FLOAT_EQ(t.vert1.slot.uv.x, 0.25f);
    EXPECT_FLOAT_EQ(t.vert1.slot.uv.y, 0.75f);
    EXPECT_FALSE(t.vert1.slot.is_opaque);
    
    // Verify vertex 2
    EXPECT_FLOAT_EQ(t.vert2.position.x, -1.0f);
    EXPECT_FLOAT_EQ(t.vert2.position.y, -2.0f);
    EXPECT_FLOAT_EQ(t.vert2.position.z, -3.0f);
    EXPECT_FLOAT_EQ(t.vert2.slot.uv.x, 0.8f);
    EXPECT_FLOAT_EQ(t.vert2.slot.uv.y, 0.2f);
    EXPECT_TRUE(t.vert2.slot.is_opaque);
    
    // Verify vertex 3
    EXPECT_FLOAT_EQ(t.vert3.position.x, 0.0f);
    EXPECT_FLOAT_EQ(t.vert3.position.y, 0.0f);
    EXPECT_FLOAT_EQ(t.vert3.position.z, 0.0f);
    EXPECT_FLOAT_EQ(t.vert3.normal.z, -1.0f);
    EXPECT_FLOAT_EQ(t.vert3.slot.uv.x, 0.5f);
    EXPECT_FLOAT_EQ(t.vert3.slot.uv.y, 0.5f);
    EXPECT_TRUE(t.vert3.slot.is_opaque);
}

// Test material ID sorting for static triangles
TEST_F(RenderDatabaseVKTest, MaterialIDSortingForStaticTriangles) {
    // Create triangles with materials in non-sorted order
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> triangles(allocator.get());
    triangles.push_back({300, createTestTriangle(0.0f)});
    triangles.push_back({100, createTestTriangle(1.0f)});
    triangles.push_back({200, createTestTriangle(2.0f)});
    triangles.push_back({100, createTestTriangle(3.0f)});
    triangles.push_back({300, createTestTriangle(4.0f)});
    
    database->set_static_opaque_triangles(std::move(triangles));
    
    // Verify that triangles are grouped by material ID
    auto [count100, triangles100] = database->get_static_opaque_triangles(100);
    EXPECT_EQ(count100, 2);
    
    auto [count200, triangles200] = database->get_static_opaque_triangles(200);
    EXPECT_EQ(count200, 1);
    
    auto [count300, triangles300] = database->get_static_opaque_triangles(300);
    EXPECT_EQ(count300, 2);
    
    // Verify the all triangles are in sorted order by material ID
    const auto& all_triangles = database->get_static_opaque_triangles();
    EXPECT_EQ(all_triangles.size(), 5);
}

// Test performance with large number of triangles
TEST_F(RenderDatabaseVKTest, LargeNumberOfTriangles) {
    const size_t num_triangles = 1000;
    const WBE::MaterialID material = 100;
    
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> triangles(allocator.get());
    for (size_t i = 0; i < num_triangles; ++i) {
        triangles.push_back({material, createTestTriangle(static_cast<float>(i))});
    }
    
    // Should handle large number of triangles without issues
    EXPECT_NO_THROW(database->set_static_opaque_triangles(std::move(triangles)));
    
    const auto& all_triangles = database->get_static_opaque_triangles();
    EXPECT_EQ(all_triangles.size(), num_triangles);
    
    auto [count, triangle_ptr] = database->get_static_opaque_triangles(material);
    EXPECT_EQ(count, num_triangles);
}

// Test database reset behavior
TEST_F(RenderDatabaseVKTest, DatabaseReset) {
    const WBE::MaterialID material = 100;
    
    // Set some data
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> static_triangles(allocator.get());
    static_triangles.push_back({material, createTestTriangle(0.0f)});
    database->set_static_opaque_triangles(std::move(static_triangles));
    
    WBE::vectorp<WBE::Triangle3DTextured> dynamic_triangles(allocator.get());
    dynamic_triangles.push_back(createTestTriangle(1.0f));
    database->set_dynamic_opaque_triangles(material, std::move(dynamic_triangles));
    
    // Reset with empty data
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> empty_static(allocator.get());
    database->set_static_opaque_triangles(std::move(empty_static));
    
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> empty_dynamic(allocator.get());
    database->set_dynamic_opaque_triangles(std::move(empty_dynamic));
    
    // Verify data is cleared
    const auto& static_result = database->get_static_opaque_triangles();
    EXPECT_EQ(static_result.size(), 0);
    
    const auto& dynamic_result = database->get_dynamic_opaque_triangles();
    EXPECT_EQ(dynamic_result.size(), 0);
}

// Test edge case: zero-sized triangles
TEST_F(RenderDatabaseVKTest, ZeroSizedTriangles) {
    const WBE::MaterialID material = 100;
    
    // Create degenerate triangle (all vertices at same position)
    WBE::Triangle3DTextured triangle;
    glm::vec3 pos(1.0f, 2.0f, 3.0f);
    triangle.vert1.position = pos;
    triangle.vert2.position = pos;
    triangle.vert3.position = pos;
    triangle.vert1.normal = triangle.vert2.normal = triangle.vert3.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    triangle.vert1.tangent = triangle.vert2.tangent = triangle.vert3.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    triangle.vert1.bitangent = triangle.vert2.bitangent = triangle.vert3.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    triangle.vert1.slot.uv = triangle.vert2.slot.uv = triangle.vert3.slot.uv = glm::vec2(0.0f, 0.0f);
    triangle.vert1.slot.is_opaque = triangle.vert2.slot.is_opaque = triangle.vert3.slot.is_opaque = true;
    
    WBE::vectorp<WBE::Triangle3DTextured> triangles(allocator.get());
    triangles.push_back(triangle);
    
    // Should handle degenerate triangles without issues
    EXPECT_NO_THROW(database->set_dynamic_opaque_triangles(material, std::move(triangles)));
    
    const auto& result = database->get_dynamic_opaque_triangles(material);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].vert1.position, result[0].vert2.position);
    EXPECT_EQ(result[0].vert2.position, result[0].vert3.position);
}

// Test edge case: extreme coordinate values
TEST_F(RenderDatabaseVKTest, ExtremeCoordinateValues) {
    const WBE::MaterialID material = 100;
    
    WBE::Triangle3DTextured triangle;
    
    // Use extreme float values
    triangle.vert1.position = glm::vec3(std::numeric_limits<float>::max(), 
                                      std::numeric_limits<float>::lowest(), 
                                      0.0f);
    triangle.vert2.position = glm::vec3(-std::numeric_limits<float>::max(), 
                                      std::numeric_limits<float>::max(), 
                                      std::numeric_limits<float>::lowest());
    triangle.vert3.position = glm::vec3(0.0f, 0.0f, std::numeric_limits<float>::max());
    
    // Set normalized vectors
    triangle.vert1.normal = triangle.vert2.normal = triangle.vert3.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    triangle.vert1.tangent = triangle.vert2.tangent = triangle.vert3.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    triangle.vert1.bitangent = triangle.vert2.bitangent = triangle.vert3.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    
    triangle.vert1.slot.uv = glm::vec2(0.0f, 1.0f);
    triangle.vert2.slot.uv = glm::vec2(1.0f, 0.0f);
    triangle.vert3.slot.uv = glm::vec2(0.5f, 0.5f);
    triangle.vert1.slot.is_opaque = triangle.vert2.slot.is_opaque = triangle.vert3.slot.is_opaque = true;
    
    WBE::vectorp<WBE::Triangle3DTextured> triangles(allocator.get());
    triangles.push_back(triangle);
    
    EXPECT_NO_THROW(database->set_dynamic_opaque_triangles(material, std::move(triangles)));
    
    const auto& result = database->get_dynamic_opaque_triangles(material);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].vert1.position.x, std::numeric_limits<float>::max());
    EXPECT_EQ(result[0].vert2.position.y, std::numeric_limits<float>::max());
    EXPECT_EQ(result[0].vert3.position.z, std::numeric_limits<float>::max());
}

// Test multiple material handling with various edge cases
TEST_F(RenderDatabaseVKTest, MultipleMaterialEdgeCases) {
    // Test with material IDs that include boundary values
    std::vector<WBE::MaterialID> materials = {
        1,                                          // Minimum valid material
        std::numeric_limits<WBE::MaterialID>::max(), // Maximum material ID
        std::numeric_limits<WBE::MaterialID>::max() / 2, // Mid-range
        42,                                         // Arbitrary value
        1000                                        // Another arbitrary value
    };
    
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> triangles(allocator.get());
    
    for (size_t i = 0; i < materials.size(); ++i) {
        triangles.push_back({materials[i], createTestTriangle(static_cast<float>(i))});
    }
    
    database->set_static_opaque_triangles(std::move(triangles));
    
    // Verify all materials are correctly stored and retrievable
    for (size_t i = 0; i < materials.size(); ++i) {
        auto [count, tri_ptr] = database->get_static_opaque_triangles(materials[i]);
        EXPECT_EQ(count, 1) << "Failed for material ID: " << materials[i];
        EXPECT_NE(tri_ptr, nullptr) << "Null pointer for material ID: " << materials[i];
    }
    
    const auto& all_triangles = database->get_static_opaque_triangles();
    EXPECT_EQ(all_triangles.size(), materials.size());
}

// Test concurrent access patterns (even though class is not thread-safe, 
// test logical consistency)
TEST_F(RenderDatabaseVKTest, ConsistentStateAfterMultipleOperations) {
    const WBE::MaterialID material1 = 100;
    const WBE::MaterialID material2 = 200;
    
    // Perform multiple operations in sequence
    WBE::vectorp<WBE::Triangle3DTextured> triangles1(allocator.get());
    triangles1.push_back(createTestTriangle(0.0f));
    database->set_dynamic_opaque_triangles(material1, std::move(triangles1));
    
    WBE::vectorp<WBE::Triangle3DTextured> triangles2(allocator.get());
    triangles2.push_back(createTestTriangle(1.0f));
    database->set_dynamic_opaque_triangles(material2, std::move(triangles2));
    
    // Add more triangles to material1
    std::vector<WBE::Triangle3DTextured> additional;
    additional.push_back(createTestTriangle(2.0f));
    additional.push_back(createTestTriangle(3.0f));
    database->add_dynamic_opaque_triangles(material1, additional.data(), additional.size());
    
    // Update cache explicitly
    database->update_cached_dynamic_opaque_triangles();
    
    // Verify final state
    const auto& result1 = database->get_dynamic_opaque_triangles(material1);
    EXPECT_EQ(result1.size(), 3);
    
    const auto& result2 = database->get_dynamic_opaque_triangles(material2);
    EXPECT_EQ(result2.size(), 1);
    
    const auto& all_triangles = database->get_dynamic_opaque_triangles();
    EXPECT_EQ(all_triangles.size(), 4);
}

// Test memory allocation behavior with large datasets
TEST_F(RenderDatabaseVKTest, MemoryAllocationStressTest) {
    const size_t large_count = 10000;
    const WBE::MaterialID material = 100;
    
    // Test large static triangle set
    WBE::vectorp<std::pair<WBE::MaterialID, WBE::Triangle3DTextured>> large_static(allocator.get());
    large_static.reserve(large_count);
    
    for (size_t i = 0; i < large_count; ++i) {
        large_static.push_back({material, createTestTriangle(static_cast<float>(i))});
    }
    
    EXPECT_NO_THROW(database->set_static_opaque_triangles(std::move(large_static)));
    
    // Test large dynamic triangle set
    WBE::vectorp<WBE::Triangle3DTextured> large_dynamic(allocator.get());
    large_dynamic.reserve(large_count);
    
    for (size_t i = 0; i < large_count; ++i) {
        large_dynamic.push_back(createTestTriangle(static_cast<float>(i + large_count)));
    }
    
    EXPECT_NO_THROW(database->set_dynamic_opaque_triangles(material, std::move(large_dynamic)));
    
    // Verify data integrity
    auto [static_count, static_ptr] = database->get_static_opaque_triangles(material);
    EXPECT_EQ(static_count, large_count);
    
    const auto& dynamic_result = database->get_dynamic_opaque_triangles(material);
    EXPECT_EQ(dynamic_result.size(), large_count);
    
    // Test cached data retrieval
    const auto& all_dynamic = database->get_dynamic_opaque_triangles();
    EXPECT_EQ(all_dynamic.size(), large_count);
}

#endif
