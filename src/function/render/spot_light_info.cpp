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
#include "function/render/spot_light_info.hh"
#include "function/ecs/components.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

namespace WhiteBirdEngine {

SpotLightInfo make_spot_light_info(const ComponentSpotLight& p_spot_light) {
    glm::vec3 direction = glm::normalize(p_spot_light.direction);
    // Pick a stable up vector that is not collinear with the light direction
    // so glm::lookAt produces a well-defined view matrix.
    glm::vec3 up = std::abs(direction.y) > 0.99F ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
    glm::mat4 view = glm::lookAt(p_spot_light.position, p_spot_light.position + direction, up);

    float outer_half_angle = std::acos(std::clamp(p_spot_light.outer_cone_cos, -1.0F, 1.0F));
    float fov = 2.0F * outer_half_angle;
    float far_plane = std::max(p_spot_light.range, 0.2F);
    glm::mat4 proj = glm::perspective(fov, 1.0F, 0.1F, far_plane);
    proj[1][1] *= -1.0F;

    return SpotLightInfo{
        .view = view,
        .proj = proj,
        .position = p_spot_light.position,
        .intensity = p_spot_light.intensity,
        .direction = direction,
        .inner_cone_cos = p_spot_light.inner_cone_cos,
        .color = p_spot_light.color,
        .outer_cone_cos = p_spot_light.outer_cone_cos,
        .range = p_spot_light.range,
        .shadow_map_resolution = p_spot_light.shadow_map_resolution,
    };
}

} // namespace WhiteBirdEngine
