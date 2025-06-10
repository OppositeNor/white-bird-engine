#ifndef __CORE_UTILS_H__
#define __CORE_UTILS_H__

#include "global/global.h"
#include "logging/log.h"

#include <memory>
namespace WhiteBirdEngine {

template <ChannelID CHANNEL>
inline Log* wbe_log() {
    return Global::get_singleton()->stdio_logging_manager->get_log<CHANNEL>();
}

}

#endif
