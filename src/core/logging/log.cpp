#include "core/logging/log.h"
#include "core/global/global.h"

namespace WhiteBirdEngine {
Log* wbe_console_log(ChannelID p_channel) {
    return Global::get_singleton()->stdio_logging_manager->get_log(p_channel);
}

}

