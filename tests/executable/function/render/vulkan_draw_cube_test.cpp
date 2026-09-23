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
#include "core/memory/reference_raw.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/input_manager/input_manager.hh"
#include "function/input_manager/key_codes.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/rt_run_static_set_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "glm/geometric.hpp"
#include "global/global.hh"
#include "platform/file_system/path.hh"
#include "render_test.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

namespace {

constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOWMAP_COLOR_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_COLOR_FORMAT;
static constexpr std::string_view SHADOW_LIGHT_INFO_BUFFER = "shadow_light_info";
static constexpr std::string_view CAMERA_INFO_BUFFER = "camera_info";
static constexpr std::string_view SPOT_LIGHT_INFO_BUFFER = "spot_light_info";
static constexpr std::string_view SPOT_LIGHT_COUNT_BUFFER = "spot_light_count";
static constexpr std::string_view SHADOWMAP_SAMPLED_IMAGE_VIEW = "shadowmap";
static constexpr std::string_view SHADOW_SAMPLER = "shadow_sampler";

} // namespace

class ApplicationVulkanDrawCubeTest : public WBE::ApplicationGame {
public:
    virtual void logic_update(double p_delta) override;
    virtual void render_update(double p_delta) override;

    void setup_draw_task();

private:
    void setup_render_context();
    void load_resources();
    void setup_spot_light();
    void create_shadow_map_resources();
    void write_shadow_dynamic_descriptors(WBE::RTRunStaticSetVK& p_task);
    void write_main_dynamic_descriptors(WBE::RTRunStaticSetVK& p_task);
    void register_draw_cube_render_graph(WBE::Unique<WBE::RTRunStaticSetVK>& p_shadow_task, WBE::Unique<WBE::RTRunStaticSetVK>& p_main_task);

