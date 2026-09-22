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
#include "core/allocator/i_allocator.hh"
#include "core/engine_core.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/function_utils.hh"
#include "function/input_manager/input_manager.hh"
#include "function/input_manager/key_codes.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_run_static_set_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "global/global.hh"
#include "platform/file_system/path.hh"
#include "render_test.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

constexpr std::string_view REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW = "sponza_depth";

namespace WBE = WhiteBirdEngine;

namespace {

constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOWMAP_COLOR_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_COLOR_FORMAT;
constexpr VkFormat MAIN_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr std::string_view RT_RUN_SHADOWMAP_OPAQUE = "run_shadowmap_opaque";
constexpr std::string_view RT_RUN_SHADOWMAP_MASKED = "run_shadowmap_masked";
constexpr std::string_view RT_RUN_MAIN_OPAQUE = "run_main_opaque";
constexpr std::string_view RT_RUN_MAIN_MASKED = "run_main_masked";
constexpr std::string_view RT_RUN_LIGHT_BILLBOARD = "run_light_billboard";
constexpr glm::vec2 LIGHT_BILLBOARD_SIZE = glm::vec2(0.35F, 0.35F);

struct ForwardSceneViewConfig {
    glm::vec3 camera_position = glm::vec3(0.0F);
    float camera_yaw = 0.0F;
    float camera_pitch = 0.0F;
    glm::vec3 light_position = glm::vec3(0.0F);
    float light_yaw = 0.0F;
    float light_pitch = 0.0F;
};

ForwardSceneViewConfig make_forward_sponza_view_config() {
    return ForwardSceneViewConfig{
        .camera_position = glm::vec3(1.894651F, 1.6352656F, 4.6991568F),
        .camera_yaw = glm::radians(-77.84428F),
        .camera_pitch = glm::radians(-8.191941F),
        .light_position = glm::vec3(2.9011555F, 1.7444739F, 2.8511076F),
        .light_yaw = glm::radians(-130.4772F),
        .light_pitch = glm::radians(-44.71062F),
    };
}

struct BillboardInfoVKUBO {
    glm::vec4 position = glm::vec4(0.0F);
    glm::vec2 size = glm::vec2(0.0F);
    glm::vec2 size_padding = glm::vec2(0.0F);
    glm::vec4 rotation_axis = glm::vec4(0.0F);
};

BillboardInfoVKUBO make_light_billboard_info(const WBE::ComponentSpotLight& p_spot_light_component) {
    return BillboardInfoVKUBO{
        .position = glm::vec4(p_spot_light_component.position, 1.0F),
        .size = LIGHT_BILLBOARD_SIZE,
        .rotation_axis = glm::vec4(0.0F, 1.0F, 0.0F, 0.0F),
    };
}

WBE::Ref<WBE::ROCMultislotsVK> create_per_frame_uniform_buffers(
    WBE::RenderAllocator* p_allocator, const WBE::BaseRenderDataVK* p_render_data, VkDeviceSize p_buffer_size) {
    return WBE::make_ref<WBE::ROCMultislotsVK>(p_allocator,
        p_allocator,
        p_render_data->frames_in_flight,
        WBE::ROCMultislotsVK::Factory{[p_render_data, p_buffer_size](uint32_t /*p_frame_slot*/) -> WBE::Ref<WBE::IRenderObjectVK> {
            return WBE::ROBufferVK::Builder().set_properties(p_buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT).create_ref(p_render_data);
        }});
}

} // namespace

class ApplicationVulkanDrawSponzaTest : public WBE::ApplicationGame {
public:
    virtual void logic_update(double p_delta) override;
    virtual void render_update(double p_delta) override;

    void setup_draw_task(const ForwardSceneViewConfig& p_view_config);
    void log_view_state() const;

private:
    void setup_render_context();
    void load_resources();
    void setup_static_scene_resources();
    void setup_spot_light();
    void create_shadow_map_resources();
    void create_main_depth_resources();
    void setup_light_billboard_overlay_resources();
    void write_shadow_dynamic_descriptors(WBE::RTRunStaticSetVK& p_task);
    void write_main_dynamic_descriptors(WBE::RTRunStaticSetVK& p_task);
    void register_draw_sponza_render_graph(WBE::Unique<WBE::RTRunStaticSetVK>& p_shadow_opaque_task,
        WBE::Unique<WBE::RTRunStaticSetVK>& p_shadow_masked_task,
        WBE::Unique<WBE::RTRunStaticSetVK>& p_main_opaque_task,
        WBE::Unique<WBE::RTRunStaticSetVK>& p_main_masked_task);

