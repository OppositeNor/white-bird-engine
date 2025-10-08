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
#include "function/render_vk/ro_render_pass_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include <cstdint>
#include <string>
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
            /**
             * @brief Push a shader stage.
             *
             * @param p_stage The shader stage.
             * @param p_entry_function The entry function of the shader.
             * @param p_shader The shader instance.
             */
            void push_shader_stage(VkShaderStageFlagBits p_stage, const std::string& p_entry_function, Ref<ROShaderVK> p_shader) {
                shader_stages.push_back({.stage = p_stage, .entry_function = p_entry_function, .shader = p_shader});
            }

            /**
             * @brief Push a shader stage.
             *
             * @param p_stage The shader stage.
             * @param p_entry_function The entry function of the shader.
             * @param p_shader The shader instance.
             */
            void push_shader_stage(VkShaderStageFlagBits p_stage, Ref<ROShaderVK> p_shader) {
                shader_stages.push_back({.stage = p_stage, .shader = p_shader});
            }
            // Shader stages.
            std::vector<ShaderStage> shader_stages;
        };
        // Vertex input configurations.
        struct VertexInputConfig {
            // The vertex bindings.
            std::vector<VkVertexInputBindingDescription> bindings;
            // The vertex attributes.
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
        // Viewport configuration.
        struct ViewportStateConfig {
            // Viewports. Manual setup needed!
            std::vector<VkViewport> viewports;
            // Scissors. Manual setup needed!
            // A scissor clamps a rectangle region that the fragments will only be generated inside this region.
            std::vector<VkRect2D> scissors;

            /**
             * @brief Add a viewport.
             *
             * @param p_viewport The viewport to add.
             * @param p_scissor The scissor of this viewport.
             */
            void add_viewport(VkViewport p_viewport, VkRect2D p_scissor) {
                viewports.push_back(p_viewport);
                scissors.push_back(p_scissor);
            }

            /**
             * @brief Add a viewport.
             *
             * @param p_viewport The viewport to add.
             */
            void add_viewport(VkViewport p_viewport) {
                VkRect2D scissor{};
                scissor.offset.x = 0;
                scissor.offset.y = 0;
                scissor.extent.width = p_viewport.width;
                scissor.extent.height = p_viewport.height;
                viewports.push_back(p_viewport);
                scissors.push_back(scissor);
            }
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
        // Vertex input stage configurations.
        VertexInputConfig vertex_input{};
        // Input assembly stage configurations.
        InputAssemblyConfig input_assembly{};
        // Teselation stage configurations.
        std::optional<TesselationConfig> tesselation;
        // Viewport state configuration.
        ViewportStateConfig viewport{};
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
        // Render pass. Manual setup needed!
        Ref<RORenderPassVK> render_pass;
        // Indicate which subpass this graphics pipeline uses.
        uint32_t subpass_index = 0;
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
     * @brief Get the default configurations.
     *
     * @param p_shader_config The shader configurations.
     * @param p_vertex_input_config The vertex input configurations.
     * @param p_pipeline_layout The pipeline layout.
     * @param p_render_pass The render pass.
     * @return The default configurations according to the input parameters.
     */
    static Config get_default_render_opaque_configuration(
        const BaseRenderDataVK* p_render_data,
        const Config::ShaderConfig& p_shader_config,
        const Config::VertexInputConfig& p_vertex_input_config,
        Ref<ROPipelineLayoutVK> p_pipeline_layout,
        Ref<RORenderPassVK> p_render_pass) {
        auto result = Config();
        result.shader = p_shader_config;
        result.vertex_input = p_vertex_input_config;
        result.viewport.viewports = { p_render_data->viewport };
        result.viewport.scissors = { p_render_data->scissor };
        result.pipeline_layout = p_pipeline_layout;
        result.render_pass = p_render_pass;
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

    /**
     * @brief Get the configurations of the graphics pipeline.
     *
     * @return The configurations of the graphics pipeline.
     */
    const Config& get_config() const {
        return config;
    }

private:
    Config config;

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

    void setup_vertex_input_state_create_info(const Config& p_config,
                                              PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_input_assembly_state_create_info(const Config& p_config,
                                                PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_tessellation_state_create_info(const Config& p_config,
                                              PipelineStateCreateInfo& p_pipeline_create_infos);
    void setup_viewport_state_create_info(const Config& p_config,
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