    WBE::RendererVK* renderer_vk = nullptr;
    const WBE::BaseRenderDataVK* base_render_data = nullptr;
    WBE::RenderAllocator* render_allocator = nullptr;
    WBE::RefRes<WBE::RUStaticOpaqueSet> cube_set;
    WBE::RefRes<WBE::RUGraphicsPipeline> shadow_pipeline_resource;
    WBE::RefRes<WBE::RUGraphicsPipeline> main_pipeline_resource;
    WBE::Ref<WBE::ROImageVK> shadow_depth_image;
    WBE::Ref<WBE::ROImageVK> shadow_color_image;
    WBE::Ref<WBE::ROImageViewVK> shadow_depth_view;
    WBE::Ref<WBE::ROImageViewVK> shadow_color_view;
    WBE::Ref<WBE::ROSamplerVK> shadow_sampler;
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    WBE::Ref<WBE::ROBufferVK> spot_light_info_ubo;
    WBE::Ref<WBE::ROBufferVK> spot_light_count_ubo;
    WBE::ComponentSpotLight spot_light_component{};
    VkExtent2D shadow_extent{};
    WBE::ComponentCamera camera_component{};
    glm::vec3 camera_position = TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = default_test_camera_yaw();
    float camera_pitch = default_test_camera_pitch();
};

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Path("test_env"));
    {
        ApplicationVulkanDrawCubeTest game_application;
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

void ApplicationVulkanDrawCubeTest::logic_update(double p_delta) {
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

    camera_component.view = test_camera_view(camera_position, camera_yaw, camera_pitch);

    WBE::ROBufferVK* camera_info_buffer = camera_info_ubo.get();
    if (camera_info_buffer == nullptr) {
        return;
    }
    WBE::CameraInfoVKUBO camera_info = make_camera_info_ubo(camera_component);
    add_render_job([camera_info, camera_info_buffer]() {
        camera_info_buffer->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
    });
}

void ApplicationVulkanDrawCubeTest::render_update(double /* p_delta */) {
    WBE::on_exec_test_render(*renderer_vk, base_render_data->current_frame);
}

void ApplicationVulkanDrawCubeTest::setup_draw_task() {
    setup_render_context();
    load_resources();
    setup_spot_light();
    create_shadow_map_resources();

    WBE::RTRunStaticSetVK::Config shadow_task_config;
    VkClearValue shadow_color_clear{};
    VkClearValue shadow_depth_clear{.depthStencil = {.depth = 0.0F, .stencil = 0}};
    shadow_task_config.set_render_extent(shadow_extent)
        .add_color_attachment(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, SHADOWMAP_COLOR_FORMAT)
        .set_depth_attachment(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, SHADOWMAP_DEPTH_FORMAT)
        .add_clear_value(shadow_color_clear)
        .add_clear_value(shadow_depth_clear);
    auto shadow_task = WBE::make_unique<WBE::RTRunStaticSetVK>(
        render_allocator, renderer_vk, cube_set, shadow_pipeline_resource, shadow_task_config);
    write_shadow_dynamic_descriptors(*shadow_task);

    WBE::RTRunStaticSetVK::Config main_task_config;
    main_task_config.add_color_attachment(WBE::REND_RES_SWAP_CHAIN, base_render_data->swap_chain_format);
    auto main_task =
        WBE::make_unique<WBE::RTRunStaticSetVK>(render_allocator, renderer_vk, cube_set, main_pipeline_resource, main_task_config);
    write_main_dynamic_descriptors(*main_task);

    register_draw_cube_render_graph(shadow_task, main_task);
}

void ApplicationVulkanDrawCubeTest::setup_render_context() {
    renderer_vk = WBE::RendererVK::get_singleton();
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = get_renderer_allocator().get();
    camera_component = make_default_test_camera_component(base_render_data->swap_chain_aspect_ratio);
}

void ApplicationVulkanDrawCubeTest::load_resources() {
    auto* resource_manager = WBE::ResourceManager::get_singleton();
    auto resource_chunk = WBE::make_ref<WBE::ResourceChunkLoadFile>(
        WBE::global_allocator(), "assets/models/models.json", resource_manager->get_allocator());
    resource_manager->register_chunk(resource_chunk);

    cube_set = resource_manager->get_resource<WBE::RUStaticOpaqueSet>("model_cube_static.static_opaque_set");
    if (cube_set == nullptr) {
        throw std::runtime_error("Failed to setup draw cube test: static opaque set resource was not found.");
    }
    shadow_pipeline_resource = resource_manager->get_resource<WBE::RUGraphicsPipeline>("shadowmap_spot_static");
    if (shadow_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup draw cube test: static shadow graphics pipeline resource not found.");
    }
    main_pipeline_resource = resource_manager->get_resource<WBE::RUGraphicsPipeline>("forward_cook_torrance_pbr_static");
    if (main_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup draw cube test: static main graphics pipeline resource not found.");
    }
}

void ApplicationVulkanDrawCubeTest::setup_spot_light() {
    spot_light_component = make_default_test_spot_light_component(32.0F);
    shadow_extent = VkExtent2D{
        .width = spot_light_component.shadow_map_resolution,
        .height = spot_light_component.shadow_map_resolution,
    };
}

void ApplicationVulkanDrawCubeTest::create_shadow_map_resources() {

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

void ApplicationVulkanDrawCubeTest::write_shadow_dynamic_descriptors(WBE::RTRunStaticSetVK& p_task) {
    static_cast<void>(p_task);
    WBE::SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    spot_light_info_ubo = WBE::ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .initial_data(&spot_light_info, sizeof(spot_light_info))
                              .create_ref(base_render_data);
}

void ApplicationVulkanDrawCubeTest::write_main_dynamic_descriptors(WBE::RTRunStaticSetVK& p_task) {
    static_cast<void>(p_task);
    WBE::CameraInfoVKUBO camera_info = make_camera_info_ubo(camera_component);
    uint32_t spot_light_count = 1;
    camera_info_ubo = WBE::ROBufferVK::Builder()
                          .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                          .initial_data(&camera_info, sizeof(camera_info))
                          .create_ref(base_render_data);
    spot_light_count_ubo = WBE::ROBufferVK::Builder()
                               .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                               .initial_data(&spot_light_count, sizeof(spot_light_count))
                               .create_ref(base_render_data);
}

void ApplicationVulkanDrawCubeTest::register_draw_cube_render_graph(
    WBE::Unique<WBE::RTRunStaticSetVK>& p_shadow_task, WBE::Unique<WBE::RTRunStaticSetVK>& p_main_task) {
    WBE::RendGraphDescVK pipeline_desc(render_allocator);
    pipeline_desc.add_resource(WBE::REND_RES_SWAP_CHAIN, renderer_vk->acquire_swap_chain_image_view_container())
        .add_resource(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, shadow_color_view)
        .add_resource(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, shadow_depth_view)
        .add_resource(SHADOW_LIGHT_INFO_BUFFER, spot_light_info_ubo)
        .add_resource(CAMERA_INFO_BUFFER, camera_info_ubo)
        .add_resource(SPOT_LIGHT_INFO_BUFFER, spot_light_info_ubo)
        .add_resource(SPOT_LIGHT_COUNT_BUFFER, spot_light_count_ubo)
        .add_resource(SHADOWMAP_SAMPLED_IMAGE_VIEW, shadow_depth_view)
        .add_resource(SHADOW_SAMPLER, shadow_sampler);

    pipeline_desc.tasks.emplace_back(render_allocator, RT_RUN_PIPELINE_SHADOWMAP, std::move(p_shadow_task));

    WBE::RendTaskDescVK main_task_desc(render_allocator, RT_RUN_PIPELINE, std::move(p_main_task));
    main_task_desc.after(RT_RUN_PIPELINE_SHADOWMAP);
    pipeline_desc.tasks.emplace_back(std::move(main_task_desc));

    WBE::Ref<WBE::RenderGraphVK> graph = renderer_vk->register_render_graph("draw_cube_pipeline", std::move(pipeline_desc));
    WBE::write_descriptor_debug_output("draw_cube_pipeline_graph_descriptors", graph->list_descriptors());
    renderer_vk->activate_render_graph("draw_cube_pipeline");
}