    WBE::RendererVK* renderer_vk = nullptr;
    const WBE::BaseRenderDataVK* base_render_data = nullptr;
    WBE::RenderAllocator* render_allocator = nullptr;
    WBE::RefRes<WBE::RUStaticOpaqueSet> sponza_opaque_set;
    WBE::RefRes<WBE::RUStaticMaskedSet> sponza_masked_set;
    WBE::RefRes<WBE::RUGraphicsPipeline> shadow_opaque_pipeline;
    WBE::RefRes<WBE::RUGraphicsPipeline> shadow_masked_pipeline;
    WBE::RefRes<WBE::RUGraphicsPipeline> main_opaque_pipeline;
    WBE::RefRes<WBE::RUGraphicsPipeline> main_masked_pipeline;
    WBE::Ref<WBE::ROImageVK> shadow_depth_image;
    WBE::Ref<WBE::ROImageVK> shadow_color_image;
    WBE::Ref<WBE::ROImageViewVK> shadow_depth_view;
    WBE::Ref<WBE::ROImageViewVK> shadow_color_view;
    WBE::Ref<WBE::ROImageVK> main_depth_image;
    WBE::Ref<WBE::ROImageViewVK> main_depth_view;
    WBE::Ref<WBE::ROSamplerVK> shadow_sampler;
    WBE::Ref<WBE::ROCMultislotsVK> camera_info_ubos;
    WBE::Ref<WBE::ROCMultislotsVK> spot_light_info_ubos;
    WBE::Ref<WBE::ROCMultislotsVK> spot_light_count_ubos;
    WBE::ComponentSpotLight spot_light_component{};
    float spot_light_yaw = 0.0F;
    float spot_light_pitch = 0.0F;
    VkExtent2D shadow_extent{};
    WBE::Ref<WBE::ROBufferVK> light_billboard_ubo;
    WBE::Ref<WBE::ROImageViewVK> light_billboard_texture_view;
    WBE::Ref<WBE::ROGraphicsPipelineVK> light_billboard_pipeline;
    WBE::Ref<WBE::ROSamplerVK> light_billboard_sampler;
    WBE::ComponentCamera camera_component{};
    glm::vec3 camera_position = glm::vec3(0.0F);
    float camera_yaw = 0.0F;
    float camera_pitch = 0.0F;
};

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Path("test_env"));
    {
        ApplicationVulkanDrawSponzaTest game_application;
        {
            WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiler");
            game_application.setup_draw_task(make_forward_sponza_view_config());
            game_application.run();
            game_application.log_view_state();
            size_t renderer_alloc_max = game_application.get_renderer_allocator()->get_internal_fragmentation_tracker();
            auto global_alloc_max_by_arena = WBE::EngineCore::get_singleton()->pool_allocator->get_internal_fragmentation_tracker();
            for (size_t arena_index = 0; arena_index < global_alloc_max_by_arena.size(); ++arena_index) {
                WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)
                    ->message(std::format("Global allocator arena {} max used: {}", arena_index, global_alloc_max_by_arena[arena_index]));
            }
            WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator max used: {}", renderer_alloc_max));
        }
    }
    const auto& global_profiles = global->engine_core->profiling_manager->get_profile_data(WBE::WBE_CHANNEL_GLOBAL);
    for (const auto& profile : global_profiles) {
        WBE::stdout_log(WBE::WBE_CHANNEL_GLOBAL)->message(std::format("{}: Delta: {} seconds", profile.message, profile.delta));
    }
    return EXIT_SUCCESS;
}

