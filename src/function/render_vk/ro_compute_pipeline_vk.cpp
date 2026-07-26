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
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "utils/defs.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROComputePipelineVK::~ROComputePipelineVK() {
    render_data->register_delayed_call(
        [render_data = render_data, compute_pipeline = compute_pipeline]() {
            vkDestroyPipeline(render_data->logic_device, compute_pipeline, render_data->allocation_callbacks);
        },
        render_data->frames_in_flight);
}

ROComputePipelineVK::ROComputePipelineVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data) {
    auto shader_stage = WBE_REQUIRED(p_builder.shader_stage);
    VkPipelineShaderStageCreateInfo shader_stage_create_info{};
    shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_create_info.module = shader_stage.shader->get_shader_module();
    shader_stage_create_info.pName = shader_stage.entry_function.c_str();
    VkComputePipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.stage = shader_stage_create_info;
    if (vkCreateComputePipelines(
            render_data->logic_device, VK_NULL_HANDLE, 1, &create_info, render_data->allocation_callbacks, &compute_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline.");
    }
}

} // namespace WhiteBirdEngine
