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

#include "core/memory/reference_strong.hh"
#include "function/render/render_pipeline.hh"
#include "function/render/shader.hh"
#include "utils/interface/singleton.hh"
#include <cstdint>
#include <string>
namespace WhiteBirdEngine {

using RenderID = uint32_t;

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
     * @brief Create a shader instance.
     *
     * @param p_shader_res The resource ID referencing the source of the shader.
     */
    virtual Ref<Shader> create_shader_instance(const std::string& p_shader_res) = 0;

    /**
     * @brief Get the render pipeline of a specific ID.
     *
     * @param p_pipeline_id The ID of the render pipeline to get.
     */
    virtual Ref<RenderPipeline> get_render_pipeline(const std::string& p_pipeline_id) = 0;
};

}

#endif
