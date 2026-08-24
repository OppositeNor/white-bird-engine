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

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include <cstdint>
#include <glm/ext/vector_float4.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTRunGraphicsPipelineIndirectVK
 * @brief Render task that submits indexed draw commands from a GPU buffer in one Vulkan call.
 *
 * The task retains its descriptor sets, while graph resources are resolved by their declared ids.
 */
struct RTRunGraphicsPipelineIndirectVK final : public RenderTaskVK {
    /**
     * @class PipelineBatch
     * @brief One graphics pipeline and its contiguous range of indirect commands.
     */
    struct PipelineBatch {
        explicit PipelineBatch(RenderAllocator* p_allocator) : descriptor_sets(p_allocator) {
        }
        ~PipelineBatch() = default;
        PipelineBatch(const PipelineBatch&) = default;
        PipelineBatch(PipelineBatch&&) noexcept = default;
        PipelineBatch& operator=(const PipelineBatch&) = default;
        PipelineBatch& operator=(PipelineBatch&&) noexcept = default;

        /** Graphics-pipeline render-graph resource id. */
        std::string pipeline_id;
        /** Optional task-owned graphics pipeline used instead of pipeline_id. */
        Ref<ROGraphicsPipelineVK> graphics_pipeline = MEM_NULL;
        /** First tightly packed indirect command in the shared command buffer. */
        uint32_t first_draw = 0;
        /** Number of indirect commands rendered by this pipeline. */
        uint32_t draw_count = 0;
        /** Descriptor sets bound in set order for this pipeline. */
        Vector<Ref<RODescriptorSetVK>, RenderAllocator> descriptor_sets;
    };

    RTRunGraphicsPipelineIndirectVK() = delete;
    virtual ~RTRunGraphicsPipelineIndirectVK() override = default;
    RTRunGraphicsPipelineIndirectVK(const RTRunGraphicsPipelineIndirectVK&) = delete;
    RTRunGraphicsPipelineIndirectVK(RTRunGraphicsPipelineIndirectVK&&) noexcept = delete;
    RTRunGraphicsPipelineIndirectVK& operator=(const RTRunGraphicsPipelineIndirectVK&) = delete;
    RTRunGraphicsPipelineIndirectVK& operator=(RTRunGraphicsPipelineIndirectVK&&) noexcept = delete;

    /**
     * @brief Construct a task with one color attachment.
     *
     * @param p_render_data Renderer state used while recording.
     * @param p_clear_color Clear value for the color attachment.
     * @param p_render_extent Render area extent.
     * @param p_color_attachment_id Color attachment graph resource id.
     * @param p_pipeline_id Graphics pipeline graph resource id.
     * @param p_vertex_buffer_id Vertex buffer graph resource id.
     * @param p_index_buffer_id Index buffer graph resource id.
     * @param p_indirect_buffer_id Indexed-indirect command buffer graph resource id.
     * @param p_draw_count Number of tightly packed VkDrawIndexedIndirectCommand records to execute.
     * @param p_descriptor_sets Descriptor sets bound in set order.
     * @param p_color_attachment_load_op Color attachment load operation.
     */
    RTRunGraphicsPipelineIndirectVK(const BaseRenderDataVK* p_render_data,
        const glm::vec4& p_clear_color,
        VkExtent2D p_render_extent,
        std::string_view p_color_attachment_id,
        std::string_view p_pipeline_id,
        std::string_view p_vertex_buffer_id,
        std::string_view p_index_buffer_id,
        std::string_view p_indirect_buffer_id,
        uint32_t p_draw_count,
        std::span<const Ref<RODescriptorSetVK>> p_descriptor_sets,
        VkAttachmentLoadOp p_color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR);

    /**
     * @brief Construct a task with multiple color attachments and an optional depth attachment.
     *
     * @param p_render_data Renderer state used while recording.
     * @param p_clear_values Color clear values followed by the depth clear value when present.
     * @param p_render_extent Render area extent.
     * @param p_color_attachment_ids Color attachment graph resource ids in pipeline order.
     * @param p_depth_attachment_id Optional depth attachment graph resource id.
     * @param p_pipeline_id Graphics pipeline graph resource id.
     * @param p_vertex_buffer_id Vertex buffer graph resource id.
     * @param p_index_buffer_id Index buffer graph resource id.
     * @param p_indirect_buffer_id Indexed-indirect command buffer graph resource id.
     * @param p_draw_count Number of tightly packed VkDrawIndexedIndirectCommand records to execute.
     * @param p_descriptor_sets Descriptor sets bound in set order.
     * @param p_color_attachment_load_op Load operation used for every color attachment.
     * @param p_depth_attachment_load_op Depth attachment load operation.
     */
    RTRunGraphicsPipelineIndirectVK(const BaseRenderDataVK* p_render_data,
        std::span<const VkClearValue> p_clear_values,
        VkExtent2D p_render_extent,
        std::span<const std::string_view> p_color_attachment_ids,
        std::optional<std::string_view> p_depth_attachment_id,
        std::string_view p_pipeline_id,
        std::string_view p_vertex_buffer_id,
        std::string_view p_index_buffer_id,
        std::string_view p_indirect_buffer_id,
        uint32_t p_draw_count,
        std::span<const Ref<RODescriptorSetVK>> p_descriptor_sets,
        VkAttachmentLoadOp p_color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentLoadOp p_depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR);

