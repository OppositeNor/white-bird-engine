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

#include "core/allocator/allocator.hh"
#include "core/memory/reference_raw.hh"
#include "primitive_slots.hh"
#include "core/reflection/reflection_defs.hh"
#include <cstdint>
#include <glm/glm.hpp>

namespace WhiteBirdEngine {

/**
 * @class Vertex3D
 * @brief 3D vertex.
 *
 * @tparam T The type of the slot.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) Vertex3D {
    /**
     * @brief The position of the vertex.
     */
    WBE_META(WBE_REFLECT)
    glm::vec3 position;
    /**
     * @brief The normal of the vertex.
     */
    WBE_META(WBE_REFLECT)
    glm::vec3 normal;
    /**
     * @brief The tanglent vector of the vertex.
     */
    WBE_META(WBE_REFLECT)
    glm::vec3 tangent;
    /**
     * @brief The bitangent vector of the vertex.
     */
    WBE_META(WBE_REFLECT)
    glm::vec3 bitangent;
};

/**
 * @class Triangle3D
 * @brief 3D triangle.
 *
 * @tparam TVertType The type of vertices.
 */
template <typename TVertType>
struct Triangle3D {
    /**
     * @brief The first vertex of the triangle.
     */
    TVertType vert1;
    /**
     * @brief The second vertex of the triangle.
     */
    TVertType vert2;
    /**
     * @brief The third vertex of the triangle.
     */
    TVertType vert3;
};

/**
 * @brief Textured vertex.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) Vertex3DTextured : public Vertex3D {
    WBE_VERT_SLOT_TEXTURED
};

/**
 * @class Triangle3DIndx
 * @brief Indexed 3D triangle.
 *
 * @tparam TVertType The type of the vertices.
 */
template <typename TVertType>
struct Triangle3DIndx {
    /**
     * @brief The vertex array.
     */
    RefRaw<TVertType, Allocator> vert_array;
    struct Indices {
        uint32_t vert_1;
        uint32_t vert_2;
        uint32_t vert_3;
    };
    /**
     * @brief The indices of the vertices.
     */
    union {
        Indices indices;
        uint32_t vertex_indices[3];
    };
    /**
     * @brief The object ID of this triangle.
     */
    uint32_t obj_id;
};

using Triangle3DIndxTextured = Triangle3DIndx<Vertex3DTextured>;
} // namespace WhiteBirdEngine

#endif
