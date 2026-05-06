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
#ifndef WBE_FILE_2D_SURFACE_UTILS_TEST_HH
#define WBE_FILE_2D_SURFACE_UTILS_TEST_HH

#include "core/surface/2d_primitive.hh"
#include "core/surface/2d_surface_utils.hh"
#include "glm/ext/vector_float2.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

namespace WBE = WhiteBirdEngine;

// Simple vertex structure for testing
struct TestVertex {
    glm::vec2 position;
};

// Simple triangle structure for testing
struct TestTriangle {
    TestVertex* vert1;
    TestVertex* vert2;
    TestVertex* vert3;
};

TEST(SurfaceTest2D, TriangulationSimpleTriangle) {
    // Test with a simple triangle (should not change)
    TestVertex vertices[3] = {{{0.0F, 0.0F}}, {{1.0F, 0.0F}}, {{0.5F, 1.0F}}};
    TestTriangle triangles[1] = {};

    EXPECT_NO_THROW(WBE::SurfaceUtils2D::triangulate(vertices, 3, triangles));
}

TEST(SurfaceTest2D, TriangulationSquare) {
    // Test with a square (4 vertices) - should produce 2 triangles
    TestVertex vertices[4] = {
        {{0.0F, 0.0F}}, // Bottom-left
        {{1.0F, 0.0F}}, // Bottom-right
        {{1.0F, 1.0F}}, // Top-right
        {{0.0F, 1.0F}}  // Top-left
    };
    TestTriangle triangles[2] = {}; // n-2 triangles for n vertices

    EXPECT_NO_THROW(WBE::SurfaceUtils2D::triangulate(vertices, 4, triangles));

    // Verify that we got valid triangles
    EXPECT_NE(triangles[0].vert1, nullptr);
    EXPECT_NE(triangles[0].vert2, nullptr);
    EXPECT_NE(triangles[0].vert3, nullptr);
    EXPECT_NE(triangles[1].vert1, nullptr);
    EXPECT_NE(triangles[1].vert2, nullptr);
    EXPECT_NE(triangles[1].vert3, nullptr);
}

TEST(SurfaceTest2D, TriangulationPentagon) {
    // Test with a pentagon (5 vertices) - should produce 3 triangles
    TestVertex vertices[5] = {
        {{0.0F, 0.0F}},     // Center bottom
        {{0.951F, 0.309F}}, // Right bottom
        {{0.588F, 1.0F}},   // Right top
        {{-0.588F, 1.0F}},  // Left top
        {{-0.951F, 0.309F}} // Left bottom
    };
    TestTriangle triangles[3] = {}; // n-2 triangles for n vertices

    EXPECT_NO_THROW(WBE::SurfaceUtils2D::triangulate(vertices, 5, triangles));

    // Verify that we got valid triangles
    for (auto& triangle : triangles) {
        EXPECT_NE(triangle.vert1, nullptr);
        EXPECT_NE(triangle.vert2, nullptr);
        EXPECT_NE(triangle.vert3, nullptr);
    }
}

TEST(SurfaceTest2D, TriangulationNullVertexList) {
    // Test null vertex list - should throw exception
    TestTriangle triangles[1] = {};

    EXPECT_THROW((WBE::SurfaceUtils2D::triangulate<TestVertex, TestTriangle>(nullptr, 3, triangles)), std::runtime_error);
}

TEST(SurfaceTest2D, TriangulationNullTriangleList) {
    // Test null triangle list - should throw exception
    TestVertex vertices[3] = {{{0.0F, 0.0F}}, {{1.0F, 0.0F}}, {{0.5F, 1.0F}}};

    EXPECT_THROW((WBE::SurfaceUtils2D::triangulate<TestVertex, TestTriangle>(vertices, 3, nullptr)), std::runtime_error);
}

TEST(SurfaceTest2D, TriangulationCollinearVertices) {
    // Test with collinear vertices (should be handled by removing degenerate vertices)
    TestVertex vertices[4] = {{{0.0F, 0.0F}},
        {{0.5F, 0.0F}}, // Collinear with first and third
        {{1.0F, 0.0F}},
        {{0.5F, 1.0F}}};
    TestTriangle triangles[2] = {};

    EXPECT_NO_THROW(WBE::SurfaceUtils2D::triangulate(vertices, 4, triangles));
}

TEST(SurfaceTest2D, TriangulationComplexPolygon) {
    // Test with a more complex polygon (hexagon)
    TestVertex vertices[6] = {
        {{1.0F, 0.0F}},     // Right
        {{0.5F, 0.866F}},   // Top-right
        {{-0.5F, 0.866F}},  // Top-left
        {{-1.0F, 0.0F}},    // Left
        {{-0.5F, -0.866F}}, // Bottom-left
        {{0.5F, -0.866F}}   // Bottom-right
    };
    TestTriangle triangles[4] = {}; // n-2 triangles for n vertices

    EXPECT_NO_THROW(WBE::SurfaceUtils2D::triangulate(vertices, 6, triangles));

    // Verify that we got valid triangles
    for (auto& triangle : triangles) {
        EXPECT_NE(triangle.vert1, nullptr);
        EXPECT_NE(triangle.vert2, nullptr);
        EXPECT_NE(triangle.vert3, nullptr);
    }
}

TEST(SurfaceTest2D, TriangulationWithWBEVertex2D) {
    // Test using actual WBE::Vertex2D structure
    WBE::Vertex2D vertices[4] = {
        {.position = {glm::vec2{0.0F, 0.0F}}, .depth = 0.0F}, // Bottom-left
        {.position = {glm::vec2{1.0F, 0.0F}}, .depth = 0.0F}, // Bottom-right
        {.position = {glm::vec2{1.0F, 1.0F}}, .depth = 0.0F}, // Top-right
        {.position = {glm::vec2{0.0F, 1.0F}}, .depth = 0.0F}  // Top-left
    };

    // Triangle structure that uses WBE::Vertex2D pointers
    struct WBETriangle {
        WBE::Vertex2D* vert1;
        WBE::Vertex2D* vert2;
        WBE::Vertex2D* vert3;
    };

    WBETriangle triangles[2] = {};

    EXPECT_NO_THROW(WBE::SurfaceUtils2D::triangulate(vertices, 4, triangles));

    // Verify that we got valid triangles
    EXPECT_NE(triangles[0].vert1, nullptr);
    EXPECT_NE(triangles[0].vert2, nullptr);
    EXPECT_NE(triangles[0].vert3, nullptr);
}

#endif
