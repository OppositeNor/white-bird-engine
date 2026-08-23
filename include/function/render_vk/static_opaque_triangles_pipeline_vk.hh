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
#ifndef WBE_FILE_STATIC_OPAQUE_TRIANGLES_PIPELINE_VK_HH
#define WBE_FILE_STATIC_OPAQUE_TRIANGLES_PIPELINE_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class RendererVK;

inline constexpr std::string_view REND_RES_STATIC_OPAQUE_TRIANGLES_INDIRECT_COMMANDS_VK =
    "REND_RES_STATIC_OPAQUE_TRIANGLES_INDIRECT_COMMANDS_VK";
WBE_LABEL(RT_RUN_STATIC_OPAQUE_TRIANGLES_INDIRECT_VK, WBE_RENDER_TASK);

/**
 * @class StaticOpaqueTrianglesPipelineVK
 * @brief Static opaque indexed-indirect pipeline assembled from the render database.
 *
 * This keeps pass-specific pipeline compilation and task construction out of RendererVK. Compiled graphics pipelines and the
 * indirect command buffer are exposed as graph resources and retained by the graph after add_resources() is called.
 */
class StaticOpaqueTrianglesPipelineVK final {
public:
    /**
     * @class Config
     * @brief Attachment and material-pipeline selection for the manually assembled pass.
     */
    class Config {
    public:
        /** @brief Construct configuration with the default material contract and swap-chain attachment settings. */
        Config() = default;
        /** @brief Destroy the configuration. */
        ~Config() = default;
        /** @brief Copy configuration. */
        Config(const Config&) = default;
        /** @brief Move configuration. */
        Config(Config&&) noexcept = default;
        /** @brief Copy configuration. */
        Config& operator=(const Config&) = default;
        /** @brief Move configuration. */
        Config& operator=(Config&&) noexcept = default;

        /**
         * @brief Set the task id assigned to the assembled task.
         * @param p_task_id Task id.
         * @return This configuration.
         */
        Config& set_task_id(RendTaskID p_task_id);
        /**
         * @brief Set the resource key of the Cook-Torrance material supplying fallback typed textures.
         * @param p_resource_key Material resource key. The material must provide albedo, normal, and rma slots.
         * @return This configuration.
         */
        Config& set_default_material_resource_key(std::string_view p_resource_key);
        /**
         * @brief Set the base shader variable name of the fixed MaterialTextures contract.
         * @param p_variable_name Base shader variable name.
         * @return This configuration.
         */
        Config& set_material_textures_variable_name(std::string_view p_variable_name);
        /**
         * @brief Set the base shader variable name of the fixed MaterialParamsBuffer contract.
         * @param p_variable_name Base shader variable name.
         * @return This configuration.
         */
        Config& set_material_params_variable_name(std::string_view p_variable_name);
        /**
         * @brief Use one graphics-pipeline resource for every material batch.
         * @param p_resource_key Graphics-pipeline resource key.
         * @return This configuration.
         */
        Config& set_graphics_pipeline_resource_key(std::string_view p_resource_key);
        /**
         * @brief Add a per-material-pipeline replacement.
         * @param p_source_resource_key Material graphics-pipeline resource key.
         * @param p_target_resource_key Replacement graphics-pipeline resource key.
         * @return This configuration.
         */
        Config& add_graphics_pipeline_override(std::string_view p_source_resource_key, std::string_view p_target_resource_key);
        /**
         * @brief Set the render area; a zero extent uses the swap-chain extent.
         * @param p_render_extent Render extent.
         * @return This configuration.
         */
        Config& set_render_extent(VkExtent2D p_render_extent);
        /**
         * @brief Add a color attachment and its dynamic-rendering format.
         * @param p_resource_id Render-graph resource id.
         * @param p_format Attachment format.
         * @return This configuration.
         */
        Config& add_color_attachment(std::string_view p_resource_id, VkFormat p_format);
        /**
         * @brief Set the depth attachment and its dynamic-rendering format.
         * @param p_resource_id Render-graph resource id.
         * @param p_format Attachment format.
         * @return This configuration.
         */
        Config& set_depth_attachment(std::string_view p_resource_id, VkFormat p_format);
        /**
         * @brief Set the dynamic-rendering stencil format.
         * @param p_format Stencil attachment format.
         * @return This configuration.
         */
        Config& set_stencil_attachment_format(VkFormat p_format);
        /**
         * @brief Add a clear value in attachment order.
         * @param p_clear_value Clear value.
         * @return This configuration.
         */
        Config& add_clear_value(VkClearValue p_clear_value);
        /**
         * @brief Set the load operation shared by color attachments.
         * @param p_load_op Attachment load operation.
         * @return This configuration.
         */
        Config& set_color_attachment_load_op(VkAttachmentLoadOp p_load_op);
        /**
         * @brief Set the depth attachment load operation.
         * @param p_load_op Attachment load operation.
         * @return This configuration.
         */
        Config& set_depth_attachment_load_op(VkAttachmentLoadOp p_load_op);

