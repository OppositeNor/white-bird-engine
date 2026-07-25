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
#ifndef WBE_FILE_RENDERER_HH
#define WBE_FILE_RENDERER_HH

#include "core/job/job_handler.hh"
#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/render_database.hh"
#include "utils/defs.hh"
#include "utils/interface/i_singleton.hh"
#include <cstdint>
namespace WhiteBirdEngine {

WBE_LABEL(WBE_CHANNEL_RENDER, WBE_CHANNEL);

/**
 * @class IRenderer
 * @brief IRenderer interface for different types of renderers, usually refer
 * to the rendering API.
 *
 */
class IRenderer : public ISingleton<IRenderer>, public JobHandler {
public:
    WBE_R6_DELETE_COPY_MOVE_OVERRIDE(IRenderer)

    /**
     * @brief Called every frame.
     *
     * @param p_delta The difference between the last two calls of this
     * function.
     */
    virtual void tick(double p_delta) = 0;

    /**
     * @brief Get number of workers working on rendering.
     *
     * @return The number of workers working on rendering.
     */
    virtual uint32_t get_worker_nums() = 0;

    /**
     * @brief Get the instance of the render database.
     */
    virtual Ref<RenderDatabase> get_render_database() = 0;

    /**
     * @brief Activate a pipeline.
     *
     * @param p_pipeline_name The name of the pipeline to activate.
     */
    virtual void activate_render_graph(std::string_view p_pipeline_name) = 0;

    /**
     * @brief Update the static opaque triangles data.
     */
    virtual void update_static_opaque_triangles() = 0;
};

} // namespace WhiteBirdEngine
#endif
