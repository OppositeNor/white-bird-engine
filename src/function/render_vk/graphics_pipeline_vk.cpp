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
#include "function/render_vk/graphics_pipeline_vk.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

GraphicsPipelineVK::GraphicsPipelineVK(const PipelineConfigurations& p_pipeline_configurations) {
    initialize(p_pipeline_configurations);
}

void GraphicsPipelineVK::initialize(const PipelineConfigurations& p_pipeline_configurations) {
    // Gather shader stages.
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    shader_stages.reserve(p_pipeline_configurations.shaders.size());
    for (auto& shader : p_pipeline_configurations.shaders) {
        shader_stages.push_back(shader.get_shader_stage_create_info());
    }
    // Setup create information for states.
    PipelineStateCreateInfo state_create_info;
    setup_vertex_input_state_create_info(p_pipeline_configurations, state_create_info);
    setup_input_assembly_state_create_info(p_pipeline_configurations, state_create_info);
    setup_tessellation_state_create_info(p_pipeline_configurations, state_create_info);
    setup_rasterization_state_create_info(p_pipeline_configurations, state_create_info);
    setup_depth_stencil_state_create_info(p_pipeline_configurations, state_create_info);
}

void GraphicsPipelineVK::setup_vertex_input_state_create_info(const PipelineConfigurations& p_pipeline_configurations,
                                                              PipelineStateCreateInfo& p_pipeline_create_infos) {
    auto& vertex_input_config = p_pipeline_configurations.vertex_input;
    VkPipelineVertexInputStateCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    // Setup bindings.
    create_info.vertexBindingDescriptionCount = vertex_input_config.bindings.size();
    create_info.pVertexBindingDescriptions = vertex_input_config.bindings.data();
    // Setup attributes.
    create_info.vertexAttributeDescriptionCount = vertex_input_config.attributes.size();
    create_info.pVertexAttributeDescriptions = vertex_input_config.attributes.data();
    p_pipeline_create_infos.vertex_input_state = create_info;
}

void GraphicsPipelineVK::setup_input_assembly_state_create_info(const PipelineConfigurations& p_pipeline_configurations,
                                                                PipelineStateCreateInfo& p_pipeline_create_infos) {
    auto& input_assembly_config = p_pipeline_configurations.input_assembly;
    VkPipelineInputAssemblyStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    create_info.topology = input_assembly_config.primitive_topology;
    create_info.primitiveRestartEnable = input_assembly_config.primitive_restart_enabled;
    p_pipeline_create_infos.input_assembly_state = create_info;
}

void GraphicsPipelineVK::setup_tessellation_state_create_info(const PipelineConfigurations& p_pipeline_configurations,
                                                              PipelineStateCreateInfo& p_pipeline_create_infos) {
    // TODO
}

void GraphicsPipelineVK::setup_rasterization_state_create_info(const PipelineConfigurations& p_pipeline_configurations,
                                                               PipelineStateCreateInfo& p_pipeline_create_infos) {
    auto& rasterization_config = p_pipeline_configurations.rasterization;
    VkPipelineRasterizationStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    create_info.depthClampEnable = rasterization_config.depth_clamp_enabled;
    create_info.rasterizerDiscardEnable = VK_FALSE;
    create_info.polygonMode = rasterization_config.polygon_mode;
    create_info.cullMode = rasterization_config.cull_mode;
    create_info.frontFace = rasterization_config.front_face;
    create_info.depthBiasEnable = VK_FALSE;
    create_info.lineWidth = 1.0f;
    p_pipeline_create_infos.rasterization_state = create_info;
}

void GraphicsPipelineVK::setup_mulitsample_state_create_info(const PipelineConfigurations& p_pipeline_configurations,
                                                              PipelineStateCreateInfo& p_pipeline_create_infos) {
    // TODO
}

void GraphicsPipelineVK::setup_depth_stencil_state_create_info(const PipelineConfigurations& p_pipeline_configurations,
                                                               PipelineStateCreateInfo& p_pipeline_create_infos) {
    auto& depth_stencil_config = p_pipeline_configurations.depth_test;
    VkPipelineDepthStencilStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    create_info.depthTestEnable = depth_stencil_config.depth_test_enable;
    create_info.depthWriteEnable = depth_stencil_config.depth_write_enable;
    create_info.depthCompareOp = depth_stencil_config.depth_compare_op;
    create_info.stencilTestEnable = VK_FALSE;
    p_pipeline_create_infos.depth_stencil_state = create_info;
}

}
