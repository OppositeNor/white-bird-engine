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
#include "generated/type_uuid.gen.hh"
#include "render_test.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/render/render_task.hh"
#include "function/render/renderer.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "global/global.hh"
#include <cstdlib>
#include <format>

namespace WBE = WhiteBirdEngine;

void setup_pipeline(WBE::ApplicationGame& game_application);

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv);
    {
        WBE::ApplicationGame game_application;
        {
            WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiller");
            setup_pipeline(game_application);
            game_application.run();
            size_t renderer_alloc_max = game_application.get_renderer_allocator()->get_internal_fragmentation_tracker();
            size_t global_alloc_max = WBE::EngineCore::get_singleton()->pool_allocator->get_internal_fragmentation_tracker();
            WBE::wbe_console_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Global allocator max used: {}", global_alloc_max));
            WBE::wbe_console_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator max used: {}", renderer_alloc_max));
        }
    }
    auto& global_profiles = global->engine_core->profiling_manager->get_profile_data(WBE::WBE_CHANNEL_GLOBAL);
    for (auto& profile : global_profiles) {
        WBE::wbe_console_log(WBE::WBE_CHANNEL_GLOBAL)->message(std::format("{}: Delta: {} seconds",
                                                                           profile.message, profile.delta));
    }
    return EXIT_SUCCESS;
}

void setup_pipeline(WBE::ApplicationGame& game_application) {
    auto render_allocator = game_application.get_renderer_allocator().get();
    WBE::Ref<WBE::RendTaskInfo> rend_task_info = WBE::make_ref<WBE::RendTaskInfo>(render_allocator);
    rend_task_info->task_id = WBE::RT_RUN_PIPELINE;
    rend_task_info->prev.push_back(WBE::RT_PRE_EXECUTE_PIPELINE_VK);
    rend_task_info->req_res.push_back({.res_id = WBE::REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK, .type_id = WBE::TypeUUID<WBE::ROFramebufferVK>::UUID});
    rend_task_info->req_res.push_back({.res_id = WBE::REND_RES_BASE_GRAPHICS_PIPELINE_VK, .type_id = WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID});
    rend_task_info->render_task_factory
        = WBE::make_ref<WBE::RTRunGraphicsPipelineFactoryVK>(render_allocator, WBE::RendererVK::get_singleton()->get_base_render_data(), true);
    game_application.get_renderer()->register_pipeline("test_pipeline", rend_task_info);
    game_application.get_renderer()->activate_pipeline("test_pipeline");

}

