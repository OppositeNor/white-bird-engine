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
#ifndef __WBE_GRAPHICS_PIPELINE_VK_HH__
#define __WBE_GRAPHICS_PIPELINE_VK_HH__

#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/shader_pipeline_vk.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class GraphicsPipelineVK {
public:
    template <typename T>
    using vector = RendererVK::vector<T>;

    /**
     * @class PipelineConfigurations
     * @brief Pipeline configurations.
     *
     */
    struct PipelineConfigurations {
        // Vertex input configurations.
        struct VertexInputConfig {
            // The vertex bindings.
            vector<VkVertexInputBindingDescription> bindings;
            // The vertex attributes.
            vector<VkVertexInputAttributeDescription> attributes;
        };
        // Input assembly configurations.
        struct InputAssemblyConfig {
            // The topology used in the input assembly stage.
            VkPrimitiveTopology primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            // Enable restart for strip and fan primitive topology.
            VkBool32 primitive_restart_enabled = VK_FALSE;
        };
        // Teselation configurations.
        struct TesselationConfig {
            // TODO
        };
        // Rasterization configuraion.
        struct RasterizationConfig {
            // Enables depth clamping.
            VkBool32 depth_clamp_enabled = VK_TRUE;
            // Polygon mode.
            VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
            // Cull mode. Default cull back faces.
            VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
            // Which direction is considered front, default to counter-clockwise.
            VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        };
        // The input shaders.
        vector<ShaderPipelineVK> shaders;
        // Vertex input stage configurations.
        VertexInputConfig vertex_input;
        // Input assembly stage configurations.
        InputAssemblyConfig input_assembly;
        // Teselation stage configurations.
        std::optional<TesselationConfig> tesselation;
        // Rasterization stage configurations.
        RasterizationConfig rasterization;
    };

    ~GraphicsPipelineVK();
    GraphicsPipelineVK(const GraphicsPipelineVK&) = delete;
    GraphicsPipelineVK(GraphicsPipelineVK&&) = delete;
    GraphicsPipelineVK& operator=(const GraphicsPipelineVK&) = delete;
    GraphicsPipelineVK& operator=(GraphicsPipelineVK&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_shaders The list of shaders used in the pipeline.
     */
    GraphicsPipelineVK(const PipelineConfigurations& p_pipeline_configurations);

private:

    struct PipelineStateCreateInfo {
        VkPipelineVertexInputStateCreateInfo vertex_input_state;
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
        std::optional<VkPipelineTessellationStateCreateInfo> tessellation_state;
        VkPipelineRasterizationStateCreateInfo rasterization_state;
    };

    void initialize(const PipelineConfigurations& p_pipeline_configurations);
    void setup_vertex_input_state_create_info(const PipelineConfigurations& p_pipeline_configurations,
                                              PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_input_assembly_state_create_info(const PipelineConfigurations& p_pipeline_configurations,
                                                PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_tessellation_state_create_info(const PipelineConfigurations& p_pipeline_configurations,
                                              PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_rasterization_state_create_info(const PipelineConfigurations& p_pipeline_configurations,
                                              PipelineStateCreateInfo& p_pipeline_create_infos);
};

}

#endif
