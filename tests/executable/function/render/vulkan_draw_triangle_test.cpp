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
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_graph_bindings_function_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "global/global.hh"
#include "input_manager_mocked.hh"
#include "platform/file_system/path.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstdlib>
#include <format>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace WBE = WhiteBirdEngine;

// The render graph definition, res/assets/render_graphs/render_graphs.toml, whose recipe is render_graphs.draw_triangle.
inline constexpr std::string_view DRAW_TRIANGLE_RENDER_GRAPH = "draw_triangle_graph";

class ApplicationVulkanDrawPipelineTest : public WBE::ApplicationGame {
public:
    ApplicationVulkanDrawPipelineTest()
        : WBE::ApplicationGame({}, WBE::make_ref<WBE::InputManagerMocked>(WBE::global_allocator(), WBE::global_allocator())) {
    }

    virtual void logic_update(double p_delta) override {
    }

    virtual void render_update(double /*p_delta*/) override {
        auto* renderer = WBE::RendererVK::get_singleton();
        WBE::on_exec_test_render(*renderer, renderer->get_render_context()->current_frame);
    }

    void setup_graphics_pipeline() {
        auto triangle_mesh = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUMesh3D>("triangle");
        if (triangle_mesh == nullptr || triangle_mesh->opaque_instances.submesh_count != 1 ||
            triangle_mesh->opaque_instances.submeshes == nullptr) {
            throw std::runtime_error("Failed to setup draw triangle test: expected exactly one opaque triangle submesh.");
        }
        // The recipe registers the mesh and the pipeline and imports only the swap chain, which the renderer supplies.
        render_graph = WBE::acquire_test_render_graph(*WBE::RendererVK::get_singleton(),
            DRAW_TRIANGLE_RENDER_GRAPH,
            WBE::make_ref<WBE::RenderGraphBindingsFunctionVK>(WBE::global_allocator(), DRAW_TRIANGLE_RENDER_GRAPH));
    }

    void release_graphics_pipeline() {
        WBE::release_test_render_graph(*WBE::RendererVK::get_singleton(), render_graph);
    }

private:
    WBE::Ref<WBE::RenderGraphVK> render_graph;
};

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Path("test_env"));
    {
        ApplicationVulkanDrawPipelineTest game_application;
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
