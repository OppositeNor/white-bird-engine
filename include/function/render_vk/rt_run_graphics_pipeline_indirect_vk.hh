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
#ifndef WBE_FILE_RT_RUN_GRAPHICS_PIPELINE_INDIRECT_VK_HH
#define WBE_FILE_RT_RUN_GRAPHICS_PIPELINE_INDIRECT_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/pipeline_descriptor_bindings_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTRunGraphicsPipelineIndirectVK
 * @brief Render task that submits indexed draw commands from a GPU buffer in one Vulkan call.
 *
 * Pipelines are retained directly while graph resources are bound to stable containers during graph construction.
 */
struct RTRunGraphicsPipelineIndirectVK final : public IRenderTaskVK {
    /**
     * @class PipelineBatch
     * @brief One graphics pipeline and its contiguous range of indirect commands.
     */
    struct PipelineBatch {
        explicit PipelineBatch(RenderAllocator* p_allocator) {
            static_cast<void>(p_allocator);
        }
        ~PipelineBatch() = default;
        PipelineBatch(const PipelineBatch&) = default;
        PipelineBatch(PipelineBatch&&) noexcept = default;
        PipelineBatch& operator=(const PipelineBatch&) = default;
        PipelineBatch& operator=(PipelineBatch&&) noexcept = default;

        /** Graphics pipeline retained directly by the task. */
        Ref<ROGraphicsPipelineVK> graphics_pipeline = MEM_NULL;
        /** First tightly packed indirect command in the shared command buffer. */
        uint32_t first_draw = 0;
        /** Number of indirect commands rendered by this pipeline. */
        uint32_t draw_count = 0;
        /** Dynamic vertex-input state used by this command range. */
        VertexInputDescriptionVK vertex_input;
        /** Byte offset bound for the shared vertex buffer. */
        VkDeviceSize vertex_buffer_offset = 0;
    };

    /**
     * @class Builder
     * @brief Configures an indexed-indirect graphics task without constructor variants.
     *
     * Attachments are optional. Leaving both attachment collections empty creates an attachment-less dynamic-rendering task.
     * Draw buffers are identified by graph resource ids and resolved when the graph is built.
     */
    struct Builder {
        Builder() = delete;
        ~Builder() = default;
        Builder(const Builder&) = default;
        Builder(Builder&&) noexcept = default;
        Builder& operator=(const Builder&) = default;
        Builder& operator=(Builder&&) noexcept = default;

        /**
         * @brief Construct allocator-owned builder storage.
         */
        explicit Builder(RenderAllocator* p_allocator);

        /**
         * @brief Set the dynamic-rendering area.
         */
        Builder& set_render_extent(VkExtent2D p_render_extent);

        /**
         * @brief Append one color attachment and its clear value in pipeline order.
         */
        Builder& add_color_attachment(std::string_view p_resource_id, const VkClearValue& p_clear_value = {});

        /**
         * @brief Set the optional depth attachment and its clear value.
         */
        Builder& set_depth_attachment(std::string_view p_resource_id, const VkClearValue& p_clear_value = {});

        /**
         * @brief Set the load operation shared by color attachments.
         */
        Builder& set_color_attachment_load_op(VkAttachmentLoadOp p_load_op);

        /**
         * @brief Set the depth attachment load operation.
         */
        Builder& set_depth_attachment_load_op(VkAttachmentLoadOp p_load_op);

        /**
         * @brief Resolve the vertex buffer from a graph resource.
         */
        Builder& set_vertex_buffer(std::string_view p_resource_id);

        /**
         * @brief Resolve the index buffer from a graph resource.
         */
        Builder& set_index_buffer(std::string_view p_resource_id);

        /**
         * @brief Resolve the indexed-indirect command buffer from a graph resource.
         */
        Builder& set_indirect_buffer(std::string_view p_resource_id);

        /**
         * @brief Replace the draw plan with one graphics pipeline batch beginning at command zero.
         */
        Builder& set_pipeline(Ref<ROGraphicsPipelineVK> p_pipeline,
            uint32_t p_draw_count,
            VertexInputDescriptionVK p_vertex_input = {},
            VkDeviceSize p_vertex_buffer_offset = 0);

        /**
         * @brief Append one graphics pipeline batch.
         */
        Builder& add_pipeline_batch(Ref<ROGraphicsPipelineVK> p_pipeline,
            uint32_t p_first_draw,
            uint32_t p_draw_count,
            VertexInputDescriptionVK p_vertex_input = {},
            VkDeviceSize p_vertex_buffer_offset = 0);

        /**
         * @brief Append one preconfigured graphics pipeline batch.
         */
        Builder& add_pipeline_batch(PipelineBatch p_batch);

        /**
         * @brief Create an allocator-owned render task.
         */
        Unique<RTRunGraphicsPipelineIndirectVK> create_unique(const BaseRenderDataVK* p_render_data) const;

    private:
        friend RTRunGraphicsPipelineIndirectVK;

        RenderAllocator* allocator = nullptr;
        VkExtent2D render_extent{};
        Vector<VkClearValue, RenderAllocator> color_clear_values;
        Vector<std::string, RenderAllocator> color_attachment_ids;
        std::optional<VkClearValue> depth_clear_value;
        std::optional<std::string> depth_attachment_id;
        VkAttachmentLoadOp color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentLoadOp depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        std::string vertex_buffer_id;
        std::string index_buffer_id;
        std::string indirect_buffer_id;
        Vector<PipelineBatch, RenderAllocator> pipeline_batches;
    };

    RTRunGraphicsPipelineIndirectVK() = delete;
    virtual ~RTRunGraphicsPipelineIndirectVK() override = default;
    RTRunGraphicsPipelineIndirectVK(const RTRunGraphicsPipelineIndirectVK&) = delete;
    RTRunGraphicsPipelineIndirectVK(RTRunGraphicsPipelineIndirectVK&&) noexcept = delete;
    RTRunGraphicsPipelineIndirectVK& operator=(const RTRunGraphicsPipelineIndirectVK&) = delete;
    RTRunGraphicsPipelineIndirectVK& operator=(RTRunGraphicsPipelineIndirectVK&&) noexcept = delete;

    /**
     * @brief Construct a task from a complete builder configuration.
     */
    RTRunGraphicsPipelineIndirectVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual void bind_resources(const RendResViewVK& p_resources) override;

    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;
    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;
    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    Vector<VkClearValue, RenderAllocator> clear_values;
    VkAttachmentLoadOp color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentLoadOp depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkExtent2D render_extent{};
    Vector<std::string, RenderAllocator> color_attachment_ids;
    Vector<Ref<RenderObjectContainerVK>, RenderAllocator> color_attachment_containers;
    std::optional<std::string> depth_attachment_id;
    Ref<RenderObjectContainerVK> depth_attachment_container;
    std::string vertex_buffer_id;
    std::string index_buffer_id;
    std::string indirect_buffer_id;
    Ref<RenderObjectContainerVK> vertex_buffer_container;
    Ref<RenderObjectContainerVK> index_buffer_container;
    Ref<RenderObjectContainerVK> indirect_buffer_container;
    Vector<PipelineBatch, RenderAllocator> pipeline_batches;
    Vector<Unique<PipelineDescriptorBindingsVK>, RenderAllocator> descriptor_bindings;

    void initialize_descriptor_bindings();
};

} // namespace WhiteBirdEngine

#endif
