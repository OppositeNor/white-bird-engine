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
#ifndef WBE_FILE_RENDER_TEST_HH
#define WBE_FILE_RENDER_TEST_HH
#include "core/reflection/reflection_defs.hh"
#include "function/ecs/components.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <string_view>

namespace WhiteBirdEngine {
inline constexpr HashCode TEST_RENDER_LABEL_HASH_ANCHOR = static_hash("TEST_RENDER_LABEL_HASH_ANCHOR");
WBE_LABEL(RT_RUN_PIPELINE, WBE_RENDER_TASK);
WBE_LABEL(RT_RUN_PIPELINE_SHADOWMAP, WBE_RENDER_TASK);
inline constexpr std::string_view REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE = "REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE";
inline constexpr std::string_view REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE = "REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE";
inline constexpr std::string_view REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW = "REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW";
inline constexpr std::string_view REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW = "REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW";
inline constexpr glm::vec3 TEST_DEFAULT_CAMERA_POSITION = glm::vec3(2.0F, 2.0F, 2.0F);
inline constexpr glm::vec3 TEST_DEFAULT_CAMERA_TARGET = glm::vec3(0.0F, 0.0F, 0.0F);
inline constexpr float TEST_CAMERA_MOVE_SPEED = 3.0F;
inline constexpr float TEST_CAMERA_TURN_SPEED = glm::radians(90.0F);
inline constexpr float TEST_CAMERA_PITCH_LIMIT = glm::radians(89.0F);

inline glm::vec3 default_test_camera_direction() {
    return glm::normalize(TEST_DEFAULT_CAMERA_TARGET - TEST_DEFAULT_CAMERA_POSITION);
}

inline float default_test_camera_yaw() {
    glm::vec3 direction = default_test_camera_direction();
    return std::atan2(direction.z, direction.x);
}

inline float default_test_camera_pitch() {
    return std::asin(default_test_camera_direction().y);
}

inline glm::vec3 test_camera_forward(float p_yaw, float p_pitch) {
    return glm::normalize(glm::vec3(std::cos(p_pitch) * std::cos(p_yaw), std::sin(p_pitch), std::cos(p_pitch) * std::sin(p_yaw)));
}

inline glm::mat4 test_camera_view(const glm::vec3& p_position, float p_yaw, float p_pitch) {
    glm::vec3 forward = test_camera_forward(p_yaw, p_pitch);
    glm::vec3 up = std::abs(forward.y) > 0.99F ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
    return glm::lookAt(p_position, p_position + forward, up);
}

inline ComponentCamera make_default_test_camera_component(float p_aspect_ratio, float p_far_plane = 10.0F) {
    ComponentCamera camera_component{
        .view = test_camera_view(TEST_DEFAULT_CAMERA_POSITION, default_test_camera_yaw(), default_test_camera_pitch()),
        .proj = glm::perspective(glm::radians(45.0F), p_aspect_ratio, 0.1F, p_far_plane),
        .is_dirty = false,
    };
    camera_component.proj[1][1] *= -1;
    return camera_component;
}

inline CameraInfoVKUBO make_camera_info_ubo(const ComponentCamera& p_camera_component) {
    return CameraInfoVKUBO{.view = p_camera_component.view, .proj = p_camera_component.proj};
}

inline ComponentSpotLight make_default_test_spot_light_component(float p_intensity) {
    ComponentSpotLight spot_light_component{
        .position = glm::vec3(2.5F, 3.5F, 2.5F),
        .direction = glm::normalize(glm::vec3(-1.0F, -1.4F, -1.0F)),
        .color = glm::vec3(1.0F, 0.95F, 0.85F),
        .intensity = p_intensity,
        .inner_cone_cos = std::cos(glm::radians(18.0F)),
        .outer_cone_cos = std::cos(glm::radians(28.0F)),
        .range = 12.0F,
    };

    glm::vec3 up = std::abs(spot_light_component.direction.y) > 0.99F ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
    spot_light_component.view =
        glm::lookAt(spot_light_component.position, spot_light_component.position + spot_light_component.direction, up);
    float outer_half_angle = std::acos(std::clamp(spot_light_component.outer_cone_cos, -1.0F, 1.0F));
    float far_plane = std::max(spot_light_component.range, 0.2F);
    spot_light_component.proj = glm::perspective(2.0F * outer_half_angle, 1.0F, 0.1F, far_plane);
    spot_light_component.proj[1][1] *= -1.0F;
    return spot_light_component;
}

inline SpotLightInfoVKUBO make_spot_light_info_ubo(const ComponentSpotLight& p_spot_light_component) {
    return SpotLightInfoVKUBO{.view = p_spot_light_component.view,
        .proj = p_spot_light_component.proj,
        .position = p_spot_light_component.position,
        .intensity = p_spot_light_component.intensity,
        .direction = p_spot_light_component.direction,
        .inner_cone_cos = p_spot_light_component.inner_cone_cos,
        .color = p_spot_light_component.color,
        .outer_cone_cos = p_spot_light_component.outer_cone_cos,
        .range = p_spot_light_component.range};
}

} // namespace WhiteBirdEngine

#endif
