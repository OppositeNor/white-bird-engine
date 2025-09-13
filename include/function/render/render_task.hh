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

namespace WhiteBirdEngine {

using RenderData = void*;

/**
 * @class RenderTask
 * @brief Render task for a render pipeline.
 *
 */
class RenderTask {
public:
    RenderTask();
    virtual ~RenderTask() {}

    /**
     * @brief Set up the render task.
     *
     * @param p_input The input to the setup process of this render task.
     * @return The result of the setup of this render task.
     */
    virtual RenderData* setup(RenderData* p_input) = 0;

    /**
     * @brief 
     *
     * @param p_input 
     * @return 
     */
    virtual RenderData* input(RenderData* p_input) = 0;

    /**
     * @brief Clean up the task.
     */
    virtual void cleanup() = 0;
};

}

#endif
