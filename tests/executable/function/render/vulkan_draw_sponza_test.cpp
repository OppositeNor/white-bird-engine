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
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/rt_run_static_set_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
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
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <memory>
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

} // namespace

class ApplicationVulkanDrawSponzaTest : public WBE::ApplicationGame {
public:
    virtual void logic_update(double p_delta) override;
    virtual void render_update(double p_delta) override;

    void setup_draw_task();

private:
    void setup_render_context();
    void load_resources();
    void setup_static_scene_resources();
    void setup_spot_light();
    void create_shadow_map_resources();
    void create_main_depth_resources();
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
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    WBE::Ref<WBE::ROBufferVK> spot_light_info_ubo;
    WBE::Ref<WBE::ROBufferVK> spot_light_count_ubo;
    WBE::ComponentSpotLight spot_light_component{};
    float spot_light_yaw = 0.0F;
    float spot_light_pitch = 0.0F;
    VkExtent2D shadow_extent{};
    WBE::ComponentCamera camera_component{};
    glm::vec3 camera_position = TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = default_test_camera_yaw();
    float camera_pitch = default_test_camera_pitch();
};

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Directory({"test_env"}));
    {
        ApplicationVulkanDrawSponzaTest game_application;
        {
            WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiler");
            game_application.setup_draw_task();
            game_application.run();
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

    WBE::CameraInfoVKUBO camera_info = make_camera_info_ubo(camera_component);
    WBE::SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    add_render_job([this, camera_info, spot_light_info]() {
        camera_info_ubo->update_buffer(&camera_info, sizeof(camera_info));
        spot_light_info_ubo->update_buffer(&spot_light_info, sizeof(spot_light_info));
    });
}

void ApplicationVulkanDrawSponzaTest::render_update(double /* p_delta */) {
    WBE::on_exec_test_render(*renderer_vk, base_render_data->current_frame);
}

void ApplicationVulkanDrawSponzaTest::setup_draw_task() {
    setup_render_context();
    load_resources();
    setup_static_scene_resources();
    setup_spot_light();
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

    register_draw_sponza_render_graph(shadow_opaque_task, shadow_masked_task, main_opaque_task, main_masked_task);
}

void ApplicationVulkanDrawSponzaTest::setup_render_context() {
    renderer_vk = WBE::RendererVK::get_singleton();
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = get_renderer_allocator().get();
    camera_component = make_default_test_camera_component(base_render_data->swap_chain_aspect_ratio, 100.0F);
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
                           .set_format(MAIN_DEPTH_FORMAT)
                           .image_2d(base_render_data->swap_chain_extent)
                           .set_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
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

void ApplicationVulkanDrawSponzaTest::write_shadow_dynamic_descriptors(WBE::RTRunStaticSetVK& p_task) {
    spot_light_info_ubo = WBE::ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                              .set_size(sizeof(WBE::SpotLightInfoVKUBO))
                              .create_ref(base_render_data);
    WBE::SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    spot_light_info_ubo->update_buffer(&spot_light_info, sizeof(spot_light_info));
    p_task.set_descriptor_binding("light_camera_info", spot_light_info_ubo);
}

void ApplicationVulkanDrawSponzaTest::write_main_dynamic_descriptors(WBE::RTRunStaticSetVK& p_task) {
    camera_info_ubo = WBE::ROBufferVK::Builder()
                          .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                          .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                          .set_size(sizeof(WBE::CameraInfoVKUBO))
                          .create_ref(base_render_data);
    spot_light_count_ubo = WBE::ROBufferVK::Builder()
                               .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                               .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                               .set_size(sizeof(uint32_t))
                               .create_ref(base_render_data);
    WBE::CameraInfoVKUBO camera_info = make_camera_info_ubo(camera_component);
    uint32_t spot_light_count = 1;
    camera_info_ubo->update_buffer(&camera_info, sizeof(camera_info));
    spot_light_count_ubo->update_buffer(&spot_light_count, sizeof(spot_light_count));

    auto write_uniform_buffer = [&p_task](const char* p_name, WBE::Ref<WBE::ROBufferVK> p_buffer) {
        p_task.set_descriptor_binding(p_name, p_buffer);
    };
    write_uniform_buffer("camera_info", camera_info_ubo);
    write_uniform_buffer("light.lights.info", spot_light_info_ubo);
    write_uniform_buffer("light.light_count", spot_light_count_ubo);
    p_task.set_descriptor_binding("light.lights.shadowmap", shadow_depth_view);
    p_task.set_descriptor_binding("light.lights.shadowmap_sampler", shadow_sampler);
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
        .add_resource(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, shadow_depth_view);

    pipeline_desc.tasks.emplace_back(render_allocator, RT_RUN_SHADOWMAP_OPAQUE, std::move(p_shadow_opaque_task));

    WBE::RendTaskDescVK shadow_masked_task_desc(render_allocator, RT_RUN_SHADOWMAP_MASKED, std::move(p_shadow_masked_task));
    shadow_masked_task_desc.after(RT_RUN_SHADOWMAP_OPAQUE);
    pipeline_desc.tasks.emplace_back(std::move(shadow_masked_task_desc));

    WBE::RendTaskDescVK main_opaque_task_desc(render_allocator, RT_RUN_MAIN_OPAQUE, std::move(p_main_opaque_task));
    main_opaque_task_desc.after(RT_RUN_SHADOWMAP_MASKED);
    pipeline_desc.tasks.emplace_back(std::move(main_opaque_task_desc));

    WBE::RendTaskDescVK main_masked_task_desc(render_allocator, RT_RUN_MAIN_MASKED, std::move(p_main_masked_task));
    main_masked_task_desc.after(RT_RUN_MAIN_OPAQUE);
    pipeline_desc.tasks.emplace_back(std::move(main_masked_task_desc));

    WBE::Ref<WBE::RenderGraphVK> graph = renderer_vk->register_render_graph("draw_sponza_pipeline", std::move(pipeline_desc));
    WBE::write_descriptor_debug_output("draw_sponza_pipeline_graph_descriptors", graph->list_descriptors());
    renderer_vk->activate_render_graph("draw_sponza_pipeline");
}
