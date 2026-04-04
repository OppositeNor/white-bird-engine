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
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_render_pass_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROGraphicsPipelineVK::Config& ROGraphicsPipelineVK::Config::build_pipeline(
    const BaseRenderDataVK* p_render_data,
    const std::string& p_res_key,
    Ref<RORenderPassVK> p_render_pass) {
    auto resource = ResourceManager::get_singleton()->get_resource<RUGraphicsPipeline>(p_res_key);
    shader.set_shaders(p_render_data, resource->vertex_stage, resource->fragment_stage,
                       std::string(resource->vertex_stage_entry.get()),
                       std::string(resource->fragment_stage_entry.get()));
    vertex_input
        .add_binding(VERTEX_3D_TEXTURED_INPUT_BINDING_DESCRIPTION)
        .add_attributes(VERTEX_3D_TEXTURED_INPUT_ATTRIBUTE_DESCRIPTION);
    // TODO: Make viewport and sissors configurable.
    viewport.add_viewport(p_render_data->viewport, p_render_data->scissor);
    // TODO: Setup pipeline layout from shaders' metadata.
    render_pass = p_render_pass;
    depth_test.set_depth_test(resource->depth_test);
    depth_test.set_depth_write(resource->depth_write);
    // TODO: Make color blend and dynamic state configurable.
    color_blend.add_default_color_attachment();
    dynamic_state.add_default_dynamic_states();
    return *this;
}

ROGraphicsPipelineVK::ROGraphicsPipelineVK(const BaseRenderDataVK* p_render_data, const Config& p_config)
    : RenderObjectVK(p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Render data is nullptr.");
    }
    init(p_config);
}

ROGraphicsPipelineVK::~ROGraphicsPipelineVK() {
    // Deallocate resource after frames in flight.
    render_data->register_delayed_call([render_data = render_data, graphics_pipeline = graphics_pipeline]() {
        vkDestroyPipeline(render_data->logic_device, graphics_pipeline, render_data->allocation_callbacks);
    }, render_data->frames_in_flight);
}

VkPipelineShaderStageCreateInfo ROGraphicsPipelineVK::get_shader_stage_create_info(VkShaderStageFlagBits p_stage_flag,
                                                                                   const Config::ShaderConfig::ShaderStage& p_stage) {
    VkPipelineShaderStageCreateInfo shader_stage_create_info{};
    shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_info.stage = p_stage_flag;
    shader_stage_create_info.module = p_stage.shader->get_shader_module();
    shader_stage_create_info.pName = p_stage.entry_function.c_str();
    return shader_stage_create_info;
}

void ROGraphicsPipelineVK::init(const Config& p_config) {
    // Gather shader stages.
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    shader_stages.reserve(4); // We can have a maximum of 4 shader stages (vertex, TSC, TSE, geometry, fragment)
    shader_stages.push_back(get_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, WBE_REQUIRED(p_config.shader.vertex_shader)));
    shader_stages.push_back(get_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, WBE_REQUIRED(p_config.shader.fragment_shader)));
    // TODO: TSC, TSE and geometry shader
    exposed_fields.vertex_shader = WBE_REQUIRED(p_config.shader.vertex_shader).shader;
    exposed_fields.fragment_shader = WBE_REQUIRED(p_config.shader.fragment_shader).shader;

    // Setup create information for states.
    PipelineStateCreateInfo state_create_info;
    setup_vertex_input_state_create_info(p_config, state_create_info);
    setup_input_assembly_state_create_info(p_config, state_create_info);
    setup_tessellation_state_create_info(p_config, state_create_info);
    setup_viewport_state_create_info(p_config, state_create_info);
    setup_rasterization_state_create_info(p_config, state_create_info);
    setup_mulitsample_state_create_info(p_config, state_create_info);
    setup_depth_stencil_state_create_info(p_config, state_create_info);
    setup_color_blend_state_create_info(p_config, state_create_info);
    setup_dynamic_state_create_info(p_config, state_create_info);

    VkGraphicsPipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = shader_stages.size();
    create_info.pStages = shader_stages.data();
    create_info.pVertexInputState = &(state_create_info.vertex_input_state);
    create_info.pInputAssemblyState = &(state_create_info.input_assembly_state);
    if (state_create_info.tessellation_state.has_value()) {
        create_info.pTessellationState = &(state_create_info.tessellation_state.value());
    }
    create_info.pViewportState = &(state_create_info.viewport_state);
    create_info.pRasterizationState = &(state_create_info.rasterization_state);
    create_info.pMultisampleState = &(state_create_info.multisample_state);
    create_info.pDepthStencilState = &(state_create_info.depth_stencil_state);
    create_info.pColorBlendState = &(state_create_info.color_blend_state);
    create_info.pDynamicState = &(state_create_info.dynamic_state);
    create_info.layout = WBE_REQUIRED(p_config.pipeline_layout).get_pipeline_layout();
    create_info.renderPass = WBE_REQUIRED(p_config.render_pass).get_render_pass();
    create_info.subpass = p_config.subpass_index;

    exposed_fields.pipeline_layout = p_config.pipeline_layout;
    exposed_fields.render_pass = p_config.render_pass;

    if (vkCreateGraphicsPipelines(
        render_data->logic_device, VK_NULL_HANDLE, 1, &create_info, render_data->allocation_callbacks, &graphics_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline.");
    }
    config = p_config;
}