        /**
         * @brief Get the configured task id.
         * @return Task id.
         */
        RendTaskID get_task_id() const;
        /**
         * @brief Get the fallback material resource key.
         * @return Material resource key.
         */
        std::string_view get_default_material_resource_key() const;
        /**
         * @brief Get the MaterialTextures base variable name.
         * @return Base shader variable name.
         */
        std::string_view get_material_textures_variable_name() const;
        /**
         * @brief Get the MaterialParamsBuffer base variable name.
         * @return Base shader variable name.
         */
        std::string_view get_material_params_variable_name() const;
        /**
         * @brief Get the pipeline resource applied to all batches.
         * @return Graphics-pipeline resource key, or an empty view when material pipelines are used.
         */
        std::string_view get_graphics_pipeline_resource_key() const;
        /**
         * @brief Get the number of per-pipeline replacements.
         * @return Replacement count.
         */
        size_t get_graphics_pipeline_override_count() const;
        /**
         * @brief Get a replacement source resource key.
         * @param p_index Replacement index.
         * @return Source graphics-pipeline resource key.
         * @throws std::runtime_error When p_index is out of bounds.
         */
        std::string_view get_graphics_pipeline_override_source(size_t p_index) const;
        /**
         * @brief Get a replacement target resource key.
         * @param p_index Replacement index.
         * @return Target graphics-pipeline resource key.
         * @throws std::runtime_error When p_index is out of bounds.
         */
        std::string_view get_graphics_pipeline_override_target(size_t p_index) const;
        /**
         * @brief Get the configured render extent.
         * @return Render extent; a zero extent selects the swap-chain extent.
         */
        VkExtent2D get_render_extent() const;
        /**
         * @brief Get the number of color attachments.
         * @return Color attachment count.
         */
        size_t get_color_attachment_count() const;
        /**
         * @brief Get a color attachment resource id.
         * @param p_index Color attachment index.
         * @return Render-graph resource id.
         * @throws std::runtime_error When p_index is out of bounds.
         */
        std::string_view get_color_attachment_id(size_t p_index) const;
        /**
         * @brief Get a color attachment format.
         * @param p_index Color attachment index.
         * @return Attachment format.
         * @throws std::runtime_error When p_index is out of bounds.
         */
        VkFormat get_color_attachment_format(size_t p_index) const;
        /**
         * @brief Return whether a depth attachment is configured.
         * @return True when a depth attachment is configured.
         */
        bool has_depth_attachment() const;
        /**
         * @brief Get the depth attachment resource id.
         * @return Render-graph resource id.
         * @throws std::runtime_error When no depth attachment is configured.
         */
        std::string_view get_depth_attachment_id() const;
        /**
         * @brief Get the depth attachment format.
         * @return Depth attachment format.
         */
        VkFormat get_depth_attachment_format() const;
        /**
         * @brief Get the stencil attachment format.
         * @return Stencil attachment format.
         */
        VkFormat get_stencil_attachment_format() const;
        /**
         * @brief Get the number of explicit clear values.
         * @return Clear value count.
         */
        size_t get_clear_value_count() const;
        /**
         * @brief Get an explicit clear value.
         * @param p_index Clear value index.
         * @return Clear value.
         * @throws std::runtime_error When p_index is out of bounds.
         */
        VkClearValue get_clear_value(size_t p_index) const;
        /**
         * @brief Get the color attachment load operation.
         * @return Color attachment load operation.
         */
        VkAttachmentLoadOp get_color_attachment_load_op() const;
        /**
         * @brief Get the depth attachment load operation.
         * @return Depth attachment load operation.
         */
        VkAttachmentLoadOp get_depth_attachment_load_op() const;

    private:
        struct PipelineOverride {
            PipelineOverride() = default;
            ~PipelineOverride() = default;
            PipelineOverride(const PipelineOverride&) = default;
            PipelineOverride(PipelineOverride&&) noexcept = default;
            PipelineOverride& operator=(const PipelineOverride&) = default;
            PipelineOverride& operator=(PipelineOverride&&) noexcept = default;

            std::string source_resource_key;
            std::string target_resource_key;
        };

        RendTaskID task_id = RT_RUN_STATIC_OPAQUE_TRIANGLES_INDIRECT_VK;
        std::string default_material_resource_key = "default_material";
        std::string material_textures_variable_name = "material_textures";
        std::string material_params_variable_name = "material_params";
        std::string graphics_pipeline_resource_key;
        std::vector<PipelineOverride> graphics_pipeline_overrides;
        VkExtent2D render_extent{};
        std::vector<std::string> color_attachment_ids;
        std::optional<std::string> depth_attachment_id;
        std::vector<VkFormat> color_attachment_formats;
        VkFormat depth_attachment_format = VK_FORMAT_UNDEFINED;
        VkFormat stencil_attachment_format = VK_FORMAT_UNDEFINED;
        std::vector<VkClearValue> clear_values;
        VkAttachmentLoadOp color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentLoadOp depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    };

