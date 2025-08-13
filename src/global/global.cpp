#include "global/global.hh"
#include "core/engine_core.hh"
#include "function/engine_function.hh"

namespace WhiteBirdEngine {

Global::Global(int p_argc, char* p_argv[])
    : Singleton<Global>() {
    singleton = this;
    engine_core = new EngineCore(p_argc, p_argv);
    auto* file_system = Global::get_singleton()->engine_core->file_system;
    engine_function = new EngineFunction(Path(file_system->get_root_directory(), "game_config.yaml"));
}

Global::Global(int p_argc, char* p_argv[], const Directory& p_root_dir)
    : Singleton<Global>() {
    singleton = this;
    engine_core = new EngineCore(p_argc, p_argv, p_root_dir);
    auto* file_system = Global::get_singleton()->engine_core->file_system;
    engine_function = new EngineFunction(Path(file_system->get_root_directory(), "game_config.yaml"));
}

Global::~Global() {
    delete engine_function;
    delete engine_core;
    singleton = nullptr;
}


}
