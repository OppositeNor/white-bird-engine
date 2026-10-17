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
#include "function/render_vk/render_task_create_utils_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_database.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "generated/type_uuid.gen.hh"
#include <format>
#include <stdexcept>
#include <string_view>

namespace WhiteBirdEngine {

namespace {

template <typename TPipeline>
Ref<TPipeline> acquire_task_pipeline(
    const BaseRenderDataVK* p_render_data, std::string_view p_task_name, std::string_view p_pipeline_key, std::string_view p_kind) {
    if (p_render_data == nullptr || p_render_data->allocator == nullptr) {
        throw std::runtime_error(std::format("{}: render data is unavailable.", p_task_name));
    }
    if (p_render_data->render_database == nullptr) {
        throw std::runtime_error(std::format("{}: the render database is unavailable.", p_task_name));
    }
    Ref<TPipeline> pipeline =
        p_render_data->render_database->get_compiled_render_resource(p_pipeline_key, TypeUUID<TPipeline>::UUID).template dynamic_cast_ref<TPipeline>();
    if (pipeline == MEM_NULL) {
        throw std::runtime_error(std::format("{}: no {} pipeline is compiled under '{}'.", p_task_name, p_kind, p_pipeline_key));
    }
    return pipeline;
}

} // namespace

Ref<ROComputePipelineVK> acquire_task_compute_pipeline_vk(
    const BaseRenderDataVK* p_render_data, std::string_view p_task_name, std::string_view p_pipeline_key) {
    return acquire_task_pipeline<ROComputePipelineVK>(p_render_data, p_task_name, p_pipeline_key, "compute");
}

Ref<ROGraphicsPipelineVK> acquire_task_graphics_pipeline_vk(
    const BaseRenderDataVK* p_render_data, std::string_view p_task_name, std::string_view p_pipeline_key) {
    return acquire_task_pipeline<ROGraphicsPipelineVK>(p_render_data, p_task_name, p_pipeline_key, "graphics");
}

} // namespace WhiteBirdEngine
