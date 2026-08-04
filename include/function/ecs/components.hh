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
#ifndef WBE_FILE_COMPONENTS_HH
#define WBE_FILE_COMPONENTS_HH

#include "core/reflection/reflection_defs.hh"
#include "function/function_utils.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string_view>
namespace WhiteBirdEngine {

/**
 * @class ComponentLabel
 * @brief Label of the entity.
 */
struct WBE_META(WBE_COMPONENT) ComponentLabel {
    static constexpr size_t MAX_NAME_SIZE = 15;
    // The buffer for the name.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    Buffer<MAX_NAME_SIZE + 1> name_buffer;
    // The name of the component.
    std::string_view name;

    ComponentLabel() : name_buffer({.buffer = ""}), name(name_buffer.buffer) {
    }
};

/**
 * @class ComponentTransform
 * @brief Component that stores a transform based on scale, rotation and
 * translation data. The transform will be performed in the order of: scale,
 * rotate and then translation (i.e. translation * rotation * scale).
 */
struct WBE_META(WBE_COMPONENT) ComponentTransform {
    // The scale value: x, y, z
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::vec3 scale;
    // The rotation value in quaternion: r, x, y, z
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::quat rotation;
    // The translation value: x, y, z
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::vec3 translation;
};

/**
 * @class ComponentTransform
 * @brief Transform component.
 */
struct WBE_META(WBE_COMPONENT) ComponentTransformMats {
    // The local transform matrix.
    glm::mat4 local_transform = glm::mat4(1);
    // The global transform matrix.
    glm::mat4 global_transform = glm::mat4(1);
    // A counter to check if this transform is up to date.
    int64_t version = -1;
};

/**
 * @class ComponentCamera
 * @brief Render-facing camera component mirror used by gameplay code.
 *
 * This mirrors the Vulkan camera UBO payload and adds a placeholder dirty
 * bit for future scene synchronization.
 */
struct WBE_META(WBE_COMPONENT) ComponentCamera {
    // The camera-local transform relative to the owning entity.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::mat4 local_transform = glm::mat4(1);
    // The view matrix.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::mat4 view = glm::mat4(1);
    // The projection matrix.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::mat4 proj = glm::mat4(1);
    // Placeholder dirty bit for future scene synchronization.
    WBE_META(WBE_REFLECT)
    bool is_dirty = false;
};

/**
 * @class ComponentHierarchy
 * @brief Hierarchy relation for an entity.
 */
struct WBE_META(WBE_COMPONENT) ComponentHierarchy {
    // The parent of this entity.
    EntityID parent = entt::null;
};

/**
 * @class ComponentSpotLight
 * @brief A spot light source. The spot light shines from `position` along
 *        `direction` and is fully bright inside the inner cone, fades
 *        smoothly to zero between the inner and outer cones, and is also
 *        attenuated by distance up to `range`. Cone angles are stored as
 *        cosines of the half-angle to keep shader evaluation cheap.
 */
struct WBE_META(WBE_COMPONENT) ComponentSpotLight {
    // World-space position of the light source.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::vec3 position;
    // World-space direction the light is pointing (unit vector).
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::vec3 direction;
    // Linear-space light color.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::vec3 color;
    // Scalar light intensity multiplier (radiant flux scale).
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    float intensity;
    // Cosine of the inner cone half-angle. Inside this cone the spot
    // attenuation is 1.0.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    float inner_cone_cos;
    // Cosine of the outer cone half-angle. Outside this cone the spot
    // attenuation is 0.0.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    float outer_cone_cos;
    // Maximum effective light distance. Intensity smoothly falls to zero
    // as the surface-to-light distance approaches this value.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    float range;
    // Side length, in texels, of the square shadow map allocated for this
    // light. Higher values give crisper shadows at the cost of more memory
    // and longer shadow-pass render times.
    WBE_META(WBE_REFLECT)
    uint32_t shadow_map_resolution = 1024;
};

} // namespace WhiteBirdEngine
#endif
