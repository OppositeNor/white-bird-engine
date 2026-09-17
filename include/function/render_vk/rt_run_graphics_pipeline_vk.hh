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

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstdint>
#include <glm/ext/vector_float4.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

struct IndexedDrawRegionVK {
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    int32_t vertex_offset = 0;
};

/**
 * @class RTRunGraphicsPipelineVK
 * @brief Render task that runs a graphics pipeline and produces an image.
 *
 * Graph resources are bound to stable containers when the graph is built.
 */
struct RTRunGraphicsPipelineVK final : public RenderTaskVK {
    RTRunGraphicsPipelineVK() = delete;
    virtual ~RTRunGraphicsPipelineVK() override = default;
    RTRunGraphicsPipelineVK(const RTRunGraphicsPipelineVK&) = delete;
    RTRunGraphicsPipelineVK(RTRunGraphicsPipelineVK&&) noexcept = delete;
    RTRunGraphicsPipelineVK& operator=(const RTRunGraphicsPipelineVK&) = delete;
    RTRunGraphicsPipelineVK& operator=(RTRunGraphicsPipelineVK&&) noexcept = delete;

    /**
     * @brief Constructor (single-color attachment convenience).
     *
     * @param p_render_data        Base render data.
     * @param p_clear_color        Clear color used in the render pass begin.
     * @param p_render_extent      Render area extent.
     * @param p_color_attachment_id Resource id of the color attachment image view (read/write when loaded).
     * @param p_graphics_pipeline Graphics pipeline retained directly by the task.
     * @param p_color_attachment_load_op Load operation for the color attachment.
     */
    RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data,
        const glm::vec4& p_clear_color,
        VkExtent2D p_render_extent,
        std::string_view p_color_attachment_id,
        Ref<ROGraphicsPipelineVK> p_graphics_pipeline,
        VkAttachmentLoadOp p_color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR);

    /**
     * @brief Constructor (single-color attachment convenience).
     *
     * @param p_render_data        Base render data.
     * @param p_clear_color        Clear color used in the render pass begin.
     * @param p_render_extent      Render area extent.
     * @param p_color_attachment_id Resource id of the color attachment image view (read/write when loaded).
     * @param p_graphics_pipeline Graphics pipeline retained directly by the task.
     * @param p_vertex_buffer_id   Optional resource id of the vertex buffer (read).
     * @param p_index_buffer_id    Optional resource id of the index buffer (read).
     * @param p_indexed_draw_region Optional indexed subregion to draw.
     * @param p_color_attachment_load_op Load operation for the color attachment.
     * @param p_vertex_input Dynamic vertex-input state; empty for a vertex-less draw.
     * @param p_vertex_buffer_offset Byte offset bound for the vertex buffer.
     */
    RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data,
        const glm::vec4& p_clear_color,
        VkExtent2D p_render_extent,
        std::string_view p_color_attachment_id,
        Ref<ROGraphicsPipelineVK> p_graphics_pipeline,
        std::optional<std::string_view> p_vertex_buffer_id,
        std::optional<std::string_view> p_index_buffer_id,
        std::optional<IndexedDrawRegionVK> p_indexed_draw_region = std::nullopt,
        VkAttachmentLoadOp p_color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VertexInputDescriptionVK p_vertex_input = {},
        VkDeviceSize p_vertex_buffer_offset = 0);

    /**
     * @brief Constructor (multi-attachment).
     *
     * Use this overload when rendering to multiple color attachments and/or a
     * depth attachment. The number and order of color attachment ids must
     * match the graphics pipeline color formats.
     *
     * @param p_render_data        Base render data.
     * @param p_clear_values       Clear values for color attachments, followed by depth when present.
     * @param p_render_extent      Render area extent.
     * @param p_color_attachment_ids Resource ids of color attachment image views (read/write when loaded).
     * @param p_depth_attachment_id Optional resource id of the depth attachment image view (read/write when loaded).
     * @param p_graphics_pipeline Graphics pipeline retained directly by the task.
     * @param p_vertex_buffer_id   Optional resource id of the vertex buffer (read).
     * @param p_index_buffer_id    Optional resource id of the index buffer (read).
     * @param p_indexed_draw_region Optional indexed subregion to draw.
     * @param p_color_attachment_load_op Load operation for every color attachment.
     * @param p_depth_attachment_load_op Load operation for the depth attachment.
     * @param p_vertex_input Dynamic vertex-input state; empty for a vertex-less draw.
     * @param p_vertex_buffer_offset Byte offset bound for the vertex buffer.
     */
    RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data,
        std::span<const VkClearValue> p_clear_values,
        VkExtent2D p_render_extent,
        std::span<const std::string_view> p_color_attachment_ids,
        std::optional<std::string_view> p_depth_attachment_id,
        Ref<ROGraphicsPipelineVK> p_graphics_pipeline,
        std::optional<std::string_view> p_vertex_buffer_id,
        std::optional<std::string_view> p_index_buffer_id,
        std::optional<IndexedDrawRegionVK> p_indexed_draw_region = std::nullopt,
        VkAttachmentLoadOp p_color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentLoadOp p_depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VertexInputDescriptionVK p_vertex_input = {},
        VkDeviceSize p_vertex_buffer_offset = 0);

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
    Ref<ROGraphicsPipelineVK> graphics_pipeline;
    std::optional<std::string> vertex_buffer_id;
    std::optional<std::string> index_buffer_id;
    Ref<RenderObjectContainerVK> vertex_buffer_container;
    Ref<RenderObjectContainerVK> index_buffer_container;
    std::optional<IndexedDrawRegionVK> indexed_draw_region;
    VertexInputDescriptionVK vertex_input;
    VkDeviceSize vertex_buffer_offset = 0;
};

} // namespace WhiteBirdEngine

#endif
