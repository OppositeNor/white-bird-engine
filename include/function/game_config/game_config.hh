#ifndef __WBE_GAME_CONFIG_HH__
#define __WBE_GAME_CONFIG_HH__

#include "platform/file_system/path.hh"
#include "utils/interface/singleton.hh"
#include "utils/utils.hh"
namespace WhiteBirdEngine {

class GameConfig : public Singleton<GameConfig> {
public:

    GameConfig(const Path& p_config_file_path) {
        // TODO
    }
    ~GameConfig() {}

    const GameConfigOptions& get_config_options() const {
        return config_options;
    }

private:
    GameConfigOptions config_options;
};

}

#endif
