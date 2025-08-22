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
    /**
     * @brief Is the texture opaque.
     */
    bool is_opaque;
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
    /**
     * @brief Is the color opaque.
     */
    bool is_opaque;
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

