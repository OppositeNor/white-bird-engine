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
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/rt_run_static_set_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/trigonometric.hpp"
#include "global/global.hh"
#include "platform/file_system/path.hh"
#include "render_test.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstdlib>
#include <format>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float4.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

static constexpr std::string_view REND_RES_TEST_DRAW_CUBE_UNSHADED_CAMERA_INFO_BUFFER = "camera_info";

class ApplicationVulkanDrawCubeUnshadedTest : public WBE::ApplicationGame {
public:
    virtual void logic_update(double /* p_delta */) override {
    }

    virtual void render_update(double p_delta) override {
        auto* renderer = WBE::RendererVK::get_singleton();
        WBE::on_exec_test_render(*renderer, renderer->get_base_render_data()->current_frame);
        camera_info.view = glm::rotate(camera_info.view, static_cast<float>(p_delta), glm::vec3(0.0F, 1.0F, 0.0F));
        camera_info_ubo->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
    }

    void setup_graphics_pipeline() {
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        const auto* base_render_data = renderer->get_base_render_data();
        auto* resource_manager = WBE::ResourceManager::get_singleton();

        auto resource_chunk = WBE::make_ref<WBE::ResourceChunkLoadFile>(
            WBE::global_allocator(), "assets/models/models.json", resource_manager->get_allocator());
        resource_manager->register_chunk(resource_chunk);
        WBE::RefRes<WBE::RUStaticOpaqueSet> cube_set =
            resource_manager->get_resource<WBE::RUStaticOpaqueSet>("model_cube_static.static_opaque_set");
        if (cube_set == nullptr) {
            throw std::runtime_error("Failed to setup draw cube unshaded test: static opaque set resource was not found.");
        }

        aspect_ratio = base_render_data->swap_chain_aspect_ratio;
        camera_info = WBE::CameraInfoVKUBO{
            // Look at the origin from (2,2,2)
            .view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F)),
            .proj = glm::perspectiveRH_ZO(glm::radians(45.0F), aspect_ratio, 10.0F, 0.1F),
        };
        // Inverse the y value since Vulkan makes y axis points downwards.
        camera_info.proj[1][1] *= -1;
        auto pipeline_resource = resource_manager->get_resource<WBE::RUGraphicsPipeline>("draw_cube_unshaded_static_pipeline");
        if (pipeline_resource == nullptr) {
            throw std::runtime_error("Failed to setup draw cube unshaded test: static graphics pipeline resource was not found.");
        }

        camera_info_ubo = WBE::ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .initial_data(&camera_info, sizeof(camera_info))
                              .create_ref(base_render_data);

        WBE::RTRunStaticSetVK::Config task_config;
        auto task = WBE::make_unique<WBE::RTRunStaticSetVK>(render_allocator, renderer, cube_set, pipeline_resource, task_config);
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_SWAP_CHAIN_IMAGE),
            .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
            .container = WBE::MEM_NULL});
        pipeline_desc.add_resource(REND_RES_TEST_DRAW_CUBE_UNSHADED_CAMERA_INFO_BUFFER, camera_info_ubo);
        pipeline_desc.tasks.emplace_back(render_allocator, WBE::RT_RUN_STATIC_SET, std::move(task));
        WBE::Ref<WBE::RenderGraphVK> graph =
            WBE::RendererVK::get_singleton()->register_render_graph("test_render_graph", std::move(pipeline_desc));
        WBE::write_descriptor_debug_output("test_render_graph_graph_descriptors", graph->list_descriptors());
        WBE::RendererVK::get_singleton()->activate_render_graph("test_render_graph");
    }

private:
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    WBE::CameraInfoVKUBO camera_info{};
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