void ApplicationVulkanDrawSponzaTest::logic_update(double p_delta) { // NOLINT
    WBE::InputManager* input_manager = WBE::InputManager::get_singleton();
    if (input_manager == nullptr) {
        return;
    }

    float delta = static_cast<float>(p_delta);
    if (input_manager->get_input_status(WBE::KEYCODE_LEFT).is_down()) {
        camera_yaw -= TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_RIGHT).is_down()) {
        camera_yaw += TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_UP).is_down()) {
        camera_pitch += TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_DOWN).is_down()) {
        camera_pitch -= TEST_CAMERA_TURN_SPEED * delta;
    }
    camera_pitch = std::clamp(camera_pitch, -TEST_CAMERA_PITCH_LIMIT, TEST_CAMERA_PITCH_LIMIT);

    glm::vec3 forward = test_camera_forward(camera_yaw, camera_pitch);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    float move_delta = TEST_CAMERA_MOVE_SPEED * delta;

    if (input_manager->get_input_status(WBE::KEYCODE_W).is_down()) {
        camera_position += forward * move_delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_S).is_down()) {
        camera_position -= forward * move_delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_D).is_down()) {
        camera_position += right * move_delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_A).is_down()) {
        camera_position -= right * move_delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_E).is_down()) {
        camera_position += up * move_delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_Q).is_down()) {
        camera_position -= up * move_delta;
    }

    bool spot_light_changed = false;
    if (input_manager->get_input_status(WBE::KEYCODE_I).is_down()) {
        spot_light_component.position.z -= move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_K).is_down()) {
        spot_light_component.position.z += move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_J).is_down()) {
        spot_light_component.position.x -= move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_L).is_down()) {
        spot_light_component.position.x += move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_U).is_down()) {
        spot_light_component.position.y += move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_O).is_down()) {
        spot_light_component.position.y -= move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_Y).is_down()) {
        spot_light_pitch += TEST_CAMERA_TURN_SPEED * delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_P).is_down()) {
        spot_light_pitch -= TEST_CAMERA_TURN_SPEED * delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_H).is_down()) {
        spot_light_yaw -= TEST_CAMERA_TURN_SPEED * delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_SEMICOLON).is_down()) {
        spot_light_yaw += TEST_CAMERA_TURN_SPEED * delta;
        spot_light_changed = true;
    }
    if (spot_light_changed) {
        spot_light_pitch = std::clamp(spot_light_pitch, -TEST_CAMERA_PITCH_LIMIT, TEST_CAMERA_PITCH_LIMIT);
        spot_light_component.direction = test_camera_forward(spot_light_yaw, spot_light_pitch);
        glm::vec3 light_up =
            std::abs(spot_light_component.direction.y) > 0.99F ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
        spot_light_component.view =
            glm::lookAt(spot_light_component.position, spot_light_component.position + spot_light_component.direction, light_up);
    }

    camera_component.view = test_camera_view(camera_position, camera_yaw, camera_pitch);

    if (camera_info_ubos == WBE::MEM_NULL || spot_light_info_ubos == WBE::MEM_NULL || light_billboard_ubo == WBE::MEM_NULL) {
        return;
    }

    WBE::CameraInfoVKUBO camera_info = make_camera_info_ubo(camera_component);
    WBE::SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    BillboardInfoVKUBO light_billboard_info = make_light_billboard_info(spot_light_component);
    add_render_job([this, camera_info, spot_light_info, light_billboard_info]() {
        camera_info_ubos->publish_buffer_update(&camera_info, 0, sizeof(camera_info));
        spot_light_info_ubos->publish_buffer_update(&spot_light_info, 0, sizeof(spot_light_info));
        light_billboard_ubo->update_buffer(&light_billboard_info, sizeof(light_billboard_info));
    });
}

void ApplicationVulkanDrawSponzaTest::render_update(double /* p_delta */) {
    WBE::on_exec_test_render(*renderer_vk, base_render_data->current_frame);
}

