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
#ifndef WBE_FILE_RENDERER_SOFTWARE
#define WBE_FILE_RENDERER_SOFTWARE

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render_software/render_software_defs.hh"
#include "function/render_software/render_stage_software.hh"
#include <algorithm>
#include <string>
namespace WhiteBirdEngine {

/**
 * @class RendererSoftware
 * @brief Software renderer.
 */
class RendererSoftware final : public IRenderer {
public:

    virtual ~RendererSoftware() override;
    RendererSoftware(const RendererSoftware&) = delete;
    RendererSoftware(RendererSoftware&&) = delete;
    RendererSoftware& operator=(const RendererSoftware&) = delete;
    RendererSoftware& operator=(RendererSoftware&&) = delete;

    RendererSoftware(Ref<RenderAllocator> p_allocator);

    virtual uint32_t get_worker_nums() override {
        // Leave one thread for logic and one thread for worker thread.
        return std::max(std::thread::hardware_concurrency() - 2, 1U);
    }
    virtual void tick(double p_delta) override;
    virtual Ref<RenderDatabase> get_render_database() override;
    virtual void activate_render_graph(const std::string& p_pipeline_name) override;
    virtual void update_static_opaque_triangles() override;
    virtual void add_job(std::function<void()> p_job) override;

private:
    Vector<Ref<RenderStageSoftware>> render_stages;

    BaseRenderDataSoftware base_render_data;
};
} // namespace WhiteBirdEngine

#endif
