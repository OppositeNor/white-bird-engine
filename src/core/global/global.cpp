#include "core/global/global.h"

namespace WhiteBirdEngine {

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
