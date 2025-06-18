#ifndef __CORE_UTILS_H__
#define __CORE_UTILS_H__

#include "global/global.h"
#include "logging/log.h"

#include <memory>
namespace WhiteBirdEngine {

inline Log* wbe_log(ChannelID p_channel) {
    return Global::get_singleton()->stdio_logging_manager->get_log(p_channel);
}

inline const std::string& get_label_name(ChannelID p_channel) {
    return Global::get_singleton()->game_metadata->get_channel_metadata()[p_channel];
}

}

#endif
