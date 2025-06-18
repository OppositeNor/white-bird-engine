#include "core/core_utils.h"
#include "core/global/global.h"
#include "utils/defs.h"

namespace WBE = WhiteBirdEngine;
namespace WhiteBirdEngine {
WBE_DEFINE_LABEL(WBE_USER)
}

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv);
    WBE::wbe_log(WBE::WBE_USER)->message("Hello World!");
    return 0;
}

