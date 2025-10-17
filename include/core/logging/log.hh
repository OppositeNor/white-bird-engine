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
#ifndef __WBE_LOG_HH__
#define __WBE_LOG_HH__

#include "core/reflection/reflection_defs.hh"
#include "utils/utils.hh"
#include <string>
namespace WhiteBirdEngine {

// default labels
WBE_LABEL(WBE_CHANNEL_UNKNOWN, WBE_CHANNEL)
WBE_LABEL(WBE_CHANNEL_GLOBAL, WBE_CHANNEL)
WBE_LABEL(WBE_CHANNEL_USER, WBE_CHANNEL)
WBE_LABEL(WBE_CHANNEL_DEBUG, WBE_CHANNEL)

class Log {
public:

    Log() = default;
    virtual ~Log() {}

    /**
     * @brief Get the channel of the log.
     *
     * @return the channel ID.
     */
    virtual ChannelID get_channel() const = 0;

    /**
     * @brief Print a message.
     *
     * @param p_str The message to print.
     */
    virtual void message(const std::string& p_str) = 0;

    /**
     * @brief Print a warning.
     *
     * @param p_str The warning to print.
     */
    virtual void warning(const std::string& p_str) = 0;

    /**
     * @brief Print an error.
     *
     * @param p_str The error to print.
     */
    virtual void error(const std::string& p_str) = 0;
};

Log* wbe_console_log(ChannelID p_channel = WBE_CHANNEL_GLOBAL);

}

#endif
