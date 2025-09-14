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
#ifndef __WBE_PRIMITIVE_SLOTS_HH__
#define __WBE_PRIMITIVE_SLOTS_HH__

#include <glm/glm.hpp>

namespace WhiteBirdEngine {

/**
 * @class EmptySlot
 * @brief Empty slot.
 *
 */
struct EmptySlot {};

/**
 * @class SlotUV
 * @brief Slot for uv.
 *
 */
struct SlotTextured {
    /**
     * @brief The uv value.
     */
    glm::vec2 uv;
};

/**
 * @class SlotColor
 * @brief Slot for color.
 *
 */
struct SlotColor {
    /**
     * @brief The color value.
     */
    glm::vec4 color;
};

struct SlotUVBone {
    /**
     * @brief The UV value.
     */
    glm::vec2 uv;
    /**
     * @brief The bone ids.
     */
    glm::ivec4 bone_id;
    /**
     * @brief The weight of the bones.
     */
    glm::vec4 weight;
};

}

#endif

