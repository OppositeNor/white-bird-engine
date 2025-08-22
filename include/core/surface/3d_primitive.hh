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
#ifndef __WBE_3D_PRIMITIVE_HH__
#define __WBE_3D_PRIMITIVE_HH__

#include "primitive_slots.hh"
#include <glm/glm.hpp>

namespace WhiteBirdEngine {

/**
 * @class Vertex3D
 * @brief 3D vertex.
 *
 * @tparam T The type of the slot.
 */
template <typename T = EmptySlot>
struct Vertex3D {
    /**
     * @brief The position of the vertex.
     */
    glm::vec3 position;
    /**
     * @brief The normal of the vertex.
     */
    glm::vec3 normal;
    /**
     * @brief The tanglent vector of the vertex.
     */
    glm::vec3 tangent;
    /**
     * @brief The bitangent vector of the vertex.
     */
    glm::vec3 bitangent;
    /**
     * @brief The slot used for extension.
     */
    T slot;
};

/**
 * @class Triangle3D
 * @brief 3D triangle.
 *
 * @tparam TVertSlot The type of vertex.
 * @tparam TSlot The type of the triangle slot.
 */
template <typename TVertSlot, typename TSlot = EmptySlot>
struct Triangle3D {
    /**
     * @brief The first vertex of the triangle.
     */
    Vertex3D<TVertSlot> vert1;
    /**
     * @brief The second vertex of the triangle.
     */
    Vertex3D<TVertSlot> vert2;
    /**
     * @brief The third vertex of the triangle.
     */
    Vertex3D<TVertSlot> vert3;
    /**
     * @brief The slot used for extension.
     */
    TSlot slot;
};

/**
 * @brief Colored triangle.
 */
using Triangle3DColored = Triangle3D<SlotColor>;

/**
 * @brief Textured triangle.
 */
using Triangle3DTextured = Triangle3D<SlotTextured>;

/**
 * @brief Textured skinned triangle.
 */
using Triangle3DTexBone = Triangle3D<SlotUVBone>;
}

#endif
