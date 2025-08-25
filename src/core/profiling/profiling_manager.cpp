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
#include "core/profiling/profiling_manager.hh"
#include <mutex>

namespace WhiteBirdEngine {

void ProfilingManager::push_profiling_data(ChannelID p_channel, double p_start_time, double p_delta, const std::string& p_file, uint32_t p_line) {
    std::unique_lock lock(profile_stash_mutex);
    profile_stash[p_channel].push_back({.start_time = p_start_time, .delta = p_delta, .file = p_file, .line = p_line});
}

}
