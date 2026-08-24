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
#ifndef WBE_FILE_RT_RUN_STATIC_OPAQUE_TRIANGLES_VK_HH
#define WBE_FILE_RT_RUN_STATIC_OPAQUE_TRIANGLES_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class RendererVK;

WBE_LABEL(RT_RUN_STATIC_OPAQUE_TRIANGLES_INDIRECT_VK, WBE_RENDER_TASK);

/**
 * @class RTRunStaticOpaqueTrianglesVK
 * @brief Render task that compiles and draws the render database's static opaque triangles.
 *
 * The task compiles and retains its graphics pipelines, material descriptors, and indirect-command buffer. Populate
 * RenderDatabase and call RendererVK::update_static_opaque_triangles() before construction. Attachment resources configured
 * by Config remain the render graph caller's responsibility.
 */
class RTRunStaticOpaqueTrianglesVK final : public RenderTaskVK {
public:
    /**
     * @class Config
     * @brief Attachment and material-pipeline selection for the task.
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
         * @brief Set the resource key of the material supplying fallback textures for shader-reflected roles.
         * @param p_resource_key Material resource key. It must provide every WBE_TEXTURE role consumed by a pipeline batch.
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
         * @brief Set the base shader variable name of the reflected MaterialParamsBuffer contract.
         * @param p_variable_name Base shader variable name.
         * @return This configuration.
         */
        Config& set_material_params_variable_name(std::string_view p_variable_name);
        /**
         * @brief Use one graphics-pipeline resource for every material batch.
         *
         * A non-empty resource key takes precedence over replacements added with add_graphics_pipeline_override().
         *
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
         *
         * Color attachment values precede the optional depth value. Missing color values default to opaque black. When a
         * depth attachment is configured and only color values are present, depth defaults to 1.0 and stencil to 0.
         *
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

    /** @brief Static opaque tasks require a renderer and configuration. */
    RTRunStaticOpaqueTrianglesVK() = delete;
    /** @brief Destroy the task and its retained render objects. */
    virtual ~RTRunStaticOpaqueTrianglesVK() override = default;
    /** @brief Static opaque tasks are not copyable. */
    RTRunStaticOpaqueTrianglesVK(const RTRunStaticOpaqueTrianglesVK&) = delete;
    /** @brief Static opaque tasks are not movable. */
    RTRunStaticOpaqueTrianglesVK(RTRunStaticOpaqueTrianglesVK&&) noexcept = delete;
    /** @brief Static opaque tasks are not copy-assignable. */
    RTRunStaticOpaqueTrianglesVK& operator=(const RTRunStaticOpaqueTrianglesVK&) = delete;
    /** @brief Static opaque tasks are not move-assignable. */
    RTRunStaticOpaqueTrianglesVK& operator=(RTRunStaticOpaqueTrianglesVK&&) noexcept = delete;

    /**
     * @brief Compile the task from the renderer's database and generic Vulkan compiler.
     *
     * Every compiled graphics pipeline is explicitly registered in RenderDatabase before construction completes.
     *
     * @param p_renderer Renderer providing Vulkan state and static opaque geometry.
     * @param p_config Pass configuration.
     * @throws std::runtime_error When renderer or resource state is missing, configuration is invalid, reflected material
     * contracts are incompatible, or device draw and descriptor limits are exceeded.
     */
    RTRunStaticOpaqueTrianglesVK(RendererVK* p_renderer, const Config& p_config);

    /** @copydoc RenderTaskVK::queue_family */
    virtual QueueFamilyVK queue_family() const override;
    /** @copydoc RenderTaskVK::read_resources */
    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;
    /** @copydoc RenderTaskVK::write_resources */
    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;
    /** @copydoc RenderTaskVK::record */
    virtual void record(VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double p_delta) override;

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
    Unique<RTRunGraphicsPipelineIndirectVK> indirect_task;
    std::vector<CompiledBatch> compiled_batches;
};

} // namespace WhiteBirdEngine

#endif
