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
#ifndef WBE_FILE_GRAPHICS_PIPELINE_COMPILER_VK_HH
#define WBE_FILE_GRAPHICS_PIPELINE_COMPILER_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/render_vk_resource_unit.hh"
#include "function/render_vk/ro_render_pass_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <span>
#include <string>

namespace WhiteBirdEngine {

class RendererVK;

/**
 * @class GraphicsPipelineCompilerVKContext
 * @brief Runtime state required to compile a graphics pipeline resource.
 */
struct GraphicsPipelineCompilerVKContext {
    /**
     * @brief Allocator used for generated render graph descriptors.
     */
    RenderAllocator* allocator = nullptr;
    /**
     * @brief Vulkan renderer state used by render object builders.
     */
    const BaseRenderDataVK* render_data = nullptr;
    /**
     * @brief Registry used to reuse compiled render objects by resource key.
     */
    Ref<RenderDatabase> render_database = MEM_NULL;
    /**
     * @brief Render pass used by the generated graphics pipeline.
     */
    Ref<RORenderPassVK> render_pass = MEM_NULL;
    /**
     * @brief Renderer used to execute the generated task's render graph.
     */
    RendererVK* renderer = nullptr;
};

/**
 * @brief Compile a graphics pipeline resource into a runnable render task.
 *
 * The compiler reuses registered shaders, descriptor set layouts, pipeline
 * layouts, and graphics pipelines when matching entries already exist in the
 * render database. The returned task owns a one-task render graph and runs it
 * when perform() is called.
 *
 * @param p_context Runtime compilation context.
 * @param p_resource_key Key of the graphics pipeline resource.
 * @param p_pipeline_resource Loaded graphics pipeline resource unit.
 * @param p_uniform_bindings Caller-provided uniform field bindings.
 * @return A render task that runs the compiled graphics pipeline.
 */
Unique<RenderTask> compile_graphics_pipeline_task_vk(const GraphicsPipelineCompilerVKContext& p_context,
    const std::string& p_resource_key,
    RefRes<RUGraphicsPipeline> p_pipeline_resource,
    std::span<const RenderUniformBinding> p_uniform_bindings = {});

} // namespace WhiteBirdEngine

#endif