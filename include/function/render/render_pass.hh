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
#ifndef __WBE_RENDER_PASS_HH__
#define __WBE_RENDER_PASS_HH__

#include <cstddef>
#include <string>
namespace WhiteBirdEngine {

/**
 * @class RenderPipeline
 * @brief Render pipeline interface.
 *
 */
class RenderPass {
public:
    RenderPass() = default;
    virtual ~RenderPass() {}

    /**
     * @brief Get the name of the pipeline.
     */
    virtual const std::string& get_pipeline_name() = 0;

    /**
     * @brief Setup the render pipline. Will be called once after the render tasks are added.
     */
    virtual void setup() = 0;

    /**
     * @brief Perform the render pipeline. Will be called every render tick.
     *
     * @param p_delta The time lasted since the last call to this function.
     */
    virtual void perform(double p_delta) = 0;

    /**
     * @brief Preserve a number of render tasks to be added.
     *
     * @param p_num The number of tasks to be preserved.
     */
    virtual void preserve(size_t p_num) = 0;
};

}

#endif
