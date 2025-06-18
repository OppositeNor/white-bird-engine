#include "core/logging/log.h"
#include "core/global/global.h"

namespace WBE = WhiteBirdEngine;

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv);
    WBE::wbe_log(WBE::WBE_CHANNEL_USER)->message("Hello World!");
    return 0;
}

