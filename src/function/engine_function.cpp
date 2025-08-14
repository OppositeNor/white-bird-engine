#include "function/engine_function.hh"
#include "function/game_config/game_config.hh"
#include "function/render/renderer_dummy.hh"
#include "function/render/renderer_vk.hh"
#include "utils/utils.hh"

namespace WhiteBirdEngine {

EngineFunction::EngineFunction(const Path& p_game_config_file_path) {
    singleton = this;
    game_config = new GameConfig(p_game_config_file_path);
    auto render_api = game_config->get_config_options().render_api;
    switch (render_api) {
        case GameConfigOptions::RenderAPI::DUMMY:
            renderer = new RendererDummy();
        break;
        case GameConfigOptions::RenderAPI::VULKAN:
            renderer = new RendererVK();
        break;
    }
}

EngineFunction::~EngineFunction() {
    delete renderer;
    delete game_config;
    singleton = nullptr;
}


}
