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
#include "function/application/application_game.hh"
#include "core/engine_core.hh"
#include "core/memory/reference_strong.hh"
#include "function/engine_function.hh"
#include "function/render/renderer.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_dummy/renderer_dummy.hh"

namespace WhiteBirdEngine {

ApplicationGame::ApplicationGame() {
    init();
}

void ApplicationGame::run() {
    bool should_close = false;
    SDL_Event event;
    while (!should_close) {
        while (SDL_PollEvent(&event)) {
            // TODO: replace with event handler.
            switch (event.type) {
                case SDL_QUIT:
                    should_close = true;
                    break;
            }
        }
        render_tick(0.01);
    }
}

void ApplicationGame::init() {
    renderer_allocator = make_ref<RenderAllocator>(global_allocator(), WBE_MiB(0.6));
    // TODO: Create renderer based on config file.
    auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();
    if (game_config.render_api == "vulkan") {
        renderer = make_ref<RendererVK>(global_allocator(), renderer_allocator.get());
    }
    else {
        renderer = make_ref<RendererDummy>(global_allocator());
    }
}

void ApplicationGame::render_tick(double p_delta) {
    renderer->tick(p_delta);
}

void ApplicationGame::logic_tick(double p_delta) {
    // TODO
}

}
