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
#ifndef __WBE_RENDER_PIPELINE_DUMMY_HH__
#define __WBE_RENDER_PIPELINE_DUMMY_HH__

#include "function/render/render_pipeline.hh"
#include <string>
namespace WhiteBirdEngine {

/**
 * @class RenderPipelineDummy
 * @brief A render pipeline that omits all operations.
 *
 */
class RenderPipelineDummy : public RenderPipeline {
public:
    RenderPipelineDummy() = default;
    virtual ~RenderPipelineDummy() {}
    RenderPipelineDummy(const RenderPipelineDummy&) = delete;
    RenderPipelineDummy(RenderPipelineDummy&&) = delete;
    RenderPipelineDummy& operator=(const RenderPipelineDummy&) = delete;
    RenderPipelineDummy& operator=(RenderPipelineDummy&&) = delete;


    virtual const std::string& get_pipeline_name() override { 
        static std::string dummy_pipeline_name = "dummy_pipeline";
        return dummy_pipeline_name;
    }
    virtual void setup() override {}
    virtual void perform(double p_delta) override {}
    virtual void preserve(size_t p_num) override {}
    virtual void add_render_task(Ref<RenderTask> p_render_task) override {}
};

}

#endif
