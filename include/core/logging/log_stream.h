#ifndef __WBE_LOG_STREAM_H__
#define __WBE_LOG_STREAM_H__

#include "log.h"
#include "utils/defs.h"
#include <ostream>

namespace WhiteBirdEngine {

/**
 * @class LogStream
 *
 * @brief Log the message to a std::ostream
 */
class LogStream : public Log {
public:

    LogStream(ChannelID p_channel_id, std::ostream& p_ostream)
        : Log(), ostream(&p_ostream), channel_id(p_channel_id) {}
    LogStream(std::ostream& p_ostream)
        : Log(), ostream(&p_ostream), channel_id(WBE_CHANNEL_UNKNOWN) {}
    virtual ~LogStream() override {}
    LogStream(const LogStream &) = delete;
    LogStream(LogStream &&) = delete;
    LogStream &operator=(const LogStream &) = delete;
    LogStream &operator=(LogStream &&) = delete;

    LogStream(std::ostream* p_ostream)
        : Log() {
        WBE_DEBUG_ASSERT(p_ostream != nullptr);
    }

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
