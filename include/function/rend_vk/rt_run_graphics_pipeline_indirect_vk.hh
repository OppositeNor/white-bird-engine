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

#include "core/alloc/i_alloc.hh"
#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/pipeline_desc_bindings_vk.hh"
#include "function/rend_vk/rend_graph_context_vk.hh"
#include "function/rend_vk/rend_graph_vk.hh"
#include "function/render/i_rend_object_container.hh"
#include "function/rend_vk/rend_targets_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_graphics_pipeline_vk.hh"
#include "function/rend_vk/rt_params_run_graphics_pipeline_indirect_vk.hh"
#include "function/rend_vk/vertex_input_description_vk.hh"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class IRendGraph;

/**
 * @class RTRunGraphicsPipelineIndirectVK
 * @brief Render task that submits indexed draw commands from a GPU buffer in one Vulkan call.
 *
 * Every pipeline batch renders into `rend_target_colors[<index>]` and `rend_target_depth` following the pipelines'
 * shared attachment layout. The task reports those accesses, its draw buffers, and every batch's reflected shader resources;
 * map them to graph resources with RendTaskDescVK::alias().
 */
struct WBE_META(WBE_REND_TASK(draw_indirect), WBE_SERIALIZATION_ALIASES(run_graphics_pipeline_indirect))
    RTRunGraphicsPipelineIndirectVK final : public IRendTaskVK {
    /**
     * @class PipelineBatch
     * @brief One graphics pipeline and its contiguous range of indirect commands.
     */
    struct PipelineBatch {
        PipelineBatch() = default;
        ~PipelineBatch() = default;
        PipelineBatch(const PipelineBatch&) = default;
        PipelineBatch(PipelineBatch&&) noexcept = default;
        PipelineBatch& operator=(const PipelineBatch&) = default;
        PipelineBatch& operator=(PipelineBatch&&) noexcept = default;

        /** Graphics pipeline retained directly by the task. */
        Ref<ROGraphicsPipelineVK> graphics_pipeline = MEM_NULL;
        /** Index of the command buffer the range reads, in the order the buffers were added to the builder. */
        uint32_t indirect_buffer_index = 0;
        /** First tightly packed indirect command in the batch's command buffer. */
        uint32_t first_draw = 0;
        /** Number of indirect commands rendered by this pipeline. */
        uint32_t draw_count = 0;
        /** Dynamic vertex-input state used by this command range. */
        VertexInputDescriptionVK vertex_input;
        /** The task vertex buffer and offset bound for each vertex-input binding, parallel to vertex_input.bindings. */
        std::vector<VertexBufferBindingVK> vertex_buffers;
    };

    /**
     * @class Builder
     * @brief Configures an indexed-indirect graphics task without constructor variants.
     *
     * The render targets follow the pipelines' attachment layout; pipelines without attachments create an attachment-less
     * dynamic-rendering task. Draw buffers are identified by task-local resource ids and resolved when the graph is built.
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
        explicit Builder(RendAlloc* p_alloc);

        /**
         * @brief Set the dynamic-rendering area.
         */
        Builder& set_rend_extent(VkExtent2D p_rend_extent);

        /**
         * @brief Set clear values for the color targets followed by the depth target. Empty clears to zero.
         */
        Builder& set_clear_values(std::span<const VkClearValue> p_clear_values);

        /**
         * @brief Set the load operation shared by color targets.
         */
        Builder& set_color_load_op(VkAttachmentLoadOp p_load_op);

        /**
         * @brief Set the load operation of the depth target.
         */
        Builder& set_depth_load_op(VkAttachmentLoadOp p_load_op);

        /**
         * @brief Append a vertex buffer resolved from a graph resource; batches select buffers by their index in append
         * order.
         *
         * Omit vertex buffers for vertex shaders that derive all positions from vertex and instance IDs.
         */
        Builder& add_vertex_buffer(std::string_view p_resource_id);

        /**
         * @brief Resolve the index buffer from a graph resource.
         */
        Builder& set_index_buffer(std::string_view p_resource_id);

        /**
         * @brief Use one graph resource as the only indexed-indirect command buffer, replacing any added before.
         */
        Builder& set_indirect_buffer(std::string_view p_resource_id);

        /**
         * @brief Add an indexed-indirect command buffer, selected by PipelineBatch::indirect_buffer_index.
         * @return The buffer's index.
         * @throws std::runtime_error If p_resource_id is empty.
         */
        uint32_t add_indirect_buffer(std::string_view p_resource_id);

        /**
         * @brief Replace the draw plan with one vertex-less graphics pipeline batch beginning at command zero.
         *
         * Batches with a vertex input are added through add_pipeline_batch(PipelineBatch).
         */
        Builder& set_pipeline(Ref<ROGraphicsPipelineVK> p_pipeline, uint32_t p_draw_count);

        /**
         * @brief Append one vertex-less graphics pipeline batch.
         *
         * Batches with a vertex input are added through add_pipeline_batch(PipelineBatch).
         */
        Builder& add_pipeline_batch(Ref<ROGraphicsPipelineVK> p_pipeline, uint32_t p_first_draw, uint32_t p_draw_count);

        /**
         * @brief Append one preconfigured graphics pipeline batch.
         */
        Builder& add_pipeline_batch(PipelineBatch p_batch);

        /**
         * @brief Create an allocator-owned render task.
         */
        Unique<RTRunGraphicsPipelineIndirectVK> create_unique(const RendContextVK* p_rend_data) const;

    private:
        friend RTRunGraphicsPipelineIndirectVK;

        RendAlloc* alloc = nullptr;
        VkExtent2D rend_extent{};
        Vector<VkClearValue, RendAlloc> clear_values;
        VkAttachmentLoadOp color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentLoadOp depth_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        Vector<std::string, RendAlloc> vertex_buffer_ids;
        std::string index_buffer_id;
        Vector<std::string, RendAlloc> indirect_buffer_ids;
        Vector<PipelineBatch, RendAlloc> pipeline_batches;
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
    RTRunGraphicsPipelineIndirectVK(const RendContextVK* p_rend_data, const Builder& p_builder);

    /**
     * @brief Create the task from recipe parameters, as one batch of the pipeline without vertex buffers; used by
     * RendTaskRegistryVK.
     * @throws std::runtime_error If the pipeline is not registered on the render graph, or the parameters do not make a valid
     * builder (see Builder::create_unique()).
     */
    WBE_META(WBE_REFLECT)
    static Unique<IRendTaskVK> create(
        const RendContextVK* p_rend_data, IRendGraph* p_rend_graph, const RTParamsRunGraphicsPipelineIndirectVK& p_params, const RendGraphContextVK& p_context);

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual void bind_resources(const RendResViewVK& p_resources) override;

    virtual void append_accesses(Vector<ResAccessVK, RendAlloc>& p_reads, Vector<ResAccessVK, RendAlloc>& p_writes) const override;

    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const RendContextVK* rend_data = nullptr;
    VkExtent2D rend_extent{};
    Vector<PipelineBatch, RendAlloc> pipeline_batches;
    RendTargetsVK rend_targets;
    Vector<std::string, RendAlloc> vertex_buffer_ids;
    std::string index_buffer_id;
    Vector<std::string, RendAlloc> indirect_buffer_ids;
    Vector<Ref<IRendObjectContainer>, RendAlloc> vertex_buffer_containers;
    // Per-record scratch storage for the buffers and offsets bound by one batch, sized for the largest batch.
    Vector<VkBuffer, RendAlloc> bound_vertex_buffers;
    Vector<VkDeviceSize, RendAlloc> bound_vertex_buffer_offsets;
    Ref<IRendObjectContainer> index_buffer_container;
    Vector<Ref<IRendObjectContainer>, RendAlloc> indirect_buffer_containers;
    Vector<Unique<PipelineDescBindingsVK>, RendAlloc> desc_bindings;

    void init_desc_bindings();
};

} // namespace WhiteBirdEngine

#endif
