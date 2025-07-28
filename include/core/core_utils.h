#ifndef __CORE_UTILS_H__
#define __CORE_UTILS_H__

#include "core/engine_core.h"
#include "logging/log.h"

namespace WhiteBirdEngine {

/**
 * @brief Get the name of the channel's label.
 *
 * @param p_channel The channel to get name from.
 * @return The name of the channel.
 */
inline const std::string& get_label_name(ChannelID p_channel) {
    return EngineCore::get_singleton()->game_metadata->get_channel_metadata()[p_channel];
}

}

#endif
