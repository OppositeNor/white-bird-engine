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
#include "function/function_utils.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/mesh_compiler_vk.hh"
#include "function/render_vk/mesh_resources_vk.hh"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/trigonometric.hpp"
#include "global/global.hh"
#include "input_manager_mocked.hh"
#include "platform/file_system/path.hh"
#include "render_test.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <array>
#include <cstdlib>
#include <format>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float4.hpp>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

static constexpr std::string_view REND_RES_TEST_DRAW_CUBE_UNSHADED_CAMERA_INFO_BUFFER = "camera_info";

class ApplicationVulkanDrawCubeUnshadedTest : public WBE::ApplicationGame {
public:
    ApplicationVulkanDrawCubeUnshadedTest()
        : WBE::ApplicationGame({}, WBE::make_ref<WBE::InputManagerMocked>(WBE::global_allocator(), WBE::global_allocator())) {
    }

    virtual void logic_update(double /* p_delta */) override {
    }

    virtual void render_update(double p_delta) override {
        auto* renderer = WBE::RendererVK::get_singleton();
        WBE::on_exec_test_render(*renderer, renderer->get_base_render_data()->current_frame);
        camera_info.view = glm::rotate(camera_info.view, static_cast<float>(p_delta), glm::vec3(0.0F, 1.0F, 0.0F));
        camera_info_ubo->update_buffer(&camera_info, sizeof(CameraInfoGPU));
    }

    void setup_graphics_pipeline() {
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        const auto* base_render_data = renderer->get_base_render_data();
        auto* resource_manager = WBE::ResourceManager::get_singleton();

        auto resource_chunk = WBE::make_ref<WBE::ResourceChunkLoadFile>(
            WBE::global_allocator(), "assets/models/models.json", resource_manager->get_allocator());
        resource_manager->register_chunk(resource_chunk);
        WBE::RefRes<WBE::RUMesh3D> cube_mesh = resource_manager->get_resource<WBE::RUMesh3D>("model_cube_static.mesh");
        if (cube_mesh == nullptr) {
            throw std::runtime_error("Failed to setup draw cube unshaded test: static mesh resource was not found.");
        }

        aspect_ratio = base_render_data->swap_chain_aspect_ratio;
        camera_info = CameraInfoGPU{
            // Look at the origin from (2,2,2)
            .view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F)),
            .proj = glm::perspectiveRH_ZO(glm::radians(45.0F), aspect_ratio, 10.0F, 0.1F),
        };
        // Inverse the y value since Vulkan makes y axis points downwards.
        camera_info.proj[1][1] *= -1;
        auto pipeline_resource = resource_manager->get_resource<WBE::RUGraphicsPipeline>("draw_cube_unshaded_pipeline");
        if (pipeline_resource == nullptr) {
            throw std::runtime_error("Failed to setup draw cube unshaded test: static graphics pipeline resource was not found.");
        }

        WBE::Ref<WBE::MeshResourcesVK> static_mesh_resources =
            WBE::create_mesh_resources(renderer, cube_mesh, pipeline_resource, WBE::MEM_NULL, WBE::MEM_NULL);
        if (static_mesh_resources->acquire_textures() == WBE::MEM_NULL) {
            throw std::runtime_error("Failed to setup draw cube unshaded test: static-mesh texture list was not compiled.");
        }
        const std::array color_formats{base_render_data->swap_chain_format};
        WBE::Ref<WBE::ROGraphicsPipelineVK> graphics_pipeline =
            WBE::compile_render_test_graphics_pipeline(renderer, "draw_cube_unshaded_pipeline", pipeline_resource, color_formats);
        const std::array<VkClearValue, 1> clear_values{};
        const std::array<std::string_view, 1> color_attachments{WBE::REND_RES_SWAP_CHAIN};
        WBE::RendTaskDescVK task_desc = WBE::make_mesh_task_desc(render_allocator,
            base_render_data,
            "run_static_mesh",
            static_mesh_resources,
            graphics_pipeline,
            false,
            clear_values,
            &WBE::RendGraphContextVK::swap_chain_extent,
            color_attachments);
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        pipeline_desc.add_resource(WBE::STATIC_MESH_KEY, *static_mesh_resources);
        pipeline_desc.add_resource(WBE::REND_RES_SWAP_CHAIN, *renderer->acquire_swap_chain_image_view_container())
            .add_buffer(REND_RES_TEST_DRAW_CUBE_UNSHADED_CAMERA_INFO_BUFFER,
                [initial_data = WBE::to_initial_data(camera_info)](const WBE::RendGraphContextVK& /*p_context*/) {
                    return WBE::RendBufferDescVK{.usage = WBE::RendResPrimaryUsageVK::BUFFER_UNIFORM,
                        .domain = WBE::DeviceDomain::HOST_DEVICE,
                        .initial_data = initial_data};
                });
        pipeline_desc.tasks.emplace_back(std::move(task_desc));
        WBE::Ref<WBE::RenderGraphVK> graph =
            WBE::RendererVK::get_singleton()->register_render_graph("test_render_graph", std::move(pipeline_desc));
        camera_info_ubo =
            graph->acquire_resource(REND_RES_TEST_DRAW_CUBE_UNSHADED_CAMERA_INFO_BUFFER)->get().dynamic_cast_ref<WBE::ROBufferVK>();
        WBE::write_descriptor_debug_output("test_render_graph_graph_descriptors", graph->list_descriptors());
        WBE::RendererVK::get_singleton()->activate_render_graph("test_render_graph");
    }

private:
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    CameraInfoGPU camera_info{};
    float aspect_ratio = 1.0F;
};

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Path("test_env"));
    {
        ApplicationVulkanDrawCubeUnshadedTest game_application;
        {
            WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiler");
            game_application.setup_graphics_pipeline();
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
