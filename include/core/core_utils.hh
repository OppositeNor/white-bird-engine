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
#ifndef __CORE_UTILS_HH__
#define __CORE_UTILS_HH__

#include "core/engine_core.hh"
#include "logging/log.hh"

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
