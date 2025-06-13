#ifndef __WBE_PROFILLING_MANAGER_H__
#define __WBE_PROFILLING_MANAGER_H__

#include "core/logging/log.h"
#include "utils/interface/singleton.h"
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class ProfilingManager
 * @brief The manager for all the profilers.
 *
 */
class ProfilingManager : public Singleton<ProfilingManager> {
public:
    ProfilingManager() = default;
    virtual ~ProfilingManager() override {}

    /**
     * @brief Push a profiling data to the profile stash.
     *
     * @param p_channel The channel to push the data to.
     * @param p_delta The delta time that's been pushed into the profiler.
     */
    void push_profiling_data(ChannelID p_channel, uint64_t p_delta);

    /**
     * @brief Get all the profile data from the channel, and calculate the mean.
     * This will also remove all the stashed profiles.
     * @todo make channel a template argument and load it statically.
     * @param p_channel The channel to flush.
     */
    void flush_stashed_profiles(ChannelID p_channel);

    /**
     * @brief Get the profile data from a channel in seconds.
     *
     * @param p_channel The channel to get profile from.
     * @return The profile of the channel.
     */
    double get_profile(ChannelID p_channel) const;

private:
    mutable std::shared_mutex profile_stash_mutex;
    std::unordered_map<ChannelID, std::vector<uint64_t>> profile_stash;

    mutable std::shared_mutex profile_mutex;
    std::unordered_map<ChannelID, double> profiles;
};

}

#endif