void ApplicationVulkanDrawSponzaTest::log_view_state() const {
    glm::vec3 camera_forward = test_camera_forward(camera_yaw, camera_pitch);
    WBE::stdout_log(WBE::WBE_CHANNEL_GLOBAL)
        ->message(std::format("Forward Sponza camera: position=({}, {}, {}), yaw_deg={}, pitch_deg={}, forward=({}, {}, {})",
            camera_position.x,
            camera_position.y,
            camera_position.z,
            glm::degrees(camera_yaw),
            glm::degrees(camera_pitch),
            camera_forward.x,
            camera_forward.y,
            camera_forward.z));
    WBE::stdout_log(WBE::WBE_CHANNEL_GLOBAL)
        ->message(std::format("Forward Sponza light: position=({}, {}, {}), yaw_deg={}, pitch_deg={}, direction=({}, {}, {})",
            spot_light_component.position.x,
            spot_light_component.position.y,
            spot_light_component.position.z,
            glm::degrees(spot_light_yaw),
            glm::degrees(spot_light_pitch),
            spot_light_component.direction.x,
            spot_light_component.direction.y,
            spot_light_component.direction.z));
}

void ApplicationVulkanDrawSponzaTest::setup_draw_task(const ForwardSceneViewConfig& p_view_config) {
    camera_position = p_view_config.camera_position;
    camera_yaw = p_view_config.camera_yaw;
    camera_pitch = p_view_config.camera_pitch;
    setup_render_context();
    load_resources();
    setup_static_scene_resources();
    setup_spot_light();
    spot_light_component.position = p_view_config.light_position;
    spot_light_yaw = p_view_config.light_yaw;
    spot_light_pitch = p_view_config.light_pitch;
    spot_light_component.direction = test_camera_forward(spot_light_yaw, spot_light_pitch);
    {
        glm::vec3 light_up =
            std::abs(spot_light_component.direction.y) > 0.99F ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
        spot_light_component.view =
            glm::lookAt(spot_light_component.position, spot_light_component.position + spot_light_component.direction, light_up);
    }
    create_shadow_map_resources();
    create_main_depth_resources();

    WBE::RTRunStaticSetVK::Config shadow_opaque_task_config;
    VkClearValue shadow_color_clear{};
    VkClearValue shadow_depth_clear{.depthStencil = {.depth = 0.0F, .stencil = 0}};
    shadow_opaque_task_config.set_render_extent(shadow_extent)
        .add_color_attachment(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, SHADOWMAP_COLOR_FORMAT)
        .set_depth_attachment(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, SHADOWMAP_DEPTH_FORMAT)
        .add_clear_value(shadow_color_clear)
        .add_clear_value(shadow_depth_clear);
    auto shadow_opaque_task = WBE::make_unique<WBE::RTRunStaticSetVK>(
        render_allocator, renderer_vk, sponza_opaque_set, shadow_opaque_pipeline, shadow_opaque_task_config);
    write_shadow_dynamic_descriptors(*shadow_opaque_task);

    WBE::RTRunStaticSetVK::Config shadow_masked_task_config;
    shadow_masked_task_config.set_render_extent(shadow_extent)
        .add_color_attachment(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, SHADOWMAP_COLOR_FORMAT)
        .set_depth_attachment(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, SHADOWMAP_DEPTH_FORMAT)
        .set_color_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
        .set_depth_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    auto shadow_masked_task = WBE::make_unique<WBE::RTRunStaticSetVK>(
        render_allocator, renderer_vk, sponza_masked_set, shadow_masked_pipeline, shadow_masked_task_config);
    write_shadow_dynamic_descriptors(*shadow_masked_task);

    WBE::RTRunStaticSetVK::Config main_opaque_task_config;
    main_opaque_task_config.set_depth_attachment(REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW, MAIN_DEPTH_FORMAT);
    auto main_opaque_task = WBE::make_unique<WBE::RTRunStaticSetVK>(
        render_allocator, renderer_vk, sponza_opaque_set, main_opaque_pipeline, main_opaque_task_config);
    write_main_dynamic_descriptors(*main_opaque_task);

    WBE::RTRunStaticSetVK::Config main_masked_task_config;
    main_masked_task_config.set_depth_attachment(REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW, MAIN_DEPTH_FORMAT)
        .set_color_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
        .set_depth_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    auto main_masked_task = WBE::make_unique<WBE::RTRunStaticSetVK>(
        render_allocator, renderer_vk, sponza_masked_set, main_masked_pipeline, main_masked_task_config);
    write_main_dynamic_descriptors(*main_masked_task);

    setup_light_billboard_overlay_resources();

    register_draw_sponza_render_graph(shadow_opaque_task, shadow_masked_task, main_opaque_task, main_masked_task);
}

