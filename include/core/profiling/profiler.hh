// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_PROFILLER_HH__
#define __WBE_PROFILLER_HH__

#include "core/engine_core.hh"
#include "core/logging/log.hh"
#include "core/profiling/profiling_manager.hh"
#include <chrono>
namespace WhiteBirdEngine {

#define WBE_START_PROFILE(CHANNEL) auto profiler_##__LINE__ = WhiteBirdEngine::Profiler(CHANNEL, std::string(__FILE__), __LINE__)

/**
 * @brief The profiler class.
 * This initiates the profiling right after it is constructed, and ends and push data to the manager
 * right after it is destructed.
 */
class Profiler {
public:

    Profiler(ChannelID p_channel, const std::string& p_file, uint32_t p_line)
        : channel(p_channel), file(p_file), line(p_line) {
        start_time = std::chrono::high_resolution_clock::now();
    }

    ~Profiler() {
        const auto end_time = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> diff = end_time - start_time;
        double delta = diff.count();
        EngineCore::get_singleton()->profiling_manager->push_profiling_data(channel, delta, file, line);
    }

    Profiler(const Profiler&) = delete;
    Profiler(Profiler&&) = delete;
    Profiler& operator=(const Profiler&) = delete;
    Profiler& operator=(Profiler&&) = delete;

private:
    ChannelID channel;
    std::string file;
    uint32_t line;
    std::chrono::high_resolution_clock::time_point start_time;
};

}

#endif
