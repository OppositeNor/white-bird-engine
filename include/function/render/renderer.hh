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
#ifndef __WBE_RENDERER_HH__
#define __WBE_RENDERER_HH__

#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/input_manager/input_manager.hh"
#include "function/render/render_database.hh"
#include "function/render/render_task.hh"
#include "utils/interface/singleton.hh"
namespace WhiteBirdEngine {

WBE_LABEL(WBE_CHANNEL_RENDER, WBE_CHANNEL);

using RenderAllocator = HeapAllocatorDefault;

/**
 * @class Renderer
 * @brief Renderer interface for different types of renderers, usually refer
 * to the rendering API.
 *
 */
class Renderer : public Singleton<Renderer> {
public:
    Renderer() = default;
    virtual ~Renderer() {}

    /**
     * @brief Called every frame.
     *
     * @param p_delta The difference between the last two calls of this function.
     */
    virtual void tick(double p_delta) = 0;

    /**
     * @brief Get the instance of the render database.
     */
    virtual Ref<RenderDatabase<RenderAllocator>> get_render_database() = 0;

    /**
     * @brief Register a graphics pipeline. If a pipeline is already registered, it will be overritten.
     *
     * @todo Test
     * @param p_pipeline_name The name of the pipeline to register.
     * @param p_root_task The root render task of the pipeline.
     */
    virtual void register_pipeline(const std::string& p_pipeline_name, Ref<RendTaskInfo> p_root_task) = 0;

    /**
     * @brief Activate a pipeline.
     *
     * @todo Test
     * @param p_pipeline_name The name of the pipeline to activate.
     */
    virtual void activate_pipeline(const std::string& p_pipeline_name) = 0;

    /**
     * @brief Update the static opaque triangles data.
     */
    virtual void update_static_opaque_triangles() = 0;

    /**
     * @brief Pool events.
     *
     * @param p_should_close Should the application be closed.
     * @param p_input_manager The input manager instance to be updated.
     */
    virtual void pool_events(bool& p_should_close, Ref<InputManager> p_input_manager) = 0;
protected:

private:
    void initialize_render_pass();

};

}

#endif
