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
#ifndef WBE_FILE_RT_RUN_GRAPHICS_PIPELINE_VK_HH
#define WBE_FILE_RT_RUN_GRAPHICS_PIPELINE_VK_HH

#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/pipeline_desc_bindings_vk.hh"
#include "function/rend_vk/rend_graph_context_vk.hh"
#include "function/rend_vk/rend_graph_vk.hh"
#include "function/rend_vk/rend_object_container_vk.hh"
#include "function/rend_vk/rend_targets_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_graphics_pipeline_vk.hh"
#include "function/rend_vk/rt_params_run_graphics_pipeline_vk.hh"
#include "function/rend_vk/vertex_input_description_vk.hh"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class IRendGraph;

struct IndexedDrawRegionVK {
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    int32_t vertex_offset = 0;
};

/**
 * @struct VertexBufferSourceVK
 * @brief A task-local vertex buffer resource and the byte offset bound for it. Copyable; the id must outlive the call it is
 * passed to.
 */
struct VertexBufferSourceVK {
    /** Task-local vertex buffer resource id. */
    std::string_view resource_id;
    /** Byte offset bound for the buffer. */
    VkDeviceSize offset = 0;
};

/**
 * @class RTRunGraphicsPipelineVK
 * @brief Render task that runs one graphics pipeline draw.
 *
 * The task renders into `rend_target_colors[<index>]` and `rend_target_depth` following the pipeline's attachment
 * layout, and reports those accesses, its vertex and index buffers, and its reflected shader resources. Map them to graph
 * resources with RendTaskDescVK::alias().
 */
struct WBE_META(WBE_REND_TASK(draw), WBE_SERIALIZATION_ALIASES(run_graphics_pipeline)) RTRunGraphicsPipelineVK final : public IRendTaskVK {
    /**
     * @class Builder
     * @brief Configures a graphics draw. Copyable and movable; its allocator must outlive every copy.
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
         * @brief Set the graphics pipeline retained by the task.
         */
        Builder& set_pipeline(Ref<ROGraphicsPipelineVK> p_pipeline);

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
         * @brief Draw indexed vertices read from task-local vertex buffers, one per vertex-input binding.
         * @param p_vertex_buffers The buffer bound for each vertex-input binding, in binding order; there must be one per
         * binding of p_vertex_input.
         * @param p_index_buffer_id Index buffer resource.
         * @param p_vertex_input Dynamic vertex-input state.
         * @param p_indexed_draw_region Optional subregion; the whole index buffer is drawn when omitted.
         * @throws std::runtime_error If a resource id is empty or the buffer and binding counts differ.
         */
        Builder& set_indexed_draw(std::span<const VertexBufferSourceVK> p_vertex_buffers,
            std::string_view p_index_buffer_id,
            VertexInputDescriptionVK p_vertex_input,
            std::optional<IndexedDrawRegionVK> p_indexed_draw_region = std::nullopt);

        /**
         * @brief Set the vertex count of a draw without vertex and index buffers. Defaults to three.
         */
        Builder& set_draw_vertex_count(uint32_t p_draw_vertex_count);

        /**
         * @brief Create an allocator-owned render task.
         * @throws std::runtime_error If the pipeline or render extent is missing, or clear values are too few.
         */
        Unique<RTRunGraphicsPipelineVK> create_unique(const RendContextVK* p_rend_data) const;

    private:
        friend RTRunGraphicsPipelineVK;

        Ref<ROGraphicsPipelineVK> pipeline;
        VkExtent2D rend_extent{};
        Vector<VkClearValue, RendAlloc> clear_values;
        VkAttachmentLoadOp color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentLoadOp depth_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        std::vector<std::string> vertex_buffer_ids;
        std::vector<VkDeviceSize> vertex_buffer_offsets;
        std::optional<std::string> index_buffer_id;
        std::optional<IndexedDrawRegionVK> indexed_draw_region;
        VertexInputDescriptionVK vertex_input;
        uint32_t draw_vertex_count = 3;
    };

    RTRunGraphicsPipelineVK() = delete;
    virtual ~RTRunGraphicsPipelineVK() override = default;
    RTRunGraphicsPipelineVK(const RTRunGraphicsPipelineVK&) = delete;
    RTRunGraphicsPipelineVK(RTRunGraphicsPipelineVK&&) noexcept = delete;
    RTRunGraphicsPipelineVK& operator=(const RTRunGraphicsPipelineVK&) = delete;
    RTRunGraphicsPipelineVK& operator=(RTRunGraphicsPipelineVK&&) noexcept = delete;

    /**
     * @brief Construct a task from a complete builder configuration.
     */
    RTRunGraphicsPipelineVK(const RendContextVK* p_rend_data, const Builder& p_builder);

    /**
     * @brief Create the task from recipe parameters; used by RendTaskRegistryVK.
     * @param p_rend_data Base render data.
     * @param p_rend_graph The graph the task is on, whose registered pipelines it uses.
     * @param p_params The recipe parameters.
     * @param p_context The graph context the task is created for.
     * @return The task.
     * @throws std::runtime_error If the render graph is null, no graphics pipeline is registered under p_params.pipeline, or the render extent or a clear value is invalid. The constructor's own
     * validation also applies.
     */
    WBE_META(WBE_REFLECT)
    static Unique<IRendTaskVK> create(
        const RendContextVK* p_rend_data, IRendGraph* p_rend_graph, const RTParamsRunGraphicsPipelineVK& p_params, const RendGraphContextVK& p_context);

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual void bind_resources(const RendResViewVK& p_resources) override;

    virtual void append_accesses(Vector<ResAccessVK, RendAlloc>& p_reads, Vector<ResAccessVK, RendAlloc>& p_writes) const override;

    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const RendContextVK* rend_data = nullptr;
    VkExtent2D rend_extent{};
    Ref<ROGraphicsPipelineVK> graphics_pipeline;
    RendTargetsVK rend_targets;
    Unique<PipelineDescBindingsVK> desc_bindings;
    std::vector<std::string> vertex_buffer_ids;
    std::vector<VkDeviceSize> vertex_buffer_offsets;
    std::optional<std::string> index_buffer_id;
    std::vector<Ref<RendObjectContainerVK>> vertex_buffer_containers;
    std::vector<VkBuffer> bound_vertex_buffers;
    Ref<RendObjectContainerVK> index_buffer_container;
    std::optional<IndexedDrawRegionVK> indexed_draw_region;
    VertexInputDescriptionVK vertex_input;
    uint32_t draw_vertex_count = 3;
};

} // namespace WhiteBirdEngine

#endif
