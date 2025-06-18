#include "core/global/global.h"
#include "utils/defs.h"

namespace WBE = WhiteBirdEngine;
namespace WhiteBirdEngine {
WBE_DEFINE_LABEL(WBE_USER)
}

int main(int argc, char* argv[]) {
    WBE::Global::init(argc, argv);

    WBE::Global::terminate();
    return 0;
}

