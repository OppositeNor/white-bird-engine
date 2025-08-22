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
#ifndef __LOGGING_MANAGER_HH__
#define __LOGGING_MANAGER_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/stack_allocator.hh"
#include "core/logging/log.hh"
#include "utils/interface/singleton.hh"
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
     * @param p_log_allocator_size The argument to input to the constructor of the log object.
     */
    LoggingManager(T& p_channel_argument, size_t p_log_allocator_size = WBE_KiB(1))
        : Singleton<LoggingManager>(), channel_argument(&p_channel_argument), log_allocator(p_log_allocator_size) {}

    virtual ~LoggingManager() override {
        size_t channel_logs_size = channel_logs.size();
        for (uint32_t i = 0; i < channel_logs_size; ++i) {
            pop_stack_obj<LogType>(log_allocator);
        }
        log_allocator.clear();
    }

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
                return log_allocator.get_obj<Log>(channel->second);
            }
        }
        {
            std::unique_lock lock(channel_logs_mutex);
            auto channel = channel_logs.find(p_channel_id);
            if (channel != channel_logs.end()) {
                return log_allocator.get_obj<Log>(channel->second);
            }
            MemID result = create_stack_obj<LogType>(log_allocator, p_channel_id, *channel_argument);
            channel_logs[p_channel_id] = result;
            return log_allocator.get_obj<Log>(result);
        }
    }

private:
    std::shared_mutex channel_logs_mutex;
    std::unordered_map<ChannelID, MemID> channel_logs;

    T* channel_argument;
    StackAllocator log_allocator;
};

}

#endif
