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
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/function_utils.hh"
#include "function/input_manager/input_manager.hh"
#include "function/input_manager/key_codes.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/rt_run_static_opaque_triangles_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <array>
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

namespace WhiteBirdEngine {
constexpr std::string_view REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW = "REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW";

} // namespace WhiteBirdEngine

namespace WBE = WhiteBirdEngine;

namespace {

constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOWMAP_COLOR_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_COLOR_FORMAT;
constexpr VkFormat MAIN_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

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
    void write_shadow_dynamic_descriptors(WBE::RTRunStaticOpaqueTrianglesVK& p_task);
    void write_main_dynamic_descriptors(WBE::RTRunStaticOpaqueTrianglesVK& p_task);
    void register_draw_sponza_render_graph(WBE::Unique<WBE::RTRunStaticOpaqueTrianglesVK>& p_shadow_task,
        WBE::Unique<WBE::RTRunStaticOpaqueTrianglesVK>& p_main_task);

    WBE::RendererVK* renderer_vk = nullptr;
    const WBE::BaseRenderDataVK* base_render_data = nullptr;
    WBE::RenderAllocator* render_allocator = nullptr;
    WBE::RenderDatabase* render_database = nullptr;
    WBE::RefRes<WBE::RUMesh3D> sponza_mesh;
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
    std::vector<std::pair<WBE::Ref<WBE::RODescriptorSetVK>, uint32_t>> camera_descriptor_bindings;
    WBE::ComponentSpotLight spot_light_component{};
    VkExtent2D shadow_extent{};
    WBE::ComponentCamera camera_component{};
    glm::vec3 camera_position = WBE::TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = WBE::default_test_camera_yaw();
    float camera_pitch = WBE::default_test_camera_pitch();
};

int main(int p_argc, char* p_argv[]) {
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

void ApplicationVulkanDrawSponzaTest::logic_update(double p_delta) {
    WBE::InputManager* input_manager = WBE::InputManager::get_singleton();
    if (input_manager == nullptr) {
        return;
    }

    float delta = static_cast<float>(p_delta);
    if (input_manager->get_input_status(WBE::KEYCODE_LEFT).is_down()) {
        camera_yaw -= WBE::TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_RIGHT).is_down()) {
        camera_yaw += WBE::TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_UP).is_down()) {
        camera_pitch += WBE::TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_DOWN).is_down()) {
        camera_pitch -= WBE::TEST_CAMERA_TURN_SPEED * delta;
    }
    camera_pitch = std::clamp(camera_pitch, -WBE::TEST_CAMERA_PITCH_LIMIT, WBE::TEST_CAMERA_PITCH_LIMIT);

    glm::vec3 forward = WBE::test_camera_forward(camera_yaw, camera_pitch);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    float move_delta = WBE::TEST_CAMERA_MOVE_SPEED * delta;

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

    camera_component.view = WBE::test_camera_view(camera_position, camera_yaw, camera_pitch);

    WBE::CameraInfoVKUBO camera_info = WBE::make_camera_info_ubo(camera_component);
    add_render_job([this, camera_info]() {
        WBE::Ref<WBE::ROBufferVK> new_camera_info_ubo = WBE::ROBufferVK::Builder()
                                                            .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                                            .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                                                            .set_size(sizeof(WBE::CameraInfoVKUBO))
                                                            .create_ref(base_render_data);
        new_camera_info_ubo->update_buffer(&camera_info, sizeof(camera_info));
        for (auto& [descriptor_set, binding] : camera_descriptor_bindings) {
            descriptor_set->write_descriptor_set(WBE::RODescriptorSetVK::Writer().set_binding(binding).add_uniform_buffer_info(
                new_camera_info_ubo, 0, sizeof(WBE::CameraInfoVKUBO)));
        }
        camera_info_ubo = std::move(new_camera_info_ubo);
    });
}

void ApplicationVulkanDrawSponzaTest::render_update(double /* p_delta */) {
}

