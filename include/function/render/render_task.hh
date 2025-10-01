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
#ifndef __WBE_RENDER_TASK_HH__
#define __WBE_RENDER_TASK_HH__

#include <string>
#include <vector>
namespace WhiteBirdEngine {

/**
 * @class RenderTask
 * @brief A render task.
 *
 */
class RenderTask {
public:
    RenderTask() = default;
    virtual ~RenderTask() {}

    /**
     * @brief Get the dependencies of resources.
     *
     * @return The ids of the resources.
     */
    virtual std::vector<std::string> get_resource_dependencies() const = 0;

    /**
     * @brief Has the task started running.
     *
     * @param p_tick_flag Changes every tick, for checking if a new tick has started.
     * @return True if the task started, false otherwise.
     */
    virtual bool has_started(bool p_tick_flag) const = 0;
};

}

#endif
