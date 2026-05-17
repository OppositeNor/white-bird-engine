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
#include "core/logging/log.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <array>
#include <cstdlib>
#include <exception>
#include <format>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <memory>
#include <print>
#include <span>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;
namespace WhiteBirdEngine {
WBE_LABEL(REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE, WBE_RENDER_RESOURCE);
}

namespace {

WBE::CameraInfoVKUBO default_camera_info(float p_aspect_ratio) {
    WBE::CameraInfoVKUBO camera_info{
        .view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F)),
        .proj = glm::perspective(glm::radians(45.0F), p_aspect_ratio, 0.1F, 10.0F),
    };
    camera_info.proj[1][1] *= -1;
    return camera_info;
}

} // namespace

class ApplicationVulkanDrawCubeTest : public WBE::ApplicationGame {
public:
    virtual void logic_update(double /* p_delta */) override {
    }

    void setup_draw_task() {
        auto* resource_manager = WBE::ResourceManager::get_singleton();
        auto cube_mesh = resource_manager->get_resource<WBE::RUMesh3D>("cube");
        auto default_material = resource_manager->get_resource<WBE::RUMaterial>("default_material");
        const auto* base_render_data = WBE::RendererVK::get_singleton()->get_base_render_data();
        if (cube_mesh == nullptr) {
            throw std::runtime_error("Failed to setup draw cube test: cube mesh resource was not found.");
        }
        if (default_material == nullptr || default_material->graphics_pipeline == nullptr ||
            default_material->graphics_pipeline->resource_key == nullptr) {
            throw std::runtime_error("Failed to setup draw cube test: default material has no compiled graphics pipeline key.");
        }

        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer_vk = WBE::RendererVK::get_singleton();
        auto camera_info = default_camera_info(renderer_vk->get_base_render_data()->swap_chain_aspect_ratio);
        std::array<WBE::RenderUniformBinding, 2> uniform_bindings{
            WBE::RenderUniformBinding{.field_path = "camera_info.view", .data = &camera_info.view, .size = sizeof(camera_info.view)},
            WBE::RenderUniformBinding{.field_path = "camera_info.proj", .data = &camera_info.proj, .size = sizeof(camera_info.proj)},
        };
        WBE::RenderDatabase::get_singleton()->set_static_opaque_triangles(cube_mesh.get());
        renderer_vk->update_static_opaque_triangles();

        WBE::GraphicsPipelineCompilerVKContext compiler_context{
            .allocator = render_allocator,
            .render_data = renderer_vk->get_base_render_data(),
            .render_database = renderer_vk->get_render_database(),
            .render_pass = renderer_vk->get_swap_chain_render_pass(),
            .renderer = renderer_vk,
        };
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        std::array<VkClearValue, 1> clear_values{};
        clear_values[0].color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}};
        auto [graphics_pipeline, descriptor_sets] = WBE::GraphicsPipelineCompilerVK(compiler_context)
                .compile(default_material->graphics_pipeline->resource_key,
                    default_material->graphics_pipeline,
                    std::span<const WBE::RenderUniformBinding>(uniform_bindings.data(), uniform_bindings.size()));
        pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = WBE::REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE,
            .res_type_id = WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID,
            .res = graphics_pipeline});
        WBE::RendTaskDescVK task_desc(render_allocator);
        task_desc.task_id = WBE::RT_RUN_PIPELINE;
        task_desc.create = [render_allocator, base_render_data, descriptor_sets]() -> WBE::Unique<WBE::RenderTaskVK> {
            return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(render_allocator,
                base_render_data,
                glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
                WBE::REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK,
                WBE::REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE,
                WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
                WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
                std::span<const WBE::Ref<WBE::RODescriptorSetVK>>(descriptor_sets.data(), descriptor_sets.size()));
        };
        pipeline_desc.tasks.emplace_back(std::move(task_desc));
        renderer_vk->register_render_graph("draw_cube_pipeline", std::move(pipeline_desc));
        renderer_vk->activate_render_graph("draw_cube_pipeline");
    }
};

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Directory({"test_env"}));
    try {
        {
            ApplicationVulkanDrawCubeTest game_application;
            {
                WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiler");
                game_application.setup_draw_task();
                game_application.run();
                size_t renderer_alloc_max = game_application.get_renderer_allocator()->get_internal_fragmentation_tracker();
                size_t global_alloc_max = WBE::EngineCore::get_singleton()->pool_allocator->get_internal_fragmentation_tracker();
                WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Global allocator max used: {}", global_alloc_max));
                WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator max used: {}", renderer_alloc_max));
            }
        }
        const auto& global_profiles = global->engine_core->profiling_manager->get_profile_data(WBE::WBE_CHANNEL_GLOBAL);
        for (const auto& profile : global_profiles) {
            WBE::stdout_log(WBE::WBE_CHANNEL_GLOBAL)->message(std::format("{}: Delta: {} seconds", profile.message, profile.delta));
        }
    } catch (const std::exception& e) {
        std::println("Exception caught in main: {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
