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
#ifndef __WBE_LOG_STREAM_HH__
#define __WBE_LOG_STREAM_HH__

#include "log.hh"
#include <ostream>

namespace WhiteBirdEngine {

/**
 * @class LogStream
 *
 * @brief Log the message to a std::ostream
 */
class LogStream : public Log {
public:

    /**
     * @brief Constructor.
     *
     * @param p_channel_id The channel id.
     * @param p_ostream The output stream.
     */
    LogStream(ChannelID p_channel_id, std::ostream& p_ostream)
        : Log(), ostream(&p_ostream), channel_id(p_channel_id) {}
    /**
     * @brief Constructor. The channel ID will be WBE_CHANNEL_UNKNOWN.
     *
     * @param p_ostream The output stream.
     */
    LogStream(std::ostream& p_ostream)
        : Log(), ostream(&p_ostream), channel_id(WBE_CHANNEL_UNKNOWN) {}
    virtual ~LogStream() override {}
    LogStream(const LogStream &) = delete;
    LogStream(LogStream &&) = delete;
    LogStream &operator=(const LogStream &) = delete;
    LogStream &operator=(LogStream &&) = delete;

    virtual ChannelID get_channel() const override {
        return channel_id;
    }

    virtual void message(const std::string& p_str) override;

    virtual void warning(const std::string& p_str) override;

    virtual void error(const std::string& p_str) override;

private:
    std::ostream* ostream;
    ChannelID channel_id;
};

}

#endif
