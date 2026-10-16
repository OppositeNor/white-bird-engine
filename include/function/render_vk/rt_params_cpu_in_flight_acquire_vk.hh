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
#ifndef WBE_FILE_RT_PARAMS_CPU_IN_FLIGHT_ACQUIRE_VK_HH
#define WBE_FILE_RT_PARAMS_CPU_IN_FLIGHT_ACQUIRE_VK_HH

#include "core/reflection/reflection_defs.hh"
#include <string>

namespace WhiteBirdEngine {

/**
 * @class RTParamsCPUInFlightAcquireVK
 * @brief Recipe parameters of RTCPUInFlightAcquireVK. Copyable and movable.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RTParamsCPUInFlightAcquireVK {
    /**
     * @brief Render-graph resource id of the source image view; it is also the event id of every acquisition.
     */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string image_view;
};

} // namespace WhiteBirdEngine

#endif