void ApplicationVulkanDrawSponzaTest::setup_draw_task() {
    setup_render_context();
    load_resources();
    setup_static_scene_resources();
    setup_spot_light();
    create_shadow_map_resources();
    create_main_depth_resources();
    WBE::RTRunStaticOpaqueTrianglesVK::Config shadow_task_config;
    VkClearValue shadow_color_clear{};
    VkClearValue shadow_depth_clear{.depthStencil = {.depth = 1.0F, .stencil = 0}};
    shadow_task_config.set_render_extent(shadow_extent)
        .add_color_attachment(WBE::REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, SHADOWMAP_COLOR_FORMAT)
        .set_depth_attachment(WBE::REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, SHADOWMAP_DEPTH_FORMAT)
        .add_clear_value(shadow_color_clear)
        .add_clear_value(shadow_depth_clear)
        .add_graphics_pipeline_override("forward_cook_torrance_pbr", "shadowmap_spot")
        .add_graphics_pipeline_override("forward_cook_torrance_pbr_masked", "shadowmap_spot_masked");
    auto shadow_task = WBE::make_unique<WBE::RTRunStaticOpaqueTrianglesVK>(render_allocator, renderer_vk, shadow_task_config);
    write_shadow_dynamic_descriptors(*shadow_task);
    WBE::RTRunStaticOpaqueTrianglesVK::Config task_config;
    task_config.set_depth_attachment(WBE::REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW, MAIN_DEPTH_FORMAT);
    auto main_task = WBE::make_unique<WBE::RTRunStaticOpaqueTrianglesVK>(render_allocator, renderer_vk, task_config);
    write_main_dynamic_descriptors(*main_task);
    register_draw_sponza_render_graph(shadow_task, main_task);
}

void ApplicationVulkanDrawSponzaTest::setup_render_context() {
    renderer_vk = WBE::RendererVK::get_singleton();
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = get_renderer_allocator().get();
    render_database = WBE::RenderDatabase::get_singleton();
    camera_component = WBE::make_default_test_camera_component(base_render_data->swap_chain_aspect_ratio, 100.0F);
}

void ApplicationVulkanDrawSponzaTest::load_resources() {
    auto* resource_manager = WBE::ResourceManager::get_singleton();
    sponza_mesh = resource_manager->get_resource<WBE::RUMesh3D>("sponza.mesh");
    if (sponza_mesh == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: sponza mesh resource was not found.");
    }
}

void ApplicationVulkanDrawSponzaTest::setup_static_scene_resources() {
    render_database->set_static_opaque_triangles(sponza_mesh.get());
    renderer_vk->update_static_opaque_triangles();
}