void ApplicationVulkanDrawSponzaTest::setup_render_context() {
    renderer_vk = WBE::RendererVK::get_singleton();
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = get_renderer_allocator().get();
    camera_component = make_default_test_camera_component(base_render_data->swap_chain_aspect_ratio, 100.0F);
    camera_component.view = test_camera_view(camera_position, camera_yaw, camera_pitch);
}

void ApplicationVulkanDrawSponzaTest::load_resources() {
    auto* resource_manager = WBE::ResourceManager::get_singleton();
    auto resource_chunk = make_ref<WBE::ResourceChunkLoadFile>(
        WBE::global_allocator(), "res_chunks/rendering_test_resources.json", resource_manager->get_allocator());
    resource_manager->register_chunk(resource_chunk);
    sponza_opaque_set = resource_manager->get_resource<WBE::RUStaticOpaqueSet>("sponza.static_opaque_set");
    if (sponza_opaque_set == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: static opaque set resource was not found.");
    }
    sponza_masked_set = resource_manager->get_resource<WBE::RUStaticMaskedSet>("sponza.static_masked_set");
    if (sponza_masked_set == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: static masked set resource was not found.");
    }
    shadow_opaque_pipeline = resource_manager->get_resource<WBE::RUGraphicsPipeline>("shadowmap_spot_static");
    if (shadow_opaque_pipeline == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: shadow opaque graphics pipeline resource was not found.");
    }
    shadow_masked_pipeline = resource_manager->get_resource<WBE::RUGraphicsPipeline>("shadowmap_spot_masked_static");
    if (shadow_masked_pipeline == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: shadow masked graphics pipeline resource was not found.");
    }
    main_opaque_pipeline = resource_manager->get_resource<WBE::RUGraphicsPipeline>("forward_cook_torrance_pbr_static");
    if (main_opaque_pipeline == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: main opaque graphics pipeline resource was not found.");
    }
    main_masked_pipeline = resource_manager->get_resource<WBE::RUGraphicsPipeline>("forward_cook_torrance_pbr_masked_static");
    if (main_masked_pipeline == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: main masked graphics pipeline resource was not found.");
    }
}

void ApplicationVulkanDrawSponzaTest::setup_static_scene_resources() {
}

void ApplicationVulkanDrawSponzaTest::setup_spot_light() {
    spot_light_component = make_default_test_spot_light_component(128.0F);
    spot_light_yaw = std::atan2(spot_light_component.direction.z, spot_light_component.direction.x);
    spot_light_pitch = std::asin(std::clamp(spot_light_component.direction.y, -1.0F, 1.0F));
    shadow_extent = VkExtent2D{
        .width = spot_light_component.shadow_map_resolution,
        .height = spot_light_component.shadow_map_resolution,
    };
}

