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
#ifndef WBE_FILE_RO_GRAPHICS_PIPELINE_VK_HH
#define WBE_FILE_RO_GRAPHICS_PIPELINE_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_render_pass_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <span>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ROGraphicsPipelineVK
 * @brief The graphics pipeline.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) ROGraphicsPipelineVK final : public RenderObjectVK {
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
                // The entry function name of the shader.
                std::string entry_function = "main";
                // Shader instance.
                Ref<ROShaderVK> shader;
            };
            // The vertex shader stage.
            Required<ShaderStage> vertex_shader;
            // The fragment shader stage.
            Required<ShaderStage> fragment_shader;
            // TODO: TSC TSE and Geometry shader setup

            /**
             * @brief Set vertex shader stage.
             *
             * @param p_shader The shader instance.
             * @return *this
             */
            ShaderConfig& set_vertex_shader(Ref<ROShaderVK> p_shader, const std::string& p_entry_function = "main") {
                vertex_shader = { .entry_function = p_entry_function, .shader = p_shader };
                return *this;
            }

            /**
             * @brief Set fragment shader stage.
             *
             * @param p_shader The shader instance.
             * @return *this
             */
            ShaderConfig& set_fragment_shader(Ref<ROShaderVK> p_shader, const std::string& p_entry_function = "main") {
                fragment_shader = { .entry_function = p_entry_function, .shader = p_shader };
                return *this;
            }

            /**
             * @brief Set shaders.
             *
             * @param p_vertex_shader The vertex shader instance.
             * @param p_fragment_shader The fragment shader instance.
             * @param p_vertex_entry_function The vertex shader entry function.
             * @param p_fragment_entry_function The fragment shader entry function.
             * @return *this.
             */
            ShaderConfig& set_shaders(Ref<ROShaderVK> p_vertex_shader,
                                      Ref<ROShaderVK> p_fragment_shader,
                                      const std::string& p_vertex_entry_function = "main",
                                      const std::string& p_fragment_entry_function = "main") {
                set_vertex_shader(p_vertex_shader, p_vertex_entry_function);
                set_fragment_shader(p_fragment_shader, p_fragment_entry_function);
                return *this;
            }

            /**
             * @brief Set shaders from resource IDs.
             *
             * @param p_render_data The global render data.
             * @param p_vertex_shader_res_id The vertex shader resource ID.
             * @param p_fragment_shader_res_id The fragment shader resource ID.
             * @param p_vertex_entry_function The vertex shader entry function.
             * @param p_fragment_entry_function The fragment shader entry function.
             * @return *this.
             */
            ShaderConfig& set_shaders(const BaseRenderDataVK* p_render_data,
                                      const std::string& p_vertex_shader_res_id,
                                      const std::string& p_fragment_shader_res_id,
                                      const std::string& p_vertex_entry_function = "main",
                                      const std::string& p_fragment_entry_function = "main") {
                set_vertex_shader(
                    make_ref<ROShaderVK>(p_render_data->allocator, p_render_data, p_vertex_shader_res_id),
                    p_vertex_entry_function);
                set_fragment_shader(
                    make_ref<ROShaderVK>(p_render_data->allocator, p_render_data, p_fragment_shader_res_id),
                    p_fragment_entry_function);
                return *this;
            }
            ShaderConfig& set_shaders(const BaseRenderDataVK* p_render_data,
                                      RefRes<RUShader> p_vertex_shader_res,
                                      RefRes<RUShader> p_fragment_shader_res,
                                      const std::string& p_vertex_entry_function = "main",
                                      const std::string& p_fragment_entry_function = "main") {
                set_vertex_shader(
                    make_ref<ROShaderVK>(p_render_data->allocator, p_render_data, p_vertex_shader_res),
                    p_vertex_entry_function);
                set_fragment_shader(
                    make_ref<ROShaderVK>(p_render_data->allocator, p_render_data, p_fragment_shader_res),
                    p_fragment_entry_function);
                return *this;
            }
        };
        // Vertex input configurations.
        struct VertexInputConfig {
            // The vertex bindings.
            std::vector<VkVertexInputBindingDescription> bindings;
            // The vertex attributes.
            std::vector<VkVertexInputAttributeDescription> attributes;
            
            /**
             * @brief Set the vertex input bindings.
             *
             * @param p_bindings The vertex input bindings.
             * @return *this
             */
            VertexInputConfig& set_bindings(const std::vector<VkVertexInputBindingDescription>& p_bindings) {
                bindings = p_bindings;
                return *this;
            }

            /**
             * @brief Set the vertex input bindings.
             *
             * @param p_bindings The vertex input bindings.
             * @return *this
             */
            VertexInputConfig& set_bindings(std::vector<VkVertexInputBindingDescription>&& p_bindings) {
                bindings = std::move(p_bindings);
                return *this;
            }

            /**
             * @brief Set the vertex input attributes.
             *
             * @param p_attributes The vertex input attributes.
             * @return *this
             */
            VertexInputConfig& set_attributes(const std::vector<VkVertexInputAttributeDescription>& p_attributes) {
                attributes = p_attributes;
                return *this;
            }

            /**
             * @brief Set the vertex input attributes.
             *
             * @param p_attributes The vertex input attributes.
             * @return *this
             */
            VertexInputConfig& set_attributes(std::vector<VkVertexInputAttributeDescription>&& p_attributes) {
                attributes = std::move(p_attributes);
                return *this;
            }

            /**
             * @brief Add a binding to the vertex input bindings.
             *
             * @param p_binding The binding to add to the vertex input bindings.
             * @return *this
             */
            VertexInputConfig& add_binding(VkVertexInputBindingDescription p_binding) {
                bindings.push_back(p_binding);
                return *this;
            }

            /**
             * @brief Add a range of bindings to the vertex input bindings.
             *
             * @param p_bindings The bindings to add to the vertex input bindings.
             * @return *this
             */
            VertexInputConfig& add_bindings(std::span<const VkVertexInputBindingDescription> p_bindings) {
                bindings.append_range(p_bindings);
                return *this;
            }

            /**
             * @brief Add an atribute to the vertex input attributes.
             *
             * @param p_attribute The attribute to add to the vertex input attributes.
             * @return *this
             */
            VertexInputConfig& add_attribute(VkVertexInputAttributeDescription p_attribute) {
                attributes.push_back(p_attribute);
                return *this;
            }

            /**
             * @brief Add a range of atributes to the vertex input attributes.
             *
             * @param p_attributes The attributes to add to the vertex input attributes.
             * @return *this
             */
            VertexInputConfig& add_attributes(std::span<const VkVertexInputAttributeDescription> p_attributes) {
                attributes.append_range(p_attributes);
                return *this;
            }
        };
        // Input assembly configurations.
        struct InputAssemblyConfig {
            // The topology used in the input assembly stage.
            VkPrimitiveTopology primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            // Enable restart for strip and fan primitive topology.
            VkBool32 primitive_restart_enabled = VK_FALSE;

            /**
             * @brief Set the primitive input topology.
             *
             * @param p_primitive_topology The primitive input topology.
             * @return *this
             */
            InputAssemblyConfig& set_primitive_topology(VkPrimitiveTopology p_primitive_topology) {
                primitive_topology = p_primitive_topology;
                return *this;
            }

            /**
             * @brief Enable primitive restart.
             *
             * @return *this
             */
            InputAssemblyConfig& enable_primitive_restart() {
                primitive_restart_enabled = VK_TRUE;
                return *this;
            }

            /**
             * @brief Disable primitive restart.
             *
             * @return *this
             */
            InputAssemblyConfig& disable_primitive_restart() {
                primitive_restart_enabled = VK_FALSE;
                return *this;
            }

            /**
             * @brief Set primitive restart.
             *
             * @param p_primitive_restart_enabled VK_TRUE to enable primitive restart, VK_FALSE otherwise.
             * @return *this
             */
            InputAssemblyConfig& set_primitive_restart(VkBool32 p_primitive_restart_enabled) {
                primitive_restart_enabled = p_primitive_restart_enabled;
                return *this;
            }
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
             * @return *this
             */
            ViewportStateConfig& add_viewport(VkViewport p_viewport, VkRect2D p_scissor) {
                viewports.push_back(p_viewport);
                scissors.push_back(p_scissor);
                return *this;
            }

            /**
             * @brief Add a viewport.
             *
             * @param p_viewport The viewport to add.
             * @return *this
             */
            ViewportStateConfig& add_viewport(VkViewport p_viewport) {
                VkRect2D scissor{};
                scissor.offset.x = 0;
                scissor.offset.y = 0;
                scissor.extent.width = static_cast<uint32_t>(p_viewport.width);
                scissor.extent.height = static_cast<uint32_t>(p_viewport.height);
                viewports.push_back(p_viewport);
                scissors.push_back(scissor);
                return *this;
            }
        };
        // Rasterization configuraion.
        struct RasterizationConfig {
            // If set to true, the fragments of depth over 1.0 or smaller than 0.0 won't be discarded but will be clamped.
            VkBool32 depth_clamp_enabled = VK_FALSE;
            // Polygon mode.
            VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
            // Cull mode. Default cull back faces.
            VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
            // Which direction is considered front, default to counter-clockwise.
            VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;

            /**
             * @brief Enalbe depth clamp.
             *
             * @return *this
             */
            RasterizationConfig& enable_depth_clamp() {
                depth_clamp_enabled = VK_TRUE;
                return *this;
            }

            /**
             * @brief Disable depth clamp.
             *
             * @return *this
             */
            RasterizationConfig& disable_depth_clamp() {
                depth_clamp_enabled = VK_FALSE;
                return *this;
            }

            /**
             * @brief Enable/disable depth clamp.
             *
             * @param p_depth_clamp_enabled VK_TRUE to enable depth clamp, VK_FALSE otherwise.
             * @return 
             */
            RasterizationConfig& set_depth_clamp(bool p_depth_clamp_enabled) {
                depth_clamp_enabled = p_depth_clamp_enabled ? VK_TRUE : VK_FALSE;
                return *this;
            }

            /**
             * @brief Set polygon mode.
             *
             * @param p_polygon_mode The polygon mode to set to.
             * @return *this
             */
            RasterizationConfig& set_polygon_mode(VkPolygonMode p_polygon_mode) {
                polygon_mode = p_polygon_mode;
                return *this;
            }

            /**
             * @brief Set cull mode.
             *
             * @param p_cull_mode The cull mode to set to.
             * @return *this
             */
            RasterizationConfig& set_cull_mode(VkCullModeFlags p_cull_mode) {
                cull_mode = p_cull_mode;
                return *this;
            }

            /**
             * @brief Set front face.
             *
             * @param p_front_face The front face to set to.
             * @return *this
             */
            RasterizationConfig& set_front_face(VkFrontFace p_front_face) {
                front_face = p_front_face;
                return *this;
            }

        };
        // Multisample configurations.
        struct MultisampleConfig {
            // TODO
        };
        // Depth test configurations.
        struct DepthTestConfig {
            // Depth test enabled.
            VkBool32 depth_test_enabled = VK_TRUE;
            // Enable depth writing.
            VkBool32 depth_write_enabled = VK_TRUE;
            // Compare operation.
            VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS;

            /**
             * @brief Enalbe depth test.
             *
             * @return *this
             */
            DepthTestConfig& enable_depth_test() {
                depth_test_enabled = VK_TRUE;
                return *this;
            }

            /**
             * @brief Disable depth test.
             *
             * @return *this
             */
            DepthTestConfig& disable_depth_test() {
                depth_test_enabled = VK_FALSE;
                return *this;
            }

            /**
             * @brief Enable/disable depth test.
             *
             * @param p_depth_test_enabled VK_TRUE to enable depth test, VK_FALSE otherwise.
             * @return 
             */
            DepthTestConfig& set_depth_test(VkBool32 p_depth_test_enabled) {
                depth_test_enabled = p_depth_test_enabled;
                return *this;
            }

            /**
             * @brief Enalbe depth write.
             *
             * @return *this
             */
            DepthTestConfig& enable_depth_write() {
                depth_write_enabled = VK_TRUE;
                return *this;
            }

            /**
             * @brief Disable depth write.
             *
             * @return *this
             */
            DepthTestConfig& disable_depth_write() {
                depth_write_enabled = VK_FALSE;
                return *this;
            }

            /**
             * @brief Enable/disable depth write.
             *
             * @param p_depth_write_enabled VK_TRUE to enable depth write, VK_FALSE otherwise.
             * @return 
             */
            DepthTestConfig& set_depth_write(VkBool32 p_depth_write_enabled) {
                depth_write_enabled = p_depth_write_enabled;
                return *this;
            }

            /**
             * @brief Set depth compare operation.
             *
             * @param p_compare_op The compare operation to set to.
             * @return *this.
             */
            DepthTestConfig& set_depth_compare_op(VkCompareOp p_compare_op) {
                depth_compare_op = p_compare_op;
                return *this;
            }
        };
        // Color blend configurations.
        struct ColorBlendConfig {
            // Enable logic operations for the attachments.
            VkBool32 logic_op_enabled = VK_FALSE;
            // Logic operation. Ignored if logic_op_enable is VK_FALSE.
            VkLogicOp logic_op;
            // Color blend attachments.
            std::vector<VkPipelineColorBlendAttachmentState> attachments;

            /**
             * @brief Enaable logic operation.
             *
             * @param p_logic_op The logic operation to set to.
             * @return *this
             */
            ColorBlendConfig& enalbe_logic_op(VkLogicOp p_logic_op) {
                logic_op_enabled = VK_TRUE;
                logic_op = p_logic_op;
                return *this;
            }

            /**
             * @brief Disable logic operation.
             *
             * @return *this
             */
            ColorBlendConfig& disable_logic_op() {
                logic_op_enabled = VK_FALSE;
                return *this;
            }

            /**
             * @brief Add a default color attachment.
             *
             * @return *this
             */
            ColorBlendConfig& add_default_color_attachment() {
                attachments.push_back({
                    .blendEnable = VK_FALSE,
                    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
                });
                return *this;
            }
        };
        // Dynamic state configurations.
        struct DynamicStateConfig {
            // Dynamic states.
            std::vector<VkDynamicState> dynamic_states;

            DynamicStateConfig& add_default_dynamic_states() {
                dynamic_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
                dynamic_states.push_back(VK_DYNAMIC_STATE_SCISSOR);
                return *this;
            }

            /**
             * @brief Add a dynamic state.
             *
             * @param p_dynamic_state The dynamic state to be added.
             * @return *this
             */
            DynamicStateConfig& add_dynamic_state(VkDynamicState p_dynamic_state) {
                dynamic_states.push_back(p_dynamic_state);
                return *this;
            }

            /**
             * @brief Set the dynamic states.
             *
             * @param p_dynamic_state The dynamic states to set to.
             * @return *this
             */
            DynamicStateConfig& set_dynamic_states(const std::vector<VkDynamicState>& p_dynamic_state) {
                dynamic_states = p_dynamic_state;
                return *this;
            }

            /**
             * @brief Set the dynamic states.
             *
             * @param p_dynamic_state The dynamic states to set to.
             * @return *this
             */
            DynamicStateConfig& set_dynamic_states(std::vector<VkDynamicState>&& p_dynamic_state) {
                dynamic_states = std::move(p_dynamic_state);
                return *this;
            }
        };

        /**
         * @brief Build the pipeline from a resource.
         *
         * @param p_res_key The key to the resource for building the pipeline.
         * @return *this
         */
        Config& build_pipeline(const BaseRenderDataVK* p_render_data, const std::string& p_res_key, Ref<RORenderPassVK> p_render_pass);

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
        Ref<ROPipelineLayoutVK> p_pipeline_layout,
        Ref<RORenderPassVK> p_render_pass) {

        ROGraphicsPipelineVK::Config::VertexInputConfig vertex_input_config;
        vertex_input_config
            .add_binding(VERTEX_3D_TEXTURED_INPUT_BINDING_DESCRIPTION)
            .add_attributes(VERTEX_3D_TEXTURED_INPUT_ATTRIBUTE_DESCRIPTION);
        auto result = Config();
        result.shader = p_shader_config;
        result.vertex_input = vertex_input_config;
        result.viewport.viewports = { p_render_data->viewport };
        result.viewport.scissors = { p_render_data->scissor };
        result.pipeline_layout = p_pipeline_layout;
        result.render_pass = p_render_pass;
        result.color_blend.add_default_color_attachment();
        result.dynamic_state.add_default_dynamic_states();
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

    struct ExposedFields {
        Ref<const ROShaderVK> vertex_shader;
        Ref<const ROShaderVK> fragment_shader;
        // TODO: TSC TSE geometry shaders
        Ref<const ROPipelineLayoutVK> pipeline_layout;
        Ref<const RORenderPassVK> render_pass;
    };

    const ExposedFields& fields() const {
        return exposed_fields;
    }

private:
    Config config;
    ExposedFields exposed_fields;
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;

    struct PipelineStateCreateInfo {
        VkPipelineViewportStateCreateInfo viewport_state{};
        VkPipelineVertexInputStateCreateInfo vertex_input_state{};
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
        Required<VkPipelineTessellationStateCreateInfo> tessellation_state;
        VkPipelineRasterizationStateCreateInfo rasterization_state{};
        VkPipelineMultisampleStateCreateInfo multisample_state{};
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
        VkPipelineColorBlendStateCreateInfo color_blend_state{};
        VkPipelineDynamicStateCreateInfo dynamic_state{};
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

    static VkPipelineShaderStageCreateInfo get_shader_stage_create_info(VkShaderStageFlagBits p_stage_flag, const Config::ShaderConfig::ShaderStage& p_stage);
};
} // namespace WhiteBirdEngine

#endif