    /**
     * @brief Construct a task with task-owned pipelines and an indirect-command buffer.
     *
     * Each pipeline batch must provide graphics_pipeline. The immutable pipelines and command buffer are retained directly
     * and are therefore not declared as render-graph resources.
     *
     * @param p_render_data Renderer state used while recording.
     * @param p_clear_values Color clear values followed by depth when present.
     * @param p_render_extent Render area extent.
     * @param p_color_attachment_ids Color attachment graph resource ids.
     * @param p_depth_attachment_id Optional depth attachment graph resource id.
     * @param p_vertex_buffer_id Vertex buffer graph resource id.
     * @param p_index_buffer_id Index buffer graph resource id.
     * @param p_indirect_buffer Task-owned indexed-indirect command buffer.
     * @param p_pipeline_batches Ordered task-owned pipeline batches covering the command buffer.
     * @param p_color_attachment_load_op Load operation used for every color attachment.
     * @param p_depth_attachment_load_op Depth attachment load operation.
     */
    RTRunGraphicsPipelineIndirectVK(const BaseRenderDataVK* p_render_data,
        std::span<const VkClearValue> p_clear_values,
        VkExtent2D p_render_extent,
        std::span<const std::string_view> p_color_attachment_ids,
        std::optional<std::string_view> p_depth_attachment_id,
        std::string_view p_vertex_buffer_id,
        std::string_view p_index_buffer_id,
        Ref<ROBufferVK> p_indirect_buffer,
        std::span<const PipelineBatch> p_pipeline_batches,
        VkAttachmentLoadOp p_color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentLoadOp p_depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR);

    /**
     * @brief Construct one render task containing multiple contiguous graphics-pipeline batches.
     *
     * @param p_render_data Renderer state used while recording.
     * @param p_clear_values Color clear values followed by depth when present.
     * @param p_render_extent Render area extent.
     * @param p_color_attachment_ids Color attachment graph resource ids.
     * @param p_depth_attachment_id Optional depth attachment graph resource id.
     * @param p_vertex_buffer_id Vertex buffer graph resource id.
     * @param p_index_buffer_id Index buffer graph resource id.
     * @param p_indirect_buffer_id Shared indexed-indirect command buffer graph resource id.
     * @param p_pipeline_batches Ordered pipeline batches covering the command buffer.
     * @param p_color_attachment_load_op Load operation used for every color attachment.
     * @param p_depth_attachment_load_op Depth attachment load operation.
     */
    RTRunGraphicsPipelineIndirectVK(const BaseRenderDataVK* p_render_data,
        std::span<const VkClearValue> p_clear_values,
        VkExtent2D p_render_extent,
        std::span<const std::string_view> p_color_attachment_ids,
        std::optional<std::string_view> p_depth_attachment_id,
        std::string_view p_vertex_buffer_id,
        std::string_view p_index_buffer_id,
        std::string_view p_indirect_buffer_id,
        std::span<const PipelineBatch> p_pipeline_batches,
        VkAttachmentLoadOp p_color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentLoadOp p_depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR);

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;
    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;
    virtual void record(VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    Vector<VkClearValue, RenderAllocator> clear_values;
    VkAttachmentLoadOp color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentLoadOp depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkExtent2D render_extent{};
    Vector<std::string, RenderAllocator> color_attachment_ids;
    std::optional<std::string> depth_attachment_id;
    std::string vertex_buffer_id;
    std::string index_buffer_id;
    std::string indirect_buffer_id;
    Ref<ROBufferVK> indirect_buffer = MEM_NULL;
    Vector<PipelineBatch, RenderAllocator> pipeline_batches;
    Vector<VkDescriptorSet, RenderAllocator> descriptor_set_instances;
};

} // namespace WhiteBirdEngine

#endif