void ApplicationVulkanDrawSponzaTest::setup_spot_light() {
    spot_light_component = WBE::make_default_test_spot_light_component(128.0F);
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

void ApplicationVulkanDrawSponzaTest::write_shadow_dynamic_descriptors(WBE::RTRunStaticOpaqueTrianglesVK& p_task) {
    spot_light_info_ubo = WBE::ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                              .set_size(sizeof(WBE::SpotLightInfoVKUBO))
                              .create_ref(base_render_data);
    WBE::SpotLightInfoVKUBO spot_light_info = WBE::make_spot_light_info_ubo(spot_light_component);
    spot_light_info_ubo->update_buffer(&spot_light_info, sizeof(spot_light_info));
    for (size_t batch_index = 0; batch_index < p_task.get_compiled_batch_count(); ++batch_index) {
        auto [set, binding] = p_task.get_descriptor_binding(batch_index, "light_camera_info");
        p_task.get_descriptor_set(batch_index, set)
            ->write_descriptor_set(WBE::RODescriptorSetVK::Writer().set_binding(binding).add_uniform_buffer_info(
                spot_light_info_ubo, 0, sizeof(WBE::CameraInfoVKUBO)));
    }
}

void ApplicationVulkanDrawSponzaTest::write_main_dynamic_descriptors(WBE::RTRunStaticOpaqueTrianglesVK& p_task) {
    if (p_task.get_compiled_batch_count() == 0) {
        throw std::runtime_error("Failed to setup draw sponza test: main pipeline produced no descriptor sets.");
    }
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
    WBE::CameraInfoVKUBO camera_info = WBE::make_camera_info_ubo(camera_component);
    uint32_t spot_light_count = 1;
    camera_info_ubo->update_buffer(&camera_info, sizeof(camera_info));
    spot_light_count_ubo->update_buffer(&spot_light_count, sizeof(spot_light_count));

    for (size_t batch_index = 0; batch_index < p_task.get_compiled_batch_count(); ++batch_index) {
        auto write_uniform_buffer = [&p_task, batch_index](const char* p_name, WBE::Ref<WBE::ROBufferVK> p_buffer, size_t p_size) {
            auto [set, binding] = p_task.get_descriptor_binding(batch_index, p_name);
            p_task.get_descriptor_set(batch_index, set)
                ->write_descriptor_set(
                    WBE::RODescriptorSetVK::Writer().set_binding(binding).add_uniform_buffer_info(p_buffer, 0, p_size));
        };
        write_uniform_buffer("camera_info", camera_info_ubo, sizeof(WBE::CameraInfoVKUBO));
        auto [camera_set, camera_binding] = p_task.get_descriptor_binding(batch_index, "camera_info");
        camera_descriptor_bindings.emplace_back(p_task.get_descriptor_set(batch_index, camera_set), camera_binding);
        write_uniform_buffer("light.lights.info", spot_light_info_ubo, sizeof(WBE::SpotLightInfoVKUBO));
        write_uniform_buffer("light.light_count", spot_light_count_ubo, sizeof(uint32_t));

        auto [shadowmap_set, shadowmap_binding] = p_task.get_descriptor_binding(batch_index, "light.lights.shadowmap");
        p_task.get_descriptor_set(batch_index, shadowmap_set)
            ->write_descriptor_set(WBE::RODescriptorSetVK::Writer()
                    .set_binding(shadowmap_binding)
                    .set_dst_array_element(0)
                    .add_sampled_image_info(WBE::RODescriptorSetVK::Writer::ImageInfo{
                        .image_view = shadow_depth_view,
                        .image_layout = VK_IMAGE_LAYOUT_GENERAL,
                    }));
        auto [shadow_sampler_set, shadow_sampler_binding] = p_task.get_descriptor_binding(batch_index, "light.lights.shadowmap_sampler");
        p_task.get_descriptor_set(batch_index, shadow_sampler_set)
            ->write_descriptor_set(WBE::RODescriptorSetVK::Writer()
                    .set_binding(shadow_sampler_binding)
                    .set_dst_array_element(0)
                    .add_sampler_info(WBE::RODescriptorSetVK::Writer::ImageInfo{.sampler = shadow_sampler}));
    }
}

void ApplicationVulkanDrawSponzaTest::register_draw_sponza_render_graph(
    WBE::Unique<WBE::RTRunStaticOpaqueTrianglesVK>& p_shadow_task, WBE::Unique<WBE::RTRunStaticOpaqueTrianglesVK>& p_main_task) {
    WBE::RendGraphDescVK pipeline_desc(render_allocator);
    pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK),
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .res = WBE::MEM_NULL});
    pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW),
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .res = main_depth_view});
    pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW),
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .res = shadow_color_view});
    pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW),
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .res = shadow_depth_view});
    pipeline_desc.tasks.emplace_back(render_allocator, WBE::RT_RUN_PIPELINE_SHADOWMAP, std::move(p_shadow_task));
    WBE::RendTaskDescVK main_task_desc(render_allocator, WBE::RT_RUN_STATIC_OPAQUE_TRIANGLES_INDIRECT_VK, std::move(p_main_task));
    main_task_desc.prev.emplace_back(WBE::RT_RUN_PIPELINE_SHADOWMAP);
    pipeline_desc.tasks.emplace_back(std::move(main_task_desc));
    renderer_vk->register_render_graph("draw_sponza_pipeline", std::move(pipeline_desc));
    renderer_vk->activate_render_graph("draw_sponza_pipeline");
}
