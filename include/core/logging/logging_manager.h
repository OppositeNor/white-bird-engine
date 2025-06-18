#ifndef __LOGGING_MANAGER_H__
#define __LOGGING_MANAGER_H__

#include "core/logging/log.h"
#include "utils/interface/singleton.h"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
namespace WhiteBirdEngine {


// TODO: Test
template <typename LogType, typename T>
class LoggingManager : public Singleton<LoggingManager<LogType, T>> {
public:
    LoggingManager(T& p_channel_argument)
        : Singleton<LoggingManager>(), channel_argument(&p_channel_argument) {}

    virtual ~LoggingManager() override {}

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