void ApplicationVulkanDrawSponzaTest::create_shadow_map_resources() {
    shadow_depth_image = WBE::ROImageVK::Builder().shadow_map_depth(spot_light_component).create_ref(base_render_data);
    shadow_color_image = WBE::ROImageVK::Builder().shadow_map_color(spot_light_component).create_ref(base_render_data);

    VkImageSubresourceRange depth_subresource{
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    VkImageSubresourceRange color_subresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    shadow_depth_view = WBE::ROImageViewVK::Builder()
                            .set_image(shadow_depth_image)
                            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                            .set_format(SHADOWMAP_DEPTH_FORMAT)
                            .set_subresource_range(depth_subresource)
                            .create_ref(base_render_data);
    shadow_color_view = WBE::ROImageViewVK::Builder()
                            .set_image(shadow_color_image)
                            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                            .set_format(SHADOWMAP_COLOR_FORMAT)
                            .set_subresource_range(color_subresource)
                            .create_ref(base_render_data);

    shadow_sampler = WBE::ROSamplerVK::Builder()
                         .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                         .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                         .create_ref(base_render_data);
}

void ApplicationVulkanDrawSponzaTest::create_main_depth_resources() {
    main_depth_image = WBE::ROImageVK::Builder()
                           .set_properties_2d(MAIN_DEPTH_FORMAT, base_render_data->swap_chain_extent, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                           .create_ref(base_render_data);
    VkImageSubresourceRange depth_subresource{
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    main_depth_view = WBE::ROImageViewVK::Builder()
                          .set_image(main_depth_image)
                          .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                          .set_format(MAIN_DEPTH_FORMAT)
                          .set_subresource_range(depth_subresource)
                          .create_ref(base_render_data);
}

void ApplicationVulkanDrawSponzaTest::setup_light_billboard_overlay_resources() {
    auto* resource_manager = WBE::ResourceManager::get_singleton();
    auto pipeline_resource = resource_manager->get_resource<WBE::RUGraphicsPipeline>("billboard_face_camera_pipeline");
    if (pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup light billboard overlay: graphics pipeline resource was not found.");
    }
    auto texture_resource = resource_manager->get_resource<WBE::RUTexture>("spot_light_billboard");
    if (texture_resource == nullptr) {
        throw std::runtime_error("Failed to setup light billboard overlay: texture resource was not found.");
    }

    if (camera_info_ubos == WBE::MEM_NULL) {
        camera_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(WBE::CameraInfoVKUBO));
    }
    BillboardInfoVKUBO light_billboard_info = make_light_billboard_info(spot_light_component);
    light_billboard_ubo = WBE::ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .initial_data(&light_billboard_info, sizeof(light_billboard_info))
                              .create_ref(base_render_data);
    light_billboard_sampler = WBE::ROSamplerVK::Builder()
                                  .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                                  .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                                  .create_ref(base_render_data);

    WBE::GraphicsPipelineCompilerVKContext compiler_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
    };
    WBE::GraphicsPipelineCompilerVK compiler(compiler_context);
    light_billboard_texture_view = compiler.compile_texture_view("spot_light_billboard", texture_resource);
    auto compiled = compiler.compile("light_billboard_pipeline", pipeline_resource);
    light_billboard_pipeline = compiled.graphics_pipeline;
}

void ApplicationVulkanDrawSponzaTest::write_shadow_dynamic_descriptors(WBE::RTRunStaticSetVK& p_task) {
    static_cast<void>(p_task);
    if (spot_light_info_ubos == WBE::MEM_NULL) {
        spot_light_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(WBE::SpotLightInfoVKUBO));
    }
    WBE::SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    spot_light_info_ubos->publish_buffer_update(&spot_light_info, 0, sizeof(spot_light_info));
    log_uniform_buffer_descriptors(spot_light_info_ubos, "forward sponza shadow light buffer");
}

void ApplicationVulkanDrawSponzaTest::write_main_dynamic_descriptors(WBE::RTRunStaticSetVK& p_task) {
    static_cast<void>(p_task);
    if (camera_info_ubos == WBE::MEM_NULL) {
        camera_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(WBE::CameraInfoVKUBO));
    }
    if (spot_light_count_ubos == WBE::MEM_NULL) {
        spot_light_count_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(uint32_t));
    }
    if (spot_light_info_ubos == WBE::MEM_NULL) {
        spot_light_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(WBE::SpotLightInfoVKUBO));
    }
    WBE::CameraInfoVKUBO camera_info = make_camera_info_ubo(camera_component);
    WBE::SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    uint32_t spot_light_count = 1;
    camera_info_ubos->publish_buffer_update(&camera_info, 0, sizeof(camera_info));
    spot_light_info_ubos->publish_buffer_update(&spot_light_info, 0, sizeof(spot_light_info));
    spot_light_count_ubos->publish_buffer_update(&spot_light_count, 0, sizeof(spot_light_count));

    log_uniform_buffer_descriptors(camera_info_ubos, "forward sponza camera buffer");
    log_uniform_buffer_descriptors(spot_light_info_ubos, "forward sponza main light buffer");
    log_uniform_buffer_descriptors(spot_light_count_ubos, "forward sponza light count buffer");
}

