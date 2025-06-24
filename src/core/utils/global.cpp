#include "global/global.h"
#include "core/engine_core.h"

namespace WhiteBirdEngine {

Global::Global(int p_argc, char* p_argv[])
    : Singleton<Global>() {
    singleton = this;
    engine_core = new EngineCore(p_argc, p_argv);
}

Global::Global(int p_argc, char* p_argv[], const Directory& p_root_dir)
    : Singleton<Global>() {
    singleton = this;
    engine_core = new EngineCore(p_argc, p_argv, p_root_dir);
}

Global::~Global() {
    delete engine_core;
    singleton = nullptr;
}


}
