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
#include "function/engine_function.hh"
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/engine_core.hh"
#include "core/memory/reference_strong.hh"
#include "function/game_config/game_config.hh"
#include "function/render/allocator_vk.hh"
#include "function/render/renderer_dummy.hh"
#include "function/render/renderer_vk.hh"
#include "utils/utils.hh"

namespace WhiteBirdEngine {

EngineFunction::EngineFunction(const Path& p_game_config_file_path) {
    singleton = this;
    game_config = new GameConfig(p_game_config_file_path);
    renderer_allocator = new HeapAllocatorAlignedPool(WBE_MiB(512));
    init_renderer();
}

EngineFunction::~EngineFunction() {
    if (renderer != nullptr) {
        delete renderer;
    }
    delete renderer_allocator;
    delete game_config;
    singleton = nullptr;
}

void EngineFunction::init_renderer() {
    auto render_api = game_config->get_config_options().render_api;
    switch (render_api) {
        case GameConfigOptions::RenderAPI::DUMMY:
            renderer = new RendererDummy();
        break;
        case GameConfigOptions::RenderAPI::VULKAN:
            Ref<AllocatorVK> vulkan_allocator = make_ref<AllocatorVK>(
                EngineCore::get_singleton()->pool_allocator, renderer_allocator);
            renderer = new RendererVK(vulkan_allocator);
        break;
    }
}
}
