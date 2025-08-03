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
     * @class ProfileData
     * @brief Data for profilations.
     *
     */
    struct ProfileData {
        double delta;
        std::string file;
        uint32_t line;
    };

    /**
     * @brief Push a profiling data to the profile stash.
     *
     * @param p_channel The channel to push the data to.
     * @param p_delta The delta time that's been pushed into the profiler in seconds.
     * @param p_file The source file that the proflier is created.
     * @param p_line The line number where the profiler is created.
     */
    void push_profiling_data(ChannelID p_channel, double p_delta, const std::string& p_file, uint32_t p_line);

    /**
     * @brief Get data for profiliing.
     *
     * @param p_channel The channel to get data from.
     * @return The profiling data.
     */
    const std::vector<ProfileData>& get_profile_data(ChannelID p_channel) {
        return profile_stash[p_channel];
    }

private:
    mutable std::shared_mutex profile_stash_mutex;
    mutable std::unordered_map<ChannelID, std::vector<ProfileData>> profile_stash;

};

}

#endif
