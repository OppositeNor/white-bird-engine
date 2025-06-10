#include "core/global/global.h"

namespace WhiteBirdEngine {

void Global::init(char* p_argc[], int p_argv, const Path& p_config_file_path) {
    singleton = new Global(p_argc, p_argv, p_config_file_path);
}

void Global::terminate() {
    delete singleton;
}

Global::Global(char* p_argc[], int p_argv, const Path& p_config_file_path)
    : Singleton<Global>() {
    // TODO
}

Global::~Global() {
    // TODO
}

Global* Global::get_singleton() {
    return nullptr; // TODO
}


}
