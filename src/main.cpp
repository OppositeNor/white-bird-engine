#include "core/logging/log.hh"
#include "global/global.hh"

namespace WBE = WhiteBirdEngine;

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv);
    WBE::wbe_console_log(WBE::WBE_CHANNEL_USER)->message("Hello World!");
    return 0;
}

