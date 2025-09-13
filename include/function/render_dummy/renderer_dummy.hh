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
#ifndef __WBE_RENDERER_DUMMY_H__
#define __WBE_RENDERER_DUMMY_H__
#include "core/memory/reference_strong.hh"
#include "function/render_dummy/render_pipeline_dummy.hh"
#include "function/render/renderer.hh"
#include "function/render_dummy/shader_dummy.hh"
#include "global/global.hh"
namespace WhiteBirdEngine {

/**
 * @class RendererDummy
 * @brief A dummy renderer that omits all operations.
 *
 */
class RendererDummy : public Renderer {
public:
    RendererDummy()
        : dummy_render_pipeline(make_ref<RenderPipelineDummy>(global_allocator())),
          dummy_shader(make_ref<ShaderDummy>(global_allocator())) {}
    
    virtual ~RendererDummy() override {}
    virtual Ref<Shader> create_shader_instance(const std::string& p_shader_res) override {
        return dummy_shader;
    }
    virtual Ref<RenderPipeline> get_render_pipeline(const std::string& p_pipeline_name) override {
        return dummy_render_pipeline;
    }
    virtual void add_render_pipeline(Ref<RenderPipeline> p_render_pipeline) override {}

private:
    Ref<RenderPipelineDummy> dummy_render_pipeline;
    Ref<ShaderDummy> dummy_shader;
};

}



#endif
