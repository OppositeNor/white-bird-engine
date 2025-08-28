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
#include "function/application/application_game.hh"
#include "global/global.hh"
#include <format>

namespace WBE = WhiteBirdEngine;

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv);
    {
        WhiteBirdEngine::ApplicationGame game_application;
        WBE::wbe_console_log(WBE::WBE_CHANNEL_USER)->message("Hello World!");
#ifdef WBE_ENABLE_HEAP_ALLOCATOR_TRACKER
        size_t renderer_alloc_max = game_application.get_renderer_allocator()->get_max_data_size();
        size_t global_alloc_max = WBE::EngineCore::get_singleton()->pool_allocator->get_max_data_size();
        WBE::wbe_console_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator max used: {}", renderer_alloc_max));
        WBE::wbe_console_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Global allocator max used: {}", global_alloc_max));
#endif
    }
    return 0;
}

