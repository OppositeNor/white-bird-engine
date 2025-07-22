#ifndef __WBE_PRIMITIVE_SLOTS_H__
#define __WBE_PRIMITIVE_SLOTS_H__

#include <cstdint>
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
struct SlotUV {
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