void ROGraphicsPipelineVK::setup_viewport_state_create_info(const Config& p_config,
                                                          PipelineStateCreateInfo& p_pipeline_create_infos) {
    const auto& viewport_config = p_config.viewport;
    VkPipelineViewportStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    // Setup viewports.
    create_info.viewportCount = viewport_config.viewports.size();
    create_info.pViewports = viewport_config.viewports.data();
    // Setup scissors.
    create_info.scissorCount = viewport_config.scissors.size();
    create_info.pScissors = viewport_config.scissors.data();
    p_pipeline_create_infos.viewport_state = create_info;
}

void ROGraphicsPipelineVK::setup_vertex_input_state_create_info(const Config& p_config,
                                                              PipelineStateCreateInfo& p_pipeline_create_infos) {
    const auto& vertex_input_config = p_config.vertex_input;
    VkPipelineVertexInputStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    // Setup bindings.
    create_info.vertexBindingDescriptionCount = vertex_input_config.bindings.size();
    create_info.pVertexBindingDescriptions = vertex_input_config.bindings.data();
    // Setup attributes.
    create_info.vertexAttributeDescriptionCount = vertex_input_config.attributes.size();
    create_info.pVertexAttributeDescriptions = vertex_input_config.attributes.data();
    p_pipeline_create_infos.vertex_input_state = create_info;
}

void ROGraphicsPipelineVK::setup_input_assembly_state_create_info(const Config& p_config,
                                                                PipelineStateCreateInfo& p_pipeline_create_infos) {
    const auto& input_assembly_config = p_config.input_assembly;
    VkPipelineInputAssemblyStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    create_info.topology = input_assembly_config.primitive_topology;
    create_info.primitiveRestartEnable = input_assembly_config.primitive_restart_enabled;
    p_pipeline_create_infos.input_assembly_state = create_info;
}

void ROGraphicsPipelineVK::setup_tessellation_state_create_info(const Config& p_config,
                                                              PipelineStateCreateInfo& p_pipeline_create_infos) {
    // TODO
}

void ROGraphicsPipelineVK::setup_rasterization_state_create_info(const Config& p_config,
                                                               PipelineStateCreateInfo& p_pipeline_create_infos) {
    const auto& rasterization_config = p_config.rasterization;
    VkPipelineRasterizationStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    create_info.depthClampEnable = rasterization_config.depth_clamp_enabled;
    create_info.rasterizerDiscardEnable = VK_FALSE;
    create_info.polygonMode = rasterization_config.polygon_mode;
    create_info.cullMode = rasterization_config.cull_mode;
    create_info.frontFace = rasterization_config.front_face;
    create_info.depthBiasEnable = VK_FALSE;
    create_info.lineWidth = 1.0F;
    p_pipeline_create_infos.rasterization_state = create_info;
}

void ROGraphicsPipelineVK::setup_mulitsample_state_create_info(const Config& p_config,
                                                              PipelineStateCreateInfo& p_pipeline_create_infos) {
    (void)p_config; // TODO: Multi-sample config
    VkPipelineMultisampleStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    create_info.sampleShadingEnable = VK_FALSE;
    create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    p_pipeline_create_infos.multisample_state = create_info;
}

void ROGraphicsPipelineVK::setup_depth_stencil_state_create_info(const Config& p_config,
                                                               PipelineStateCreateInfo& p_pipeline_create_infos) {
    const auto& depth_stencil_config = p_config.depth_test;
    VkPipelineDepthStencilStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    create_info.depthTestEnable = depth_stencil_config.depth_test_enabled;
    create_info.depthWriteEnable = depth_stencil_config.depth_write_enabled;
    create_info.depthCompareOp = depth_stencil_config.depth_compare_op;
    create_info.stencilTestEnable = VK_FALSE;
    p_pipeline_create_infos.depth_stencil_state = create_info;
}

void ROGraphicsPipelineVK::setup_color_blend_state_create_info(const Config& p_config,
                                                             PipelineStateCreateInfo& p_pipeline_create_infos)  {
    const auto& color_blend_config = p_config.color_blend;
    VkPipelineColorBlendStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    create_info.logicOpEnable = color_blend_config.logic_op_enabled;
    create_info.logicOp = color_blend_config.logic_op;
    create_info.attachmentCount = color_blend_config.attachments.size();
    create_info.pAttachments = color_blend_config.attachments.data();
    p_pipeline_create_infos.color_blend_state = create_info;
}

void ROGraphicsPipelineVK::setup_dynamic_state_create_info(const Config& p_config,
                                                         PipelineStateCreateInfo& p_pipeline_create_infos) {
    const auto& dynamic_state_config = p_config.dynamic_state;
    VkPipelineDynamicStateCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    create_info.dynamicStateCount = dynamic_state_config.dynamic_states.size();
    create_info.pDynamicStates = dynamic_state_config.dynamic_states.data();
    p_pipeline_create_infos.dynamic_state = create_info;
}

} // namespace WhiteBirdEngine
