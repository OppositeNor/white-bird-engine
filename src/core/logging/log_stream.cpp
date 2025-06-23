#include "core/logging/log_stream.h"
#include "global/global.h"

namespace WhiteBirdEngine {
void LogStream::message(const std::string& p_str) {
    auto channel_name = Global::get_singleton()->game_metadata->get_channel_metadata()[channel_id];
    *ostream << "[" << channel_name << "] Message: " << p_str << std::endl;
}

void LogStream::warning(const std::string& p_str) {
    auto channel_name = Global::get_singleton()->game_metadata->get_channel_metadata()[channel_id];
    *ostream << "[" << channel_name << "] Warning: " << p_str << std::endl;
}

void LogStream::error(const std::string& p_str) {
    auto channel_name = Global::get_singleton()->game_metadata->get_channel_metadata()[channel_id];
    *ostream << "[" << channel_name << "] Error: " << p_str << std::endl;
}

}
