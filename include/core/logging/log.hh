#ifndef __WBE_LOG_H__
#define __WBE_LOG_H__

#include "utils/defs.h"
#include "utils/utils.h"
#include <string>
namespace WhiteBirdEngine {

using ChannelID = HashCode;

// default labels
WBE_DEFINE_LABEL(WBE_CHANNEL_UNKNOWN)
WBE_DEFINE_LABEL(WBE_CHANNEL_GLOBAL)
WBE_DEFINE_LABEL(WBE_CHANNEL_USER)

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
