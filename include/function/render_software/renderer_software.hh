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

#include "function/render/i_renderer.hh"
#include "function/render/render_database.hh"
#include <string>
namespace WhiteBirdEngine {

/**
 * @class RendererSoftware
 * @brief Software renderer.
 */
class RendererSoftware final : public IRenderer {
public:

    RendererSoftware();
    virtual ~RendererSoftware() override;
    RendererSoftware(const RendererSoftware&) = delete;
    RendererSoftware(RendererSoftware&&) = delete;
    RendererSoftware& operator=(const RendererSoftware&) = delete;
    RendererSoftware& operator=(RendererSoftware&&) = delete;

    virtual void tick(double p_delta) override;
    virtual Ref<RenderDatabase> get_render_database() override;
    virtual void activate_render_graph(const std::string& p_pipeline_name) override;
    virtual void update_static_opaque_triangles() override;
    virtual void add_job(std::function<void()> p_job) override;
};
} // namespace WhiteBirdEngine

#endif
