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
#include "function/render/render_database.hh"
#include "function/render/renderer.hh"
namespace WhiteBirdEngine {

/**
 * @class RendererDummy
 * @brief A dummy renderer that omits all operations.
 *
 */
class RendererDummy final : public Renderer {
public:
    RendererDummy(RenderAllocator* p_allocator)
        : dummy_database(make_ref<RenderDatabase>(p_allocator, p_allocator)) {}
    
    virtual ~RendererDummy() override {}

    virtual void tick(double p_delta) override {}

    virtual Ref<RenderDatabase> get_render_database() override {
        // Because of RAII, we have to return a valid database since the caller is going to assume
        // this is valid.
        return dummy_database;
    }

    virtual void register_pipeline(const std::string& p_pipeline_name, Ref<RendTaskInfo> p_root_task) override {}

    virtual void activate_pipeline(const std::string& p_pipeline_name) override {}

    virtual void update_static_opaque_triangles() override {}

    virtual void pool_events(bool& p_should_close, Ref<InputManager> p_input_manager) override {}
private:
    Ref<RenderDatabase> dummy_database;
};

}



#endif
