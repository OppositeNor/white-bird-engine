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
#ifndef WBE_FILE_RENDER_TASK_CREATE_UTILS_VK_HH
#define WBE_FILE_RENDER_TASK_CREATE_UTILS_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @brief Get a compiled compute pipeline for a render task's create().
 * @param p_render_data Render data whose RenderDatabase holds the pipeline.
 * @param p_task_name Task class name used in error messages.
 * @param p_pipeline_key Resource key the pipeline is compiled under.
 * @throws std::runtime_error If the render data, its allocator, or its RenderDatabase is unavailable, or no compute pipeline
 * is compiled under p_pipeline_key.
 */
Ref<ROComputePipelineVK> acquire_task_compute_pipeline_vk(
    const BaseRenderDataVK* p_render_data, std::string_view p_task_name, std::string_view p_pipeline_key);

/**
 * @brief Get a compiled graphics pipeline for a render task's create().
 * @param p_render_data Render data whose RenderDatabase holds the pipeline.
 * @param p_task_name Task class name used in error messages.
 * @param p_pipeline_key Resource key the pipeline is compiled under.
 * @throws std::runtime_error If the render data, its allocator, or its RenderDatabase is unavailable, or no graphics
 * pipeline is compiled under p_pipeline_key.
 */
Ref<ROGraphicsPipelineVK> acquire_task_graphics_pipeline_vk(
    const BaseRenderDataVK* p_render_data, std::string_view p_task_name, std::string_view p_pipeline_key);

} // namespace WhiteBirdEngine

#endif
