#ifndef __LOGGING_MANAGER_H__
#define __LOGGING_MANAGER_H__

#include "core/logging/log.h"
#include "utils/interface/singleton.h"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
namespace WhiteBirdEngine {

/**
 * @class LoggingManager
 * @brief Manages all the log instances.
 *
 * @tparam LogType The type of the log.
 * @tparam T The input parameter of the constructor of the log object.
 * @param p_channel_argument The input argument of the log object.
 */
template <typename LogType, typename T>
class LoggingManager : public Singleton<LoggingManager<LogType, T>> {
public:
    /**
     * @brief Constructor.
     *
     * @param p_channel_argument The argument to input to the constructor of the log object.
     */
    LoggingManager(T& p_channel_argument)
        : Singleton<LoggingManager>(), channel_argument(&p_channel_argument) {}

    virtual ~LoggingManager() override {}

    /**
     * @brief Get the instance of a log object of a specific channel.
     *
     * @param p_channel_id The channel of the log object to get.
     * @return The log object instance.
     */
    Log* get_log(ChannelID p_channel_id) {
        {
            std::shared_lock lock(channel_logs_mutex);
            auto channel = channel_logs.find(p_channel_id);
            if (channel != channel_logs.end()) {
                return channel->second.get();
            }
        }
        {
            std::unique_lock lock(channel_logs_mutex);
            auto channel = channel_logs.find(p_channel_id);
            if (channel != channel_logs.end()) {
                return channel->second.get();
            }
            channel_logs[p_channel_id] = std::make_unique<LogType>(p_channel_id, *channel_argument);
            return channel_logs.at(p_channel_id).get();
        }
    }

private:
    std::shared_mutex channel_logs_mutex;
    std::unordered_map<ChannelID, std::unique_ptr<Log>> channel_logs;

    T* channel_argument;
};

}

#endif
