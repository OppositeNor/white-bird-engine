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
#include "core/memory/reference_raw.hh"
#include "core/memory/reference_strong.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/function_utils.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/i_render_graph_bindings_vk.hh"
#include "function/render_vk/mesh_compiler_vk.hh"
#include "function/render_vk/rend_recipe_context_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/render_graph_bindings_function_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
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
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

static constexpr std::string_view REND_RES_TEST_MATERIAL_VARIANTS_CAMERA_INFO_BUFFER = "camera_info";
// The render graph definition, whose recipe is render_graphs.draw_material_variants.
static constexpr std::string_view MATERIAL_VARIANTS_RENDER_GRAPH = "draw_material_variants_graph";

class ApplicationVulkanDrawMaterialVariantsTest : public WBE::ApplicationGame {
public:
    ApplicationVulkanDrawMaterialVariantsTest()
        : WBE::ApplicationGame({}, WBE::make_ref<WBE::InputManagerMocked>(WBE::global_allocator(), WBE::global_allocator())) {
    }

    virtual void logic_update(double /* p_delta */) override {
        WBE::on_exec_test_logic(*get_window());
    }

    virtual void render_update(double p_delta) override {
        auto* renderer = WBE::RendererVK::get_singleton();
        WBE::on_exec_test_render(*renderer, renderer->get_render_context()->current_frame);
        camera_info.view = glm::rotate(camera_info.view, static_cast<float>(p_delta), glm::vec3(0.0F, 1.0F, 0.0F));
        camera_info_ubo->update_buffer(&camera_info, sizeof(CameraInfoGPU));
    }

    void setup_graphics_pipeline() {
        auto* renderer = WBE::RendererVK::get_singleton();
        const auto* render_context = renderer->get_render_context();
        auto* resource_manager = WBE::ResourceManager::get_singleton();

        auto resource_chunk = WBE::make_ref<WBE::ResourceChunkLoadFile>(
            WBE::global_allocator(), "res_chunks/rendering_test_resources.json", resource_manager->get_allocator());
        resource_manager->register_chunk(resource_chunk);
        // The cube's faces are split between a material of the default pipeline variant and one of the "tinted" variant.
        WBE::RefRes<WBE::RUMesh3D> cube_mesh = resource_manager->get_resource<WBE::RUMesh3D>("material_variants.cube");
        if (cube_mesh == nullptr) {
            throw std::runtime_error("Failed to setup draw material variants test: static mesh resource was not found.");
        }

        aspect_ratio = render_context->swap_chain_aspect_ratio;
        camera_info = CameraInfoGPU{
            // Look at the origin from (2,2,2)
            .view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F)),
            .proj = glm::perspectiveRH_ZO(glm::radians(45.0F), aspect_ratio, 10.0F, 0.1F),
        };
        // Inverse the y value since Vulkan makes y axis points downwards.
        camera_info.proj[1][1] *= -1;
        // Recipes cannot set initial data, so the application owns the camera buffer and the recipe imports it.
        camera_info_ubo = render_context->create_buffer(WBE::ROBufferVK::Builder()
                .initial_data(&camera_info, sizeof(CameraInfoGPU), WBE::DeviceDomain::HOST_DEVICE)
                .set_size(sizeof(CameraInfoGPU))
                .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
        // The recipe registers the pipelines; its draw task compiles the mesh by its resource key and imports its buffers.
        render_graph = WBE::acquire_test_render_graph(*renderer,
            MATERIAL_VARIANTS_RENDER_GRAPH,
            WBE::make_ref<WBE::RenderGraphBindingsFunctionVK>(WBE::global_allocator(),
                MATERIAL_VARIANTS_RENDER_GRAPH,
                [camera_info_ubo = camera_info_ubo](
                    const WBE::RenderContextVK& /*p_render_context*/, const WBE::RendRecipeContextVK& /*p_recipe_context*/) {
                    WBE::RendGraphPreparedBindingsVK result;
                    result.imports.add(REND_RES_TEST_MATERIAL_VARIANTS_CAMERA_INFO_BUFFER, *camera_info_ubo);
                    return result;
                }));
    }

    void release_graphics_pipeline() {
        WBE::release_test_render_graph(*WBE::RendererVK::get_singleton(), render_graph);
    }

private:
    WBE::Ref<WBE::RenderGraphVK> render_graph;
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    CameraInfoGPU camera_info{};
    float aspect_ratio = 1.0F;
};

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Path("test_env"));
    {
        ApplicationVulkanDrawMaterialVariantsTest game_application;
        {
            WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiler");
            game_application.setup_graphics_pipeline();
            game_application.run();
            game_application.release_graphics_pipeline();
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
