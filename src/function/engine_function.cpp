#include "function/engine_function.hh"
#include "function/game_config/game_config.hh"
#include "function/render/renderer_vk.hh"
#include "utils/utils.hh"

namespace WhiteBirdEngine {

EngineFunction::EngineFunction(const Path& p_game_config_file_path) {
    singleton = this;
    game_config = new GameConfig(p_game_config_file_path);
    if (game_config->get_config_options().render_api == GameConfigOptions::RenderAPI::VULKAN) {
        renderer = new RendererVK();
    }
}

EngineFunction::~EngineFunction() {
    delete renderer;
    delete game_config;
    singleton = nullptr;
}


}
