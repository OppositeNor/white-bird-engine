#include "core/logging/log.hh"
#include "core/engine_core.hh"

namespace WhiteBirdEngine {
Log* wbe_console_log(ChannelID p_channel) {
    return EngineCore::get_singleton()->stdio_logging_manager->get_log(p_channel);
}

}

