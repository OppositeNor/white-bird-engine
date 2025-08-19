#include "function/engine_function.hh"
#include "core/allocator/heap_allocator.hh"
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/memory/reference_strong.hh"
#include "function/game_config/game_config.hh"
#include "function/render/allocator_vk.hh"
#include "function/render/renderer_dummy.hh"
#include "function/render/renderer_vk.hh"
#include "global/global.hh"
#include "utils/utils.hh"

namespace WhiteBirdEngine {

EngineFunction::EngineFunction(const Path& p_game_config_file_path) {
    singleton = this;
    game_config = new GameConfig(p_game_config_file_path);
    renderer_allocator = new HeapAllocatorAlignedPool(WBE_MiB(500));
    // TODO: Initialize render api
}

EngineFunction::~EngineFunction() {
    delete renderer;
    delete renderer_allocator;
    delete game_config;
    singleton = nullptr;
}

void EngineFunction::init_render_api() {
    auto render_api = game_config->get_config_options().render_api;
    switch (render_api) {
        case GameConfigOptions::RenderAPI::DUMMY:
            renderer = new RendererDummy();
        break;
        case GameConfigOptions::RenderAPI::VULKAN:
            Ref<AllocatorVK> vulkan_allocator = make_ref<AllocatorVK, HeapAllocator>(
                Global::get_singleton()->engine_core->pool_allocator, renderer_allocator);
            renderer = new RendererVK(vulkan_allocator);
        break;
    }
}

}
