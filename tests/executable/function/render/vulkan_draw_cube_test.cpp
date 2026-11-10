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
#include "core/engine_core.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/function_utils.hh"
#include "function/input_manager/i_input_manager.hh"
#include "function/input_manager/key_codes.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/rend_graph_script_vk.hh"
#include "function/render_vk/rend_recipe_context_vk.hh"
#include "function/render_vk/render_graph_bindings_function_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/vulkan_enums_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "glm/geometric.hpp"
#include "global/global.hh"
#include "input_manager_mocked.hh"
#include "platform/file_system/path.hh"
#include "render_test.hh"
#include "shadowed_mesh_recipe_context.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <cstddef>
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
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

namespace {

constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOWMAP_COLOR_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_COLOR_FORMAT;
constexpr std::string_view SHADOW_LIGHT_INFO_BUFFER = "shadow_light_info";
constexpr std::string_view CAMERA_INFO_BUFFER = "camera_info";
constexpr std::string_view SPOT_LIGHT_INFO_BUFFER = "spot_light_info";
constexpr std::string_view SPOT_LIGHT_COUNT_BUFFER = "spot_light_count";
// The render graph definition, whose recipe is render_graphs.draw_cube.
constexpr std::string_view DRAW_CUBE_RENDER_GRAPH = "draw_cube_graph";

} // namespace

class ApplicationVulkanDrawCubeTest : public WBE::ApplicationGame {
public:
    ApplicationVulkanDrawCubeTest()
        : WBE::ApplicationGame({}, WBE::make_ref<WBE::InputManagerMocked>(WBE::global_allocator(), WBE::global_allocator())) {
    }

    virtual void logic_update(double p_delta) override;
    virtual void render_update(double p_delta) override;

    void setup_draw_task();
    void release_draw_task();

private:
    void setup_render_context();
    void load_resources();
    void setup_spot_light();
    void create_buffers();
    WBE::Ref<WBE::ROBufferVK> create_uniform_buffer(const void* p_data, size_t p_size) const;
    void acquire_draw_cube_render_graph();

    WBE::RendererVK* renderer_vk = nullptr;
    const WBE::RenderContextVK* render_context = nullptr;
    WBE::RenderAllocator* render_allocator = nullptr;
    WBE::Ref<WBE::RenderGraphVK> render_graph;
    // The uniform buffers the recipe imports. The camera buffer is updated from the logic thread through render jobs.
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    WBE::Ref<WBE::ROBufferVK> spot_light_info_ubo;
    WBE::Ref<WBE::ROBufferVK> shadow_light_info_ubo;
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
            game_application.release_draw_task();
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
    WBE::IInputManager* input_manager = WBE::IInputManager::get_singleton();
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
    CameraInfoGPU camera_info = make_camera_info_gpu(camera_component);
    add_render_job([camera_info, camera_info_buffer] {
        camera_info_buffer->update_buffer(&camera_info, sizeof(CameraInfoGPU));
    });
}

void ApplicationVulkanDrawCubeTest::render_update(double /* p_delta */) {
    WBE::on_exec_test_render(*renderer_vk, render_context->current_frame);
}

void ApplicationVulkanDrawCubeTest::setup_draw_task() {
    setup_render_context();
    load_resources();
    setup_spot_light();
    create_buffers();
    acquire_draw_cube_render_graph();
}

void ApplicationVulkanDrawCubeTest::release_draw_task() {
    WBE::release_test_render_graph(*renderer_vk, render_graph);
}

void ApplicationVulkanDrawCubeTest::setup_render_context() {
    renderer_vk = WBE::RendererVK::get_singleton();
    render_context = renderer_vk->get_render_context();
    render_allocator = get_renderer_allocator().get();
    camera_component = make_default_test_camera_component(render_context->swap_chain_aspect_ratio);
}

void ApplicationVulkanDrawCubeTest::load_resources() {
    auto* resource_manager = WBE::ResourceManager::get_singleton();
    auto resource_chunk = WBE::make_ref<WBE::ResourceChunkLoadFile>(
        WBE::global_allocator(), "assets/models/models.json", resource_manager->get_allocator());
    resource_manager->register_chunk(resource_chunk);
    if (resource_manager->get_resource<WBE::RUMesh3D>("model_cube_static.mesh") == WBE::MEM_NULL) {
        throw std::runtime_error("Failed to setup draw cube test: static mesh resource was not found.");
    }
}

void ApplicationVulkanDrawCubeTest::setup_spot_light() {
    spot_light_component = make_default_test_spot_light_component(32.0F);
    shadow_extent = VkExtent2D{
        .width = spot_light_component.shadow_map_resolution,
        .height = spot_light_component.shadow_map_resolution,
    };
}

WBE::Ref<WBE::ROBufferVK> ApplicationVulkanDrawCubeTest::create_uniform_buffer(const void* p_data, size_t p_size) const {
    return render_context->create_buffer(WBE::ROBufferVK::Builder()
            .initial_data(p_data, p_size, WBE::DeviceDomain::HOST_DEVICE)
            .set_size(p_size)
            .set_domain(WBE::DeviceDomain::HOST_DEVICE)
            .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
}

void ApplicationVulkanDrawCubeTest::create_buffers() {
    // Recipes cannot set initial data, so the application owns the buffers and the recipe imports them.
    const SpotLightInfoGPU spot_light_info = make_spot_light_info_gpu(spot_light_component);
    const CameraInfoGPU camera_info = make_camera_info_gpu(camera_component);
    const uint32_t spot_light_count = 1;
    camera_info_ubo = create_uniform_buffer(&camera_info, sizeof(camera_info));
    spot_light_info_ubo = create_uniform_buffer(&spot_light_info, sizeof(spot_light_info));
    shadow_light_info_ubo = create_uniform_buffer(&spot_light_info, sizeof(spot_light_info));
    spot_light_count_ubo = create_uniform_buffer(&spot_light_count, sizeof(spot_light_count));
}

void ApplicationVulkanDrawCubeTest::acquire_draw_cube_render_graph() {
    // The recipe registers the mesh and the pipelines and declares the shadow map; the application supplies its buffers.
    render_graph = WBE::acquire_test_render_graph(*renderer_vk,
        DRAW_CUBE_RENDER_GRAPH,
        WBE::make_ref<WBE::RenderGraphBindingsFunctionVK>(WBE::global_allocator(),
            DRAW_CUBE_RENDER_GRAPH,
            [this](const WBE::RenderContextVK& /*p_render_context*/, const WBE::RendRecipeContextVK& p_recipe_context) {
                WBE::ShadowedMeshRecipeContext context;
                static_cast<WBE::RendRecipeContextVK&>(context) = p_recipe_context;
                context.shadow_extent = {.width = shadow_extent.width, .height = shadow_extent.height};
                context.shadow_color_format = static_cast<WBE::ImageFormatVK>(SHADOWMAP_COLOR_FORMAT);
                context.shadow_depth_format = static_cast<WBE::ImageFormatVK>(SHADOWMAP_DEPTH_FORMAT);
                WBE::RendGraphPreparedBindingsVK result;
                result.context = WBE::RendGraphScriptVK::serialize_context(context);
                result.imports.add(CAMERA_INFO_BUFFER, *camera_info_ubo)
                    .add(SPOT_LIGHT_INFO_BUFFER, *spot_light_info_ubo)
                    .add(SHADOW_LIGHT_INFO_BUFFER, *shadow_light_info_ubo)
                    .add(SPOT_LIGHT_COUNT_BUFFER, *spot_light_count_ubo);
                return result;
            }));
}
