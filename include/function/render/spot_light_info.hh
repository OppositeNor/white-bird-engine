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
#ifndef WBE_FILE_SPOT_LIGHT_INFO_HH
#define WBE_FILE_SPOT_LIGHT_INFO_HH

#include "function/ecs/components.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace WhiteBirdEngine {

/**
 * @class SpotLightInfo
 * @brief Render-side spot light record.
 *
 * This is the universal representation used by the renderer; it bundles the
 * raw spot light parameters together with the precomputed light-space view
 * and projection matrices used for shadow map rendering and for transforming
 * surface points into the light's clip space when sampling the shadow map.
 *
 * The field order and the trailing scalar pads are chosen to match the
 * `SpotLightInfo` UBO declared in the lighting shaders so the whole struct
 * can be bound as one uniform field when convenient.
 */
struct SpotLightInfo {
    /**
     * @brief Light-space view matrix (world -> light view space).
     */
    glm::mat4 view;
    /**
     * @brief Light-space projection matrix (light view space -> clip space).
     *
     * The matrix has the y-axis flipped to match Vulkan clip space, so it
     * can be used directly without further patching by the consumer.
     */
    glm::mat4 proj;
    /**
     * @brief World-space position of the light source.
     */
    glm::vec3 position;
    /**
     * @brief Scalar light intensity multiplier.
     */
    float intensity;
    /**
     * @brief World-space, normalized direction the light is pointing.
     */
    glm::vec3 direction;
    /**
     * @brief Cosine of the inner cone half-angle.
     */
    float inner_cone_cos;
    /**
     * @brief Linear-space light color.
     */
    glm::vec3 color;
    /**
     * @brief Cosine of the outer cone half-angle.
     */
    float outer_cone_cos;
    /**
     * @brief Maximum effective light distance.
     */
    float range;
    /**
     * @brief Side length, in texels, of the square shadow map allocated for
     * this light. CPU-side only; not part of the UBO layout above.
     */
    uint32_t shadow_map_resolution = 1024;
};

/**
 * @brief Build a render-side spot light record from an ECS component.
 *
 * Computes the light-space view and projection matrices used for shadow
 * mapping. The view matrix looks from `p_spot_light.position` along
 * `p_spot_light.direction`; the perspective FOV is twice the outer cone
 * half-angle and the far plane is `p_spot_light.range`. The y-axis of the
 * projection matrix is flipped so the result is ready for Vulkan clip
 * space.
 *
 * @param p_spot_light The source spot light component.
 * @return The render-side spot light record.
 */
SpotLightInfo make_spot_light_info(const ComponentSpotLight& p_spot_light);

} // namespace WhiteBirdEngine

#endif
