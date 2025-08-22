// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#include "core/logging/log_stream.hh"
#include "core/engine_core.hh"

namespace WhiteBirdEngine {
void LogStream::message(const std::string& p_str) {
    auto channel_name = EngineCore::get_singleton()->game_metadata->get_channel_metadata()[channel_id];
    *ostream << "[" << channel_name << "] Message: " << p_str << std::endl;
}

void LogStream::warning(const std::string& p_str) {
    auto channel_name = EngineCore::get_singleton()->game_metadata->get_channel_metadata()[channel_id];
    *ostream << "[" << channel_name << "] Warning: " << p_str << std::endl;
}

void LogStream::error(const std::string& p_str) {
    auto channel_name = EngineCore::get_singleton()->game_metadata->get_channel_metadata()[channel_id];
    *ostream << "[" << channel_name << "] Error: " << p_str << std::endl;
}

}
