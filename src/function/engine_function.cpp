#include "function/engine_function.hh"
#include "function/game_config/game_config.hh"

namespace WhiteBirdEngine {

EngineFunction::EngineFunction(const Path& p_game_config_file_path) {
    singleton = this;
    game_config = new GameConfig(p_game_config_file_path);
}

EngineFunction::~EngineFunction() {
    delete game_config;
    singleton = nullptr;
}


}
