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
#ifndef __WBE_2D_SURFACE_UTILS_TEST_HH__
#define __WBE_2D_SURFACE_UTILS_TEST_HH__

#include "core/surface/2d_surface_utils.hh"
#include "core/surface/2d_primitive.hh"
#include <gtest/gtest.h>
#include <glm/glm.hpp>

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
    TestVertex vertices[3] = {
        {{0.0f, 0.0f}},
        {{1.0f, 0.0f}},
        {{0.5f, 1.0f}}
    };
    TestTriangle triangles[1];
    
    EXPECT_NO_THROW(WBE::SurfaceUtils2D::triangulate(vertices, 3, triangles));
}

TEST(SurfaceTest2D, TriangulationSquare) {
    // Test with a square (4 vertices) - should produce 2 triangles
    TestVertex vertices[4] = {
        {{0.0f, 0.0f}},  // Bottom-left
        {{1.0f, 0.0f}},  // Bottom-right
        {{1.0f, 1.0f}},  // Top-right
        {{0.0f, 1.0f}}   // Top-left
    };
    TestTriangle triangles[2]; // n-2 triangles for n vertices
    
    EXPECT_NO_THROW(
        WBE::SurfaceUtils2D::triangulate(vertices, 4, triangles)
    );
    
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
        {{0.0f, 0.0f}},      // Center bottom
        {{0.951f, 0.309f}},  // Right bottom
        {{0.588f, 1.0f}},    // Right top
        {{-0.588f, 1.0f}},   // Left top
        {{-0.951f, 0.309f}}  // Left bottom
    };
    TestTriangle triangles[3] = {}; // n-2 triangles for n vertices
    
    EXPECT_NO_THROW(
        WBE::SurfaceUtils2D::triangulate(vertices, 5, triangles)
    );
    
    // Verify that we got valid triangles
    for (int i = 0; i < 3; ++i) {
        EXPECT_NE(triangles[i].vert1, nullptr);
        EXPECT_NE(triangles[i].vert2, nullptr);
        EXPECT_NE(triangles[i].vert3, nullptr);
    }
}

TEST(SurfaceTest2D, TriangulationNullVertexList) {
    // Test null vertex list - should throw exception
    TestTriangle triangles[1] = {};
    
    EXPECT_THROW((WBE::SurfaceUtils2D::triangulate<TestVertex, TestTriangle>(nullptr, 3, triangles)), std::runtime_error);
}

TEST(SurfaceTest2D, TriangulationNullTriangleList) {
    // Test null triangle list - should throw exception
    TestVertex vertices[3] = {
        {{0.0f, 0.0f}},
        {{1.0f, 0.0f}},
        {{0.5f, 1.0f}}
    };
    
    EXPECT_THROW((
        WBE::SurfaceUtils2D::triangulate<TestVertex, TestTriangle>(vertices, 3, nullptr)
    ), std::runtime_error);
}

TEST(SurfaceTest2D, TriangulationCollinearVertices) {
    // Test with collinear vertices (should be handled by removing degenerate vertices)
    TestVertex vertices[4] = {
        {{0.0f, 0.0f}},
        {{0.5f, 0.0f}},  // Collinear with first and third
        {{1.0f, 0.0f}},
        {{0.5f, 1.0f}}
    };
    TestTriangle triangles[2] = {};
    
    EXPECT_NO_THROW(
        WBE::SurfaceUtils2D::triangulate(vertices, 4, triangles)
    );
}

TEST(SurfaceTest2D, TriangulationComplexPolygon) {
    // Test with a more complex polygon (hexagon)
    TestVertex vertices[6] = {
        {{1.0f, 0.0f}},      // Right
        {{0.5f, 0.866f}},    // Top-right
        {{-0.5f, 0.866f}},   // Top-left
        {{-1.0f, 0.0f}},     // Left
        {{-0.5f, -0.866f}},  // Bottom-left
        {{0.5f, -0.866f}}    // Bottom-right
    };
    TestTriangle triangles[4] = {}; // n-2 triangles for n vertices
    
    EXPECT_NO_THROW(
        WBE::SurfaceUtils2D::triangulate(vertices, 6, triangles)
    );
    
    // Verify that we got valid triangles
    for (int i = 0; i < 4; ++i) {
        EXPECT_NE(triangles[i].vert1, nullptr);
        EXPECT_NE(triangles[i].vert2, nullptr);
        EXPECT_NE(triangles[i].vert3, nullptr);
    }
}

TEST(SurfaceTest2D, TriangulationWithWBEVertex2D) {
    // Test using actual WBE::Vertex2D structure
    WBE::Vertex2D<> vertices[4] = {
        {{glm::vec2{0.0f, 0.0f}}, 0.0f, {}},  // Bottom-left
        {{glm::vec2{1.0f, 0.0f}}, 0.0f, {}},  // Bottom-right
        {{glm::vec2{1.0f, 1.0f}}, 0.0f, {}},  // Top-right
        {{glm::vec2{0.0f, 1.0f}}, 0.0f, {}}   // Top-left
    };
    
    // Triangle structure that uses WBE::Vertex2D pointers
    struct WBETriangle {
        WBE::Vertex2D<>* vert1;
        WBE::Vertex2D<>* vert2;
        WBE::Vertex2D<>* vert3;
    };
    
    WBETriangle triangles[2] = {};
    
    EXPECT_NO_THROW(
        WBE::SurfaceUtils2D::triangulate(vertices, 4, triangles)
    );
    
    // Verify that we got valid triangles
    EXPECT_NE(triangles[0].vert1, nullptr);
    EXPECT_NE(triangles[0].vert2, nullptr);
    EXPECT_NE(triangles[0].vert3, nullptr);
}

#endif
