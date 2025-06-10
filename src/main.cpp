#include "core/logging/log_stream.h"
#include "utils/defs.h"
#include <iostream>

namespace WBE = WhiteBirdEngine;
namespace WhiteBirdEngine {
WBE_DEFINE_LABEL(WBE_USER)
}

int main() {
    WBE::LogStream<WBE::WBE_USER> log(std::cout);

    log.message("Hello World!");

    return 0;
}

