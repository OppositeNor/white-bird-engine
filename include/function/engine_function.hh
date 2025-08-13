#ifndef __WBE_ENGINE_FUNCTION_HH__
#define __WBE_ENGINE_FUNCTION_HH__

#include "function/game_config/game_config.hh"
#include "function/render/renderer.hh"
#include "platform/file_system/path.hh"
#include "utils/interface/singleton.hh"
namespace WhiteBirdEngine {

class EngineFunction : public Singleton<EngineFunction>{
public:
    ~EngineFunction();

    EngineFunction(const Path& p_game_config_file_path);

    static EngineFunction* get_singleton() {
        return singleton;
    }

    GameConfig* game_config = nullptr;
    Renderer* renderer = nullptr;
private:

    inline static EngineFunction* singleton;
};

}

#endif
