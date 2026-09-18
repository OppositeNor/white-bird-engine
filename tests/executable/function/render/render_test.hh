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
#include "core/memory/reference_strong.hh"
#include "function/ecs/components.hh"
#include "function/render/i_renderer.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/roc_per_frame_vk.hh"
#include "platform/os/os.hh"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <print>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace WBE = WhiteBirdEngine;

inline constexpr std::string_view RT_RUN_PIPELINE = "run_pipeline";
inline constexpr std::string_view RT_RUN_PIPELINE_SHADOWMAP = "run_shadowmap";
inline constexpr std::string_view REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW = "shadowmap_color";
inline constexpr std::string_view REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW = "shadowmap_depth";
inline constexpr std::string_view REND_RES_TEST_RSM_NORMAL_IMAGE_VIEW = "rsm_normal";
inline constexpr glm::vec3 TEST_DEFAULT_CAMERA_POSITION = glm::vec3(2.0F, 2.0F, 2.0F);
inline constexpr glm::vec3 TEST_DEFAULT_CAMERA_TARGET = glm::vec3(0.0F, 0.0F, 0.0F);
inline constexpr float TEST_CAMERA_MOVE_SPEED = 3.0F;
inline constexpr float TEST_CAMERA_TURN_SPEED = glm::radians(90.0F);
inline constexpr float TEST_CAMERA_PITCH_LIMIT = glm::radians(89.0F);

namespace WhiteBirdEngine {

inline std::string make_descriptor_debug_file_stem(std::string_view p_name) {
    std::string result;
    result.reserve(p_name.size());
    for (char character : p_name) {
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')) {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        } else {
            result.push_back('_');
        }
    }
    if (result.empty()) {
        throw std::runtime_error("Descriptor debug output name is empty.");
    }
    return result;
}

inline void write_descriptor_debug_output(std::string_view p_name, std::string_view p_json) {
    std::filesystem::path output_dir = std::filesystem::path("test_env") / "descriptor_debug";
    std::filesystem::create_directories(output_dir);
    std::filesystem::path output_path = output_dir / std::format("{}.json", make_descriptor_debug_file_stem(p_name));
    std::ofstream output(output_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error(std::format("Failed to open descriptor debug output file: {}.", output_path.generic_string()));
    }
    output << nlohmann::json::parse(p_json).dump(2) << '\n';
    if (!output.good()) {
        throw std::runtime_error(std::format("Failed to write descriptor debug output file: {}.", output_path.generic_string()));
    }
}

inline void write_descriptor_debug_output(std::string_view p_name, const DebugData& p_data) {
    std::ostringstream stream;
    stream << p_data;
    write_descriptor_debug_output(p_name, stream.str());
}

inline void on_exec_test_render(IRenderer& p_renderer, uint64_t p_frame) {
    static std::string capture_png_path;
    static uint64_t capture_frame = 5;
    static bool capture_requested = false;
    const char* capture_path = std::getenv("WBE_CAPTURE_PNG");
    if (capture_path != nullptr && *capture_path != '\0') {
        if (*capture_path != '/') {
            throw std::runtime_error("WBE_CAPTURE_PNG must be an absolute output path.");
        }
        capture_png_path = capture_path;
    }
    if (const char* frame = std::getenv("WBE_CAPTURE_FRAME"); frame != nullptr) {
        std::string frame_text(frame);
        const char* frame_end = frame_text.data() + frame_text.size();
        auto result = std::from_chars(frame_text.c_str(), frame_end, capture_frame);
        if (result.ec != std::errc{} || result.ptr != frame_end) {
            throw std::runtime_error("WBE_CAPTURE_FRAME must be a nonnegative frame number.");
        }
    }
    if (!capture_png_path.empty() && !capture_requested && p_frame >= capture_frame) {
        p_renderer.request_frame_capture(capture_png_path);
        capture_requested = true;
    }
}

inline void on_exec_test_start() {

    const char* wait_process_inject = std::getenv("WBE_WAIT_PROCESS_INJECT");
    if (wait_process_inject != nullptr && std::string_view(wait_process_inject) == "1") {
        std::println("WBE_WAIT_PROCESS_INJECT is set. Running on PID: {}, Press Enter to continue...", OS::get_pid());
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}
} // namespace WhiteBirdEngine

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

inline WBE::ComponentCamera make_default_test_camera_component(float p_aspect_ratio, float p_near_plane = 10.0F) {
    WBE::ComponentCamera camera_component{
        .view = test_camera_view(TEST_DEFAULT_CAMERA_POSITION, default_test_camera_yaw(), default_test_camera_pitch()),
        .proj = glm::perspectiveRH_ZO(glm::radians(45.0F), p_aspect_ratio, p_near_plane, 0.1F),
        .is_dirty = false,
    };
    camera_component.proj[1][1] *= -1;
    return camera_component;
}

inline WBE::CameraInfoVKUBO make_camera_info_ubo(const WBE::ComponentCamera& p_camera_component) {
    return WBE::CameraInfoVKUBO{
        .view = p_camera_component.view,
        .proj = p_camera_component.proj,
        .world_position = glm::inverse(p_camera_component.view)[3],
    };
}

inline WBE::ComponentSpotLight make_default_test_spot_light_component(float p_intensity) {
    WBE::ComponentSpotLight spot_light_component{
        .position = glm::vec3(2.5F, 3.5F, 2.5F),
        .direction = glm::normalize(glm::vec3(-1.0F, -1.4F, -1.0F)),
        .color = glm::vec3(1.0F, 0.95F, 0.85F),
        .intensity = p_intensity,
        .inner_cone_cos = std::cos(glm::radians(18.0F)),
        .outer_cone_cos = std::cos(glm::radians(28.0F)),
        .range = 12.0F,
        .emitter_width = 0.05F,
    };

    glm::vec3 up = std::abs(spot_light_component.direction.y) > 0.99F ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
    spot_light_component.view =
        glm::lookAt(spot_light_component.position, spot_light_component.position + spot_light_component.direction, up);
    float outer_half_angle = std::acos(std::clamp(spot_light_component.outer_cone_cos, -1.0F, 1.0F));
    float far_plane = std::max(spot_light_component.range, 0.2F);
    spot_light_component.proj = glm::perspectiveRH_ZO(2.0F * outer_half_angle, 1.0F, far_plane, 0.1F);
    spot_light_component.proj[1][1] *= -1.0F;
    return spot_light_component;
}

inline WBE::SpotLightInfoVKUBO make_spot_light_info_ubo(const WBE::ComponentSpotLight& p_spot_light_component) {
    return WBE::SpotLightInfoVKUBO{.view = p_spot_light_component.view,
        .proj = p_spot_light_component.proj,
        .inv_view_proj = glm::inverse(p_spot_light_component.proj * p_spot_light_component.view),
        .position = p_spot_light_component.position,
        .intensity = p_spot_light_component.intensity,
        .direction = p_spot_light_component.direction,
        .inner_cone_cos = p_spot_light_component.inner_cone_cos,
        .color = p_spot_light_component.color,
        .outer_cone_cos = p_spot_light_component.outer_cone_cos,
        .range = p_spot_light_component.range,
        .emitter_width = p_spot_light_component.emitter_width};
}

inline void log_uniform_buffer_descriptors(WBE::Ref<WBE::ROCPerFrameVK> p_buffers, std::string_view p_label) {
    WBE::write_descriptor_debug_output(std::format("{}_descriptors", p_label), p_buffers->to_string());
}

#endif
