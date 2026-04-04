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
#ifndef WBE_FILE_2D_PRIMITIVES_HH
#define WBE_FILE_2D_PRIMITIVES_HH

#include "core/reflection/reflection_defs.hh"
#include "glm/ext/vector_float2.hpp"

namespace WhiteBirdEngine {

/**
 * @class Vertex2D
 * @brief 2D vertex.
 *
 * @tparam T The type of the slot.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) Vertex2D {
    /**
     * @brief The position of the vertex.
     */
    WBE_META(WBE_REFLECT)
    glm::vec2 position;
    /**
     * @brief The depth of the vertex.
     */
    WBE_META(WBE_REFLECT)
    float depth;
};

/**
 * @brief Textured vertex.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) Vertex2DTextured {
    /**
     * @brief The position of the vertex.
     */
    WBE_META(WBE_REFLECT)
    glm::vec2 position;
    /**
     * @brief The depth of the vertex.
     */
    WBE_META(WBE_REFLECT)
    float depth;
    /**
     * @brief The uv value.
     */
    WBE_META(WBE_REFLECT)
    glm::vec2 uv;
};

/**
 * @class Triangle2D
 * @brief 2D triangle.
 *
 * @tparam TVertType The type of the vertices.
 */
template <typename TVertType>
struct Triangle2D {
    /**
     * @brief The first vertex.
     */
    TVertType vert1;
    /**
     * @brief The second vertex.
     */
    TVertType vert2;
    /**
     * @brief The third vertex.
     */
    TVertType vert3;
};
} // namespace WhiteBirdEngine

#endif
