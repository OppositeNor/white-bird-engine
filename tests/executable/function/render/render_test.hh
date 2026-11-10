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

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/parser/parser_json.hh"
#include "function/ecs/components.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_graph_bindings_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/render_graph_bindings_function_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/vulkan_enums_vk.hh"
#include "function/window/i_window.hh"
#include "platform/file_system/path.hh"
#include "platform/os/os.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <optional>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

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
    output << ParserJSON::write(ParserJSON::read(p_json), 2) << '\n';
    if (!output.good()) {
        throw std::runtime_error(std::format("Failed to write descriptor debug output file: {}.", output_path.generic_string()));
    }
}

inline void write_descriptor_debug_output(std::string_view p_name, const DebugData& p_data) {
    write_descriptor_debug_output(p_name, ParserJSON::write(p_data));
}

/**
 * @brief Read an executable-test switch environment variable.
 *
 * Known switches: WBE_WAIT_PROCESS_INJECT waits for Enter before initialization, WBE_EXEC_TEST_RAY_TRACING builds and
 * imports ray-tracing acceleration structures, WBE_EXEC_TEST_TIMESTAMP_QUERY samples GPU task times and writes a report on
 * exit, and WBE_REAL_TIME_FULL_SCENE_RECONSTRUCT rebuilds the deferred test's static SVO every frame.
 *
 * @param p_variable The environment variable name.
 * @return True when the variable is "1"; false when it is unset, empty, or "0".
 * @throws std::runtime_error If the variable has any other value.
 */
inline bool exec_test_requested(std::string_view p_variable) {
    const std::string variable(p_variable);
    const char* value = std::getenv(variable.c_str());
    if (value == nullptr || std::string_view(value).empty() || std::string_view(value) == "0") {
        return false;
    }
    if (std::string_view(value) == "1") {
        return true;
    }
    throw std::runtime_error(std::format("{} must be 0 or 1, but is \"{}\".", variable, value));
}

/**
 * @brief Parse an executable-test frame number or frame count from an environment variable value.
 * @param p_text The variable value.
 * @param p_error_message The message thrown when the value is not a nonnegative decimal integer.
 * @return The parsed value.
 * @throws std::runtime_error If p_text is not entirely a nonnegative decimal integer.
 */
inline uint64_t parse_exec_test_frame_count(std::string_view p_text, std::string_view p_error_message) {
    const std::optional<uint64_t> value = try_parse_integer<uint64_t>(p_text);
    if (!value.has_value()) {
        throw std::runtime_error(std::string(p_error_message));
    }
    return *value;
}

/**
 * @brief Run the logic-thread executable-test hooks configured by the environment.
 *
 * WBE_EXEC_TEST_RESIZE=<width>x<height>@<frame> resizes the window to width by height once, when the logic thread reaches
 * that logic frame, counted from the first call. Call once per logic frame from the logic thread, which is the thread
 * IWindow::resize() requires.
 *
 * @param p_window The window to resize.
 * @throws std::runtime_error If the variable is malformed.
 */
inline void on_exec_test_logic(IWindow& p_window) {
    static uint64_t logic_frame = 0;
    static bool resized = false;
    const char* resize = std::getenv("WBE_EXEC_TEST_RESIZE");
    if (!resized && resize != nullptr && *resize != '\0') {
        constexpr std::string_view ERROR_MESSAGE = "WBE_EXEC_TEST_RESIZE must be <width>x<height>@<frame>, such as 1280x720@60.";
        const std::string_view value(resize);
        const size_t separator = value.find('x');
        const size_t at = value.find('@');
        if (separator == std::string_view::npos || at == std::string_view::npos || at < separator) {
            throw std::runtime_error(std::string(ERROR_MESSAGE));
        }
        const uint64_t width = parse_exec_test_frame_count(value.substr(0, separator), ERROR_MESSAGE);
        const uint64_t height = parse_exec_test_frame_count(value.substr(separator + 1, at - separator - 1), ERROR_MESSAGE);
        const uint64_t frame = parse_exec_test_frame_count(value.substr(at + 1), ERROR_MESSAGE);
        if (width == 0 || height == 0 || width > UINT32_MAX || height > UINT32_MAX) {
            throw std::runtime_error(std::string(ERROR_MESSAGE));
        }
        if (logic_frame >= frame) {
            p_window.resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
            resized = true;
        }
    }
    ++logic_frame;
}

