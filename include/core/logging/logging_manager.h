#ifndef __LOGGING_MANAGER_H__
#define __LOGGING_MANAGER_H__

#include "core/logging/log.h"
#include "utils/interface/singleton.h"
#include <list>
#include <mutex>
namespace WhiteBirdEngine {


// TODO: Test
template <template <ChannelID> typename LogType, typename T>
class LoggingManager : public Singleton<LoggingManager<LogType, T>> {
public:
    LoggingManager(T& p_channel_argument)
        : Singleton<LoggingManager>(), channel_argument(&p_channel_argument) {}

    virtual ~LoggingManager() override {
        for (auto* log : created_channel_list) {
            delete log;
        }
    }

    template <ChannelID CHANNEL>
    Log* get_log() {
        if (channel_log<CHANNEL> == nullptr) {
            std::lock_guard lock(channel_mutex<CHANNEL>);
            if (channel_log<CHANNEL> == nullptr) {
                channel_log<CHANNEL> = new LogType<CHANNEL>(*channel_argument);
                created_channel_list.push_back(channel_log<CHANNEL>);
            }
        }
        return channel_log<CHANNEL>;
    }

private:
    template <ChannelID CHANNEL>
    inline static Log* channel_log;

    template <ChannelID CHANNEL>
    inline static std::mutex channel_mutex;

    std::list<Log*> created_channel_list;

    T* channel_argument;
};

}

#endif
