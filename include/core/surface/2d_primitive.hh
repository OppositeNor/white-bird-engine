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
#ifndef __WBE_2D_PRIMITIVES_HH__
#define __WBE_2D_PRIMITIVES_HH__

#include "primitive_slots.hh"
#include <glm/glm.hpp>

namespace WhiteBirdEngine {

/**
 * @class Vertex2D
 * @brief 2D vertex.
 *
 * @tparam T The type of the slot.
 */
template <typename T = EmptySlot>
struct Vertex2D {
    /**
     * @brief The position of the vertex.
     */
    glm::vec2 position;
    /**
     * @brief The depth of the vertex.
     */
    float depth;
    /**
     * @brief The slot of the vertex for extension properties.
     */
    T slot;
};

/**
 * @class Triangle2D
 * @brief 2D triangle.
 *
 * @tparam TVertSlot The type of the vertex slot.
 * @tparam TSlot The type of the triangle slot.
 */
template <typename TVertSlot, typename TSlot = EmptySlot>
struct Triangle2D {
    /**
     * @brief The first vertex.
     */
    Vertex2D<TVertSlot> vert1;
    /**
     * @brief The second vertex.
     */
    Vertex2D<TVertSlot> vert2;
    /**
     * @brief The third vertex.
     */
    Vertex2D<TVertSlot> vert3;
    /**
     * @brief The slot of the triangle for extension properties.
     */
    TSlot slot;
};

/**
 * @brief Colored triangle.
 */
using Triangle2DColored = Triangle2D<SlotColor>;

/**
 * @brief Textured triangle.
 */
using Triangle2DTextured = Triangle2D<SlotTextured>;

/**
 * @brief Textured skinned triangle.
 */
using Triangle2DTexBone = Triangle2D<SlotUVBone>;


};

#endif
