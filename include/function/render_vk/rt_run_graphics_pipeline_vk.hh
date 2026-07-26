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
#include "function/render/render_task.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "utils/defs.hh"
#include <glm/ext/vector_float4.hpp>
#include <optional>
#include <span>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTRunGraphicsPipelineVK
 * @brief Render task that runs a graphics pipeline and produces an image.
 *
 * Resource binding contract (declared in this exact order; the render graph
 * passes `bound_resources` to record() in the same order):
 *   reads:  [graphics_pipeline, (vertex_buffer?), (index_buffer?)]
 *   writes: [color_attachment..., (depth_attachment?)]
 * Therefore inside record(): index 0 = graphics_pipeline, then optionally
 * vertex_buffer and index_buffer, followed by attachments.
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
     * @param p_color_attachment_id Resource id of the color attachment image view (write).
     * @param p_pipeline_id        Resource id of the graphics pipeline (read).
     * @param p_vertex_buffer_id   Optional resource id of the vertex buffer (read).
     * @param p_index_buffer_id    Optional resource id of the index buffer (read).
     * @param p_descriptor_sets    Descriptor sets to bind in order.
     */
    RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data,
        const glm::vec4& p_clear_color,
        VkExtent2D p_render_extent,
        RendResID p_color_attachment_id,
        RendResID p_pipeline_id,
        std::optional<RendResID> p_vertex_buffer_id,
        std::optional<RendResID> p_index_buffer_id,
        std::span<const Ref<RODescriptorSetVK>> p_descriptor_sets);

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
     * @param p_color_attachment_ids Resource ids of color attachment image views (write).
     * @param p_depth_attachment_id Optional resource id of the depth attachment image view (write).
     * @param p_pipeline_id        Resource id of the graphics pipeline (read).
     * @param p_vertex_buffer_id   Optional resource id of the vertex buffer (read).
     * @param p_index_buffer_id    Optional resource id of the index buffer (read).
     * @param p_descriptor_sets    Descriptor sets to bind in order.
     */
    RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data,
        std::span<const VkClearValue> p_clear_values,
        VkExtent2D p_render_extent,
        std::span<const RendResID> p_color_attachment_ids,
        std::optional<RendResID> p_depth_attachment_id,
        RendResID p_pipeline_id,
        std::optional<RendResID> p_vertex_buffer_id,
        std::optional<RendResID> p_index_buffer_id,
        std::span<const Ref<RODescriptorSetVK>> p_descriptor_sets);

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;

    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;

    virtual void record(VkCommandBuffer p_command_buffer, const Vector<RendResVK, RenderAllocator>& p_resources, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    Vector<VkClearValue, RenderAllocator> clear_values;
    VkExtent2D render_extent{};
    Vector<RendResID, RenderAllocator> color_attachment_ids;
    std::optional<RendResID> depth_attachment_id;
    RendResID pipeline_id = 0;
    std::optional<RendResID> vertex_buffer_id;
    std::optional<RendResID> index_buffer_id;
    Vector<Ref<RODescriptorSetVK>, RenderAllocator> descriptor_sets;
    Vector<VkDescriptorSet, RenderAllocator> descriptor_sets_instances;
};

} // namespace WhiteBirdEngine

#endif
