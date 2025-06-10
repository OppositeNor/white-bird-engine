#ifndef __WBE_LOG_STREAM_H__
#define __WBE_LOG_STREAM_H__

#include "log.h"
#include "utils/defs.h"
#include <ostream>

namespace WhiteBirdEngine {

/**
 * @class
 *
 * @brief Log the message to a std::ostream
 */
template <ChannelID CHANNEL = WBE_CHANNEL_UNKNOWN>
class LogStream : public Log {
public:

    LogStream(std::ostream& p_ostream)
        : Log(), ostream(&p_ostream) {}
    virtual ~LogStream() override {}
    LogStream(const LogStream &) = delete;
    LogStream(LogStream &&) = delete;
    LogStream &operator=(const LogStream &) = delete;
    LogStream &operator=(LogStream &&) = delete;

    LogStream(std::ostream* p_ostream)
        : Log() {
        WBE_DEBUG_ASSERT(p_ostream != nullptr);
    }

    virtual void message(const std::string& p_str) override {
        *ostream << "[" << WBE_LABEL_NAME(CHANNEL) << "] Message: " << p_str << std::endl;
    }

    virtual void warning(const std::string& p_str) override {
        *ostream << "[" << WBE_LABEL_NAME(CHANNEL) << "] Warning: " << p_str << std::endl;
    }

    virtual void error(const std::string& p_str) override {
        *ostream << "[" << WBE_LABEL_NAME(CHANNEL) << "] Error: " << p_str << std::endl;
    }

private:
    std::ostream* ostream;
};

}

#endif