/**
 * @brief Request the frame captures configured by the environment.
 *
 * WBE_CAPTURE_PNG is the absolute output path and WBE_CAPTURE_FRAME the first captured frame (default 5). Without
 * WBE_CAPTURE_INTERVAL, one frame is captured to WBE_CAPTURE_PNG. With a positive WBE_CAPTURE_INTERVAL, a frame is
 * captured every that many frames from WBE_CAPTURE_FRAME on, each to WBE_CAPTURE_PNG with `_frame<number>` inserted
 * before the extension.
 *
 * @param p_renderer The renderer to request captures from.
 * @param p_frame The current frame number.
 * @return True if a capture of the frame rendered next was requested by this call.
 * @throws std::runtime_error If a variable is malformed.
 */
inline bool on_exec_test_render(IRenderer& p_renderer, uint64_t p_frame) {
    static std::string capture_png_path;
    static uint64_t capture_frame = 5;
    static bool capture_requested = false;
    static uint64_t capture_interval = 0;
    static uint64_t next_capture_frame = 0;
    const char* capture_path = std::getenv("WBE_CAPTURE_PNG");
    if (capture_path != nullptr && *capture_path != '\0') {
        if (*capture_path != '/') {
            throw std::runtime_error("WBE_CAPTURE_PNG must be an absolute output path.");
        }
        capture_png_path = capture_path;
    }
    if (const char* frame = std::getenv("WBE_CAPTURE_FRAME"); frame != nullptr) {
        capture_frame = parse_exec_test_frame_count(frame, "WBE_CAPTURE_FRAME must be a nonnegative frame number.");
    }
    if (const char* interval = std::getenv("WBE_CAPTURE_INTERVAL"); interval != nullptr && *interval != '\0') {
        capture_interval = parse_exec_test_frame_count(interval, "WBE_CAPTURE_INTERVAL must be a positive frame count.");
        if (capture_interval == 0) {
            throw std::runtime_error("WBE_CAPTURE_INTERVAL must be a positive frame count.");
        }
    }
    if (capture_png_path.empty()) {
        return false;
    }
    if (capture_interval == 0) {
        if (!capture_requested && p_frame >= capture_frame) {
            p_renderer.request_frame_capture(capture_png_path);
            capture_requested = true;
            return true;
        }
        return false;
    }
    if (!capture_requested) {
        next_capture_frame = capture_frame;
        capture_requested = true;
    }
    if (p_frame >= next_capture_frame) {
        const Path path(capture_png_path);
        const Path frame_path =
            path.parent_path() / std::format("{}_frame{}{}", path.stem().string(), p_frame, path.extension().string());
        p_renderer.request_frame_capture(frame_path.string());
        next_capture_frame = p_frame + capture_interval;
        return true;
    }
    return false;
}

