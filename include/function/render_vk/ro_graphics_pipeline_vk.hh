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
#ifndef __WBE_RO_GRAPHICS_PIPELINE_VK_HH__
#define __WBE_RO_GRAPHICS_PIPELINE_VK_HH__

#include "core/memory/reference_strong.hh"
#include "function/render/render_object.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ROGraphicsPipelineVK
 * @brief The graphics pipeline.
 *
 */
class ROGraphicsPipelineVK final : public RenderObject {
public:
    /**
     * @class PipelineConfigurations
     * @brief Pipeline configurations.
     *
     */
    struct Config {
        // Shader configuration. Manual setup needed!
        struct ShaderConfig {
            // Shader stage configurations.
            struct ShaderStage {
                // Shader stages bits.
                VkShaderStageFlagBits stage;
                // The entry function name of the shader.
                std::string entry_function = "main";
                // Shader instance.
                Ref<ROShaderVK> shader;
            };
            // Shader stages.
            std::vector<ShaderStage> shader_stages;
        };
        // Viewport configuration.
        struct ViewportStateConfig {
            // Viewports. Manual setup needed!
            std::vector<VkViewport> viewports;
            // Scissors. Manual setup needed!
            // A scissor clamps a rectangle region that the fragments will only be generated inside this region.
            std::vector<VkRect2D> scissors;
        };
        // Vertex input configurations.
        struct VertexInputConfig {
            // The vertex bindings. Manual setup needed!
            std::vector<VkVertexInputBindingDescription> bindings;
            // The vertex attributes. Manual setup needed!
            std::vector<VkVertexInputAttributeDescription> attributes;
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
            // If set to true, the fragments of depth over 1.0 or smaller than 0.0 won't be discarded but will be clamped.
            VkBool32 depth_clamp_enabled = VK_TRUE;
            // Polygon mode.
            VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
            // Cull mode. Default cull back faces.
            VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
            // Which direction is considered front, default to counter-clockwise.
            VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        };
        // Multisample configurations.
        struct MultisampleConfig {
            // TODO
        };
        // Depth test configurations.
        struct DepthTestConfig {
            // Depth test enabled.
            VkBool32 depth_test_enable = VK_TRUE;
            // Enable depth writing.
            VkBool32 depth_write_enable = VK_FALSE;
            // Compare operation.
            VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS;
        };
        // Color blend configurations.
        struct ColorBlendConfig {
            // Enable logic operations for the attachments.
            VkBool32 logic_op_enable = VK_FALSE;
            // Logic operation. Ignored if logic_op_enable is VK_FALSE.
            VkLogicOp logic_op;
            // Color blend attachments.
            std::vector<VkPipelineColorBlendAttachmentState> attachments {
                {
                    .blendEnable = VK_FALSE,
                    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
                }
            };
        };
        // Dynamic state configurations.
        struct DynamicStateConfig {
            // Dynamic states.
            std::vector<VkDynamicState> dynamic_states {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };
        };
        // The input shaders. Manual setup needed!
        ShaderConfig shader{};
        // Viewport state configuration.
        ViewportStateConfig viewport{};
        // Vertex input stage configurations.
        VertexInputConfig vertex_input{};
        // Input assembly stage configurations.
        InputAssemblyConfig input_assembly{};
        // Teselation stage configurations.
        std::optional<TesselationConfig> tesselation;
        // Rasterization stage configurations.
        RasterizationConfig rasterization{};
        // Multisample state configurations.
        MultisampleConfig multisample{};
        // Depth test configuration.
        DepthTestConfig depth_test{};
        // Color blend configuration.
        ColorBlendConfig color_blend{};
        // Dynamic state configuration.
        DynamicStateConfig dynamic_state{};
        // Pipeline layout. Manual setup needed!
        Ref<ROPipelineLayoutVK> pipeline_layout;
    };

    ~ROGraphicsPipelineVK();
    ROGraphicsPipelineVK(const ROGraphicsPipelineVK&) = delete;
    ROGraphicsPipelineVK(ROGraphicsPipelineVK&&) = delete;
    ROGraphicsPipelineVK& operator=(const ROGraphicsPipelineVK&) = delete;
    ROGraphicsPipelineVK& operator=(ROGraphicsPipelineVK&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_shaders The list of shaders used in the pipeline.
     */
    ROGraphicsPipelineVK(const BaseRenderDataVK* p_render_data, const Config& p_config);

    /**
     * @brief Get the type of the render object.
     */
    virtual std::string get_ro_type() const override {
        return "graphics_pipeline_vk";
    }

    /**
     * @brief Get the default configuration of the pipeline for rendering opaque objects.
     * Note that you still need to set the shaders, vertex input bindings and attributes yourself.
     *
     * @return The default configuration of the pipeline for rendering opaque objects.
     */
    Config get_default_render_opaque_configuration() {
        auto result = Config();
        result.viewport.viewports = { render_data->viewport };
        result.viewport.scissors = { render_data->scissor };
        return result;
    }

    /**
     * @brief Get the graphics pipeline instance.
     *
     * @return The graphics pipeline instance.
     */
    VkPipeline get_graphics_pipeline() const {
        return graphics_pipeline;
    }

private:

    const BaseRenderDataVK* render_data;

    VkPipeline graphics_pipeline;

    struct PipelineStateCreateInfo {
        VkPipelineViewportStateCreateInfo viewport_state;
        VkPipelineVertexInputStateCreateInfo vertex_input_state;
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
        std::optional<VkPipelineTessellationStateCreateInfo> tessellation_state;
        VkPipelineRasterizationStateCreateInfo rasterization_state;
        VkPipelineMultisampleStateCreateInfo multisample_state;
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
        VkPipelineColorBlendStateCreateInfo color_blend_state;
        VkPipelineDynamicStateCreateInfo dynamic_state;
    };

    void init(const Config& p_config);

    void setup_viewport_state_create_info(const Config& p_config,
                                          PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_vertex_input_state_create_info(const Config& p_config,
                                              PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_input_assembly_state_create_info(const Config& p_config,
                                                PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_tessellation_state_create_info(const Config& p_config,
                                              PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_rasterization_state_create_info(const Config& p_config,
                                              PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_mulitsample_state_create_info(const Config& p_config,
                                             PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_depth_stencil_state_create_info(const Config& p_config,
                                               PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_color_blend_state_create_info(const Config& p_config,
                                               PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_dynamic_state_create_info(const Config& p_config,
                                         PipelineStateCreateInfo& p_pipeline_create_infos);
};

}

#endif
