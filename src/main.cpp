#include "core/global/global.h"
#include "utils/defs.h"

namespace WBE = WhiteBirdEngine;
namespace WhiteBirdEngine {
WBE_DEFINE_LABEL(WBE_USER)
}

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv);
    return 0;
}

