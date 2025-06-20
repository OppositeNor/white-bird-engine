#include "core/profiling/profiling_manager.h"
#include <mutex>

namespace WhiteBirdEngine {

void ProfilingManager::push_profiling_data(ChannelID p_channel, uint64_t p_delta, const std::string& p_file, uint32_t p_line) {
    std::unique_lock lock(profile_stash_mutex);
    profile_stash[p_channel].push_back({.delta = p_delta, .file = p_file, .line = p_line});
}

}