inline void on_exec_test_start() {

    if (exec_test_requested("WBE_WAIT_PROCESS_INJECT")) {
        std::println("WBE_WAIT_PROCESS_INJECT is set. Running on PID: {}, Press Enter to continue...", OS::get_pid());
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

/**
 * @brief Registration key of the scene's static mesh on the render tests' graphs, which their recipes register.
 */
inline constexpr std::string_view STATIC_MESH_KEY = "static_mesh";

/**
 * @brief Registration key of the deferred test's moving mesh, which its recipe registers.
 */
inline constexpr std::string_view DYNAMIC_MESH_KEY = "dynamic_mesh";

/**
 * @brief Import-name prefix of the entities placing the deferred test's moving mesh.
 */
inline constexpr std::string_view DYNAMIC_MESH_ENTITY_KEY = "dynamic_mesh_entity";

/**
 * @brief Extent of a render test pass, evaluated against the render graph context whenever its task is created.
 */
using RenderTestExtentFactory = std::function<VkExtent2D(const RendGraphContextVK&)>;

/**
 * @brief Acquire a render test's graph instance, write its descriptors to the debug output, and optionally activate it.
 *
 * Call on the rendering thread, or before the application runs. Release the graph with release_test_render_graph()
 * before the renderer shuts down.
 *
 * @param p_renderer The renderer.
 * @param p_resource_key The render graph definition key, declared in res/assets/render_graphs/render_graphs.toml.
 * @param p_bindings The bindings of the instance.
 * @param p_activate Whether to activate the graph.
 * @return The graph.
 */
inline Ref<RenderGraphVK> acquire_test_render_graph(
    RendererVK& p_renderer, std::string_view p_resource_key, Ref<IRenderGraphBindingsVK> p_bindings, bool p_activate = true) {
    Ref<RenderGraphVK> graph = p_renderer.acquire_render_graph(p_resource_key, p_bindings).dynamic_cast_ref<RenderGraphVK>();
    write_descriptor_debug_output(
        std::format("{}_{}_descriptors", p_resource_key, p_bindings->get_instance_key()), graph->list_descriptors());
    if (p_activate) {
        p_renderer.activate_render_graph(graph);
    }
    return graph;
}

/**
 * @brief Clear the renderer's activation and release a render test's graph, after the rendering thread stopped.
 * @param p_renderer The renderer.
 * @param p_graph The graph reference to release.
 */
inline void release_test_render_graph(RendererVK& p_renderer, Ref<RenderGraphVK>& p_graph) {
    p_renderer.activate_render_graph(MEM_NULL);
    p_graph = MEM_NULL;
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

/**
 * @struct CameraInfoGPU
 * @brief CPU-side mirror of the shader `CameraInfo` struct.
 */
struct CameraInfoGPU {
    /**
     * @brief The view matrix.
     */
    glm::mat4 view;
    /**
     * @brief The projection matrix.
     */
    glm::mat4 proj;
    /**
     * @brief The world-space camera position.
     */
    glm::vec4 world_position;
};

/**
 * @brief Maximum number of spot lights in a SpotLightInfosGPU table.
 */
constexpr uint32_t MAX_SPOT_LIGHT_INFO_COUNT = 4;

/**
 * @struct SpotLightInfoGPU
 * @brief CPU-side mirror of the shader `SpotLightInfo` struct.
 */
struct SpotLightInfoGPU {
    /**
     * @brief Light-space view matrix.
     */
    glm::mat4 view;
    /**
     * @brief Light-space projection matrix.
     */
    glm::mat4 proj;
    /**
     * @brief Inverse light-space view-projection matrix used to reconstruct world positions from shadow-map depth.
     */
    glm::mat4 inv_view_proj;
    /**
     * @brief World-space position of the spot light.
     */
    glm::vec3 position;
    /**
     * @brief Scalar intensity multiplier.
     */
    float intensity = 0.0F;
    /**
     * @brief World-space direction the spot is pointing.
     */
    glm::vec3 direction;
    /**
     * @brief Cosine of the inner cone half-angle.
     */
    float inner_cone_cos = 0.0F;
    /**
     * @brief Linear-space color of the spot light.
     */
    glm::vec3 color;
    /**
     * @brief Cosine of the outer cone half-angle.
     */
    float outer_cone_cos = 0.0F;
    /**
     * @brief Maximum effective light distance.
     */
    float range = 0.0F;
    /**
     * @brief Projected emitter width in world units.
     */
    float emitter_width = 0.0F;
    /**
     * @brief Padding to match the shader struct array stride.
     */
    std::array<uint32_t, 2> padding{};
};

static_assert(sizeof(SpotLightInfoGPU) == 256);

/**
 * @struct SpotLightInfosGPU
 * @brief Spot light table with its active-entry count.
 */
struct SpotLightInfosGPU {
    /**
     * @brief Active spot light data consumed by the shader.
     */
    std::array<SpotLightInfoGPU, MAX_SPOT_LIGHT_INFO_COUNT> spot_light_info{};
    /**
     * @brief Number of active entries in spot_light_info.
     */
    uint32_t spot_light_count = 0;
    /**
     * @brief Pads the table to the 16-byte-aligned size of the shader `SpotLightInfoBuffer` struct.
     */
    std::array<uint32_t, 3> padding{};
};

static_assert(sizeof(SpotLightInfosGPU) == MAX_SPOT_LIGHT_INFO_COUNT * sizeof(SpotLightInfoGPU) + 16);

inline CameraInfoGPU make_camera_info_gpu(const WBE::ComponentCamera& p_camera_component) {
    return CameraInfoGPU{
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

inline SpotLightInfoGPU make_spot_light_info_gpu(const WBE::ComponentSpotLight& p_spot_light_component) {
    return SpotLightInfoGPU{
        .view = p_spot_light_component.view,
        .proj = p_spot_light_component.proj,
        .inv_view_proj = glm::inverse(p_spot_light_component.proj * p_spot_light_component.view),
        .position = p_spot_light_component.position,
        .intensity = p_spot_light_component.intensity,
        .direction = p_spot_light_component.direction,
        .inner_cone_cos = p_spot_light_component.inner_cone_cos,
        .color = p_spot_light_component.color,
        .outer_cone_cos = p_spot_light_component.outer_cone_cos,
        .range = p_spot_light_component.range,
        .emitter_width = p_spot_light_component.emitter_width,
    };
}

inline void log_uniform_buffer_descriptors(WBE::Ref<WBE::ROCMultislotsVK> p_buffers, std::string_view p_label) {
    WBE::write_descriptor_debug_output(std::format("{}_descriptors", p_label), p_buffers->to_string());
}

#endif
