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
private:
    struct RenderTaskDummy final : public RenderTask {
        virtual void init() override {
        }

        virtual void perform(double p_delta) override {
            (void)p_delta;
        }
    };

public:
    RendererDummy(RenderAllocator* p_allocator)
        : allocator(p_allocator), dummy_database(make_ref<RenderDatabase>(p_allocator, p_allocator)) {
    }

    virtual ~RendererDummy() override = default;
    WBE_R6_NDCD_DELETE_COPY_MOVE(RendererDummy)

    virtual void tick(double p_delta) override {
        (void)p_delta;
    }

    virtual Ref<RenderDatabase> get_render_database() override {
        // Because of RAII, we have to return a valid database since the caller
        // is going to assume this is valid.
        return dummy_database;
    }

    virtual Unique<RenderTask> compile_graphics_pipeline(
        const std::string& p_resource_key, std::span<const RenderUniformBinding> p_uniform_bindings = {}) override {
        (void)p_resource_key;
        (void)p_uniform_bindings;
        return make_unique<RenderTaskDummy>(allocator);
    }

    virtual void activate_pipeline(const std::string& p_pipeline_name) override {
        (void)p_pipeline_name;
    }

    virtual void update_static_opaque_triangles() override {
    }

    virtual void add_job(std::function<void()> p_job) override {
        (void)p_job;
    }

private:
    RenderAllocator* allocator = nullptr;
    Ref<RenderDatabase> dummy_database;
};

} // namespace WhiteBirdEngine

#endif
