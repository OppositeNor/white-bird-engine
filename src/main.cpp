#include "core/logging/log.h"
#include "global/global.h"
#include "core/surface/3d_primitive.hpp"

namespace WBE = WhiteBirdEngine;

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv);
    WBE::wbe_console_log(WBE::WBE_CHANNEL_USER)->message("Hello World!");
    return 0;
}

