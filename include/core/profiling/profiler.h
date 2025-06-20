#ifndef __WBE_PROFILLER_H__
#define __WBE_PROFILLER_H__

#include "function/global/global.h"
#include "core/logging/log.h"
#include "core/profiling/profiling_manager.h"
#include <chrono>
namespace WhiteBirdEngine {

/**
 * @brief The profiler class.
 * This initiates the profiling right after it is constructed, and ends and push data to the manager
 * right after it is destructed.
 */
class Profiler {
public:

    Profiler(ChannelID p_channel)
        : channel(p_channel) {
        start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    ~Profiler() {
        uint64_t delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        delta -= start_time_ms;
        Global::get_singleton()->profiling_manager->push_profiling_data(channel, delta);
    }

    Profiler(const Profiler&) = delete;
    Profiler(Profiler&&) = delete;
    Profiler& operator=(const Profiler&) = delete;
    Profiler& operator=(Profiler&&) = delete;

private:
    uint64_t start_time_ms;
    ChannelID channel;
};

}

#endif
