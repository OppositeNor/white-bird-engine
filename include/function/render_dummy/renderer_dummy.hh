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
#ifndef WBE_FILE_RENDERER_DUMMY_H
#define WBE_FILE_RENDERER_DUMMY_H
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "utils/defs.hh"
#include <functional>
#include <span>
#include <string>
namespace WhiteBirdEngine {

/**
 * @class RendererDummy
 * @brief A dummy renderer that omits all operations.
 *
 */
class RendererDummy final : public IRenderer {
public:
    RendererDummy(RenderAllocator* p_allocator) : dummy_database(make_ref<RenderDatabase>(p_allocator, p_allocator)) {
    }

    virtual ~RendererDummy() override = default;
    WBE_R6_NDCD_DELETE_COPY_MOVE(RendererDummy)

    virtual uint32_t get_worker_nums() override {
        return 1;
    }

    virtual void tick(double p_delta) override {
        (void)p_delta;
    }

    virtual Ref<RenderDatabase> get_render_database() override {
        // Because of RAII, we have to return a valid database since the caller
        // is going to assume this is valid.
        return dummy_database;
    }

    virtual void activate_render_graph(const std::string& p_pipeline_name) override {
        (void)p_pipeline_name;
    }

    virtual void update_static_opaque_triangles() override {
    }

    virtual void add_job(std::function<void()> p_job) override {
        (void)p_job;
    }

private:
    Ref<RenderDatabase> dummy_database;
};

} // namespace WhiteBirdEngine

#endif