void ApplicationVulkanDrawSponzaTest::register_draw_sponza_render_graph(WBE::Unique<WBE::RTRunStaticSetVK>& p_shadow_opaque_task,
    WBE::Unique<WBE::RTRunStaticSetVK>& p_shadow_masked_task,
    WBE::Unique<WBE::RTRunStaticSetVK>& p_main_opaque_task,
    WBE::Unique<WBE::RTRunStaticSetVK>& p_main_masked_task) {
    WBE::RendGraphDescVK pipeline_desc(render_allocator);
    pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_SWAP_CHAIN_IMAGE),
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .container = WBE::MEM_NULL});
    pipeline_desc.add_resource(REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW, main_depth_view)
        .add_resource(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, shadow_color_view)
        .add_resource(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, shadow_depth_view)
        .add_resource("shadow_light_info", spot_light_info_ubos)
        .add_resource("camera_info", camera_info_ubos)
        .add_resource("spot_light_info", spot_light_info_ubos)
        .add_resource("spot_light_count", spot_light_count_ubos)
        .add_resource("shadowmap", shadow_depth_view)
        .add_resource("shadow_sampler", shadow_sampler)
        .add_resource("gbuffer_camera_info", camera_info_ubos)
        .add_resource("light_billboard_info", light_billboard_ubo)
        .add_resource("light_billboard_texture", light_billboard_texture_view)
        .add_resource("light_billboard_sampler", light_billboard_sampler);

    pipeline_desc.tasks.emplace_back(render_allocator, RT_RUN_SHADOWMAP_OPAQUE, std::move(p_shadow_opaque_task));

    WBE::RendTaskDescVK shadow_masked_task_desc(render_allocator, RT_RUN_SHADOWMAP_MASKED, std::move(p_shadow_masked_task));
    shadow_masked_task_desc.after(RT_RUN_SHADOWMAP_OPAQUE);
    pipeline_desc.tasks.emplace_back(std::move(shadow_masked_task_desc));

    WBE::RendTaskDescVK main_opaque_task_desc(render_allocator, RT_RUN_MAIN_OPAQUE, std::move(p_main_opaque_task));
    main_opaque_task_desc.after(RT_RUN_SHADOWMAP_MASKED).read(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, WBE::RendResUsageVK::SAMPLED_IMAGE);
    pipeline_desc.tasks.emplace_back(std::move(main_opaque_task_desc));

    WBE::RendTaskDescVK main_masked_task_desc(render_allocator, RT_RUN_MAIN_MASKED, std::move(p_main_masked_task));
    main_masked_task_desc.after(RT_RUN_MAIN_OPAQUE).read(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, WBE::RendResUsageVK::SAMPLED_IMAGE);
    pipeline_desc.tasks.emplace_back(std::move(main_masked_task_desc));

    WBE::RendTaskDescVK light_billboard_task_desc(render_allocator);
    light_billboard_task_desc.task_id = RT_RUN_LIGHT_BILLBOARD;
    light_billboard_task_desc.after(RT_RUN_MAIN_MASKED);
    light_billboard_task_desc.create = [render_allocator = render_allocator,
                                           base_render_data = base_render_data,
                                           light_billboard_pipeline = light_billboard_pipeline]() -> WBE::Unique<WBE::IRenderTaskVK> {
        return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(render_allocator,
            base_render_data,
            glm::vec4(0.0F),
            base_render_data->swap_chain_extent,
            WBE::REND_RES_SWAP_CHAIN_IMAGE,
            light_billboard_pipeline,
            std::optional<std::string_view>{},
            std::optional<std::string_view>{},
            std::optional<WBE::IndexedDrawRegionVK>{},
            VK_ATTACHMENT_LOAD_OP_LOAD,
            WBE::VertexInputDescriptionVK{},
            0,
            6);
    };
    pipeline_desc.tasks.emplace_back(std::move(light_billboard_task_desc));

    WBE::Ref<WBE::RenderGraphVK> graph = renderer_vk->register_render_graph("draw_sponza_pipeline", std::move(pipeline_desc));
    WBE::write_descriptor_debug_output("draw_sponza_pipeline_graph_descriptors", graph->list_descriptors());
    renderer_vk->activate_render_graph("draw_sponza_pipeline");
}