    /**
     * @struct DescriptorBinding
     * @brief Reflected descriptor location within a compiled batch.
     */
    struct DescriptorBinding {
        /** @brief Descriptor-set index. */
        uint32_t set_index;
        /** @brief Binding within the descriptor set. */
        uint32_t binding;
    };

    /** @brief Static opaque pipelines require a renderer and configuration. */
    StaticOpaqueTrianglesPipelineVK() = delete;
    /** @brief Destroy the compiled pipeline state. */
    ~StaticOpaqueTrianglesPipelineVK() = default;
    /** @brief Static opaque pipeline state is not copyable. */
    StaticOpaqueTrianglesPipelineVK(const StaticOpaqueTrianglesPipelineVK&) = delete;
    /** @brief Move compiled pipeline state. */
    StaticOpaqueTrianglesPipelineVK(StaticOpaqueTrianglesPipelineVK&&) noexcept = default;
    /** @brief Static opaque pipeline state is not copy-assignable. */
    StaticOpaqueTrianglesPipelineVK& operator=(const StaticOpaqueTrianglesPipelineVK&) = delete;
    /** @brief Move-assign compiled pipeline state. */
    StaticOpaqueTrianglesPipelineVK& operator=(StaticOpaqueTrianglesPipelineVK&&) noexcept = default;

    /**
     * @brief Compile and assemble the pass from the renderer's database and generic Vulkan compiler.
     *
     * Every compiled graphics pipeline is explicitly registered in RenderDatabase before construction completes.
     *
     * @param p_renderer Renderer providing Vulkan state and static opaque geometry.
     * @param p_config Pass configuration.
     */
    StaticOpaqueTrianglesPipelineVK(RendererVK* p_renderer, const Config& p_config);

    /**
     * @brief Add the compiled graphics pipelines and indirect command buffer to a graph descriptor.
     * @param p_graph_desc Graph descriptor receiving the resources.
     */
    void add_resources(RendGraphDescVK& p_graph_desc) const;

    /**
     * @brief Move the assembled task descriptor out of the pipeline.
     * @return Assembled render-task descriptor.
     */
    RendTaskDescVK take_task() {
        return std::move(task);
    }

    /**
     * @brief Get the task descriptor before it is moved out of the pipeline.
     * @return Mutable assembled render-task descriptor.
     */
    RendTaskDescVK& get_task() {
        return task;
    }

    /**
     * @brief Get the number of compiled material batches.
     * @return Compiled batch count.
     */
    size_t get_compiled_batch_count() const;

    /**
     * @brief Get a descriptor set retained by a compiled batch.
     * @param p_batch_index Compiled batch index.
     * @param p_set_index Descriptor-set index.
     * @return Retained descriptor set.
     * @throws std::runtime_error When either index is out of bounds.
     */
    Ref<RODescriptorSetVK> get_descriptor_set(size_t p_batch_index, size_t p_set_index) const;

    /**
     * @brief Find a reflected descriptor binding by field path.
     * @param p_batch_index Compiled batch index.
     * @param p_field_path Reflected shader field path.
     * @return Descriptor-set index and binding.
     * @throws std::runtime_error When the batch or field path is absent.
     */
    DescriptorBinding get_descriptor_binding(size_t p_batch_index, std::string_view p_field_path) const;

private:
    struct CompiledBatch {
        CompiledBatch() = default;
        ~CompiledBatch() = default;
        CompiledBatch(const CompiledBatch&) = default;
        CompiledBatch(CompiledBatch&&) noexcept = default;
        CompiledBatch& operator=(const CompiledBatch&) = default;
        CompiledBatch& operator=(CompiledBatch&&) noexcept = default;

        std::vector<Ref<RODescriptorSetVK>> descriptor_sets;
        GraphicsPipelineCompilerVK::DescriptorSetBindings descriptor_set_bindings;
    };
    struct GraphicsPipelineResource {
        GraphicsPipelineResource() = default;
        ~GraphicsPipelineResource() = default;
        GraphicsPipelineResource(const GraphicsPipelineResource&) = default;
        GraphicsPipelineResource(GraphicsPipelineResource&&) noexcept = default;
        GraphicsPipelineResource& operator=(const GraphicsPipelineResource&) = default;
        GraphicsPipelineResource& operator=(GraphicsPipelineResource&&) noexcept = default;

        std::string resource_key;
        Ref<ROGraphicsPipelineVK> graphics_pipeline;
    };

    RendTaskDescVK task;
    std::string indirect_commands_resource_key;
    Ref<ROBufferVK> indirect_commands;
    std::vector<GraphicsPipelineResource> graphics_pipeline_resources;
    std::vector<CompiledBatch> compiled_batches;
};

} // namespace WhiteBirdEngine

#endif
