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

#include "core/memory/reference_strong.hh"
#include <string>
namespace WhiteBirdEngine {

using RenderData = void;

/**
 * @class RenderPass
 * @brief Render pass interface.
 *
 */
class RenderPass {
public:
    RenderPass() = default;
    virtual ~RenderPass() {}

    /**
     * @brief Get the name of the pass.
     */
    virtual const std::string& get_pass_name() = 0;

    /**
     * @brief Setup the render pass.
     *
     * @param p_input The input to the setup process.
     * @return The output of the setup process.
     */
    virtual Ref<RenderData> setup(Ref<RenderData> p_input) = 0;

    /**
     * @brief Involke the render pass.
     *
     * @param p_input The input to the render pass.
     * @return The output of the render pass.
     */
    virtual Ref<RenderData> involke(Ref<RenderData> p_input) = 0;
};

}

#endif
