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
#include "global/global.hh"
#include <cstdlib>
#include <format>

namespace WBE = WhiteBirdEngine;

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv);
    {
        WhiteBirdEngine::ApplicationGame game_application;
        {
            WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiller");
            WBE::wbe_console_log(WBE::WBE_CHANNEL_USER)->message("Hello World!");
            size_t renderer_alloc_max = game_application.get_renderer_allocator()->get_internal_fragmentation_tracker();
            size_t global_alloc_max = WBE::EngineCore::get_singleton()->pool_allocator->get_internal_fragmentation_tracker();
            WBE::wbe_console_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Global allocator max used: {}", global_alloc_max));
            WBE::wbe_console_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator max used: {}", renderer_alloc_max));
        }
        WBE::wbe_console_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Global allocator layout: {}",
            static_cast<std::string>(*WBE::global_allocator())));
        WBE::wbe_console_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator layout: {}",
            static_cast<std::string>(*game_application.get_renderer_allocator())));
    }
    auto& profiles = global->engine_core->profiling_manager->get_profile_data(WBE::WBE_CHANNEL_GLOBAL);
    for (auto& profile : profiles) {
        WBE::wbe_console_log(WBE::WBE_CHANNEL_GLOBAL)->message(std::format("{}: Total: {} seconds",
                                                                           profile.message, profile.start_time + profile.delta));
    }
    return EXIT_SUCCESS;
}

