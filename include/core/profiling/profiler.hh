/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
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
