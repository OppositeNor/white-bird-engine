#include "core/profiling/profiling_manager.h"
#include <mutex>

namespace WhiteBirdEngine {

void ProfilingManager::push_profiling_data(ChannelID p_channel, uint64_t p_delta) {
    std::unique_lock lock(profile_stash_mutex);
    profile_stash[p_channel].push_back(p_delta);
}

void ProfilingManager::flush_stashed_profiles(ChannelID p_channel) {
    double total_avrg = 0.0;
    {
        double mul = 0.001 / profile_stash[p_channel].size();
        std::shared_lock lock(profile_stash_mutex);
        for (auto& data : profile_stash[p_channel]) {
            total_avrg += static_cast<double>(data) * mul;
        }
    }
    {
        std::unique_lock lock(profile_stash_mutex);
        profile_stash[p_channel].clear();
    }
    std::unique_lock lock(profile_mutex);
    profiles[p_channel] = total_avrg;
}

double ProfilingManager::get_profile(ChannelID p_channel) const {
    double profile_data;
    {
        std::shared_lock lock(profile_mutex);
        profile_data = profiles.at(p_channel);
    }
    return profile_data;

}

}
