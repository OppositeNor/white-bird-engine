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
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstddef>
#include <cstdint>
#include <glm/ext/vector_float4.hpp>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunGraphicsPipelineVK::RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data, // NOLINT
    const glm::vec4& p_clear_color,
    VkExtent2D p_render_extent,
    std::string_view p_color_attachment_id,
    std::string_view p_pipeline_id,
    std::optional<std::string_view> p_vertex_buffer_id,
    std::optional<std::string_view> p_index_buffer_id,
    std::optional<IndexedDrawRegionVK> p_indexed_draw_region,
    VkAttachmentLoadOp p_color_attachment_load_op,
    VertexInputDescriptionVK p_vertex_input,
    VkDeviceSize p_vertex_buffer_offset)
    : render_data(p_render_data), clear_values(p_render_data->allocator), color_attachment_load_op(p_color_attachment_load_op),
      render_extent(p_render_extent), color_attachment_ids(p_render_data->allocator), pipeline_id(p_pipeline_id),
      vertex_buffer_id(p_vertex_buffer_id.has_value() ? std::optional<std::string>(*p_vertex_buffer_id) : std::nullopt),
      index_buffer_id(p_index_buffer_id.has_value() ? std::optional<std::string>(*p_index_buffer_id) : std::nullopt),
      indexed_draw_region(p_indexed_draw_region), vertex_input(std::move(p_vertex_input)),
      vertex_buffer_offset(p_vertex_buffer_offset) {
    clear_values.push_back(
        VkClearValue{.color = {.float32 = {p_clear_color.r, p_clear_color.g, p_clear_color.b, p_clear_color.a}}});
    color_attachment_ids.emplace_back(p_color_attachment_id);
}

RTRunGraphicsPipelineVK::RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data, // NOLINT
    std::span<const VkClearValue> p_clear_values,
    VkExtent2D p_render_extent,
    std::span<const std::string_view> p_color_attachment_ids,
    std::optional<std::string_view> p_depth_attachment_id,
    std::string_view p_pipeline_id,
    std::optional<std::string_view> p_vertex_buffer_id,
    std::optional<std::string_view> p_index_buffer_id,
    std::optional<IndexedDrawRegionVK> p_indexed_draw_region,
    VkAttachmentLoadOp p_color_attachment_load_op,
    VkAttachmentLoadOp p_depth_attachment_load_op,
    VertexInputDescriptionVK p_vertex_input,
    VkDeviceSize p_vertex_buffer_offset)
    : render_data(p_render_data), clear_values(p_clear_values.begin(), p_clear_values.end(), p_render_data->allocator),
      color_attachment_load_op(p_color_attachment_load_op), depth_attachment_load_op(p_depth_attachment_load_op),
      render_extent(p_render_extent),
      color_attachment_ids(p_color_attachment_ids.begin(), p_color_attachment_ids.end(), p_render_data->allocator),
      depth_attachment_id(p_depth_attachment_id.has_value() ? std::optional<std::string>(*p_depth_attachment_id) : std::nullopt),
      pipeline_id(p_pipeline_id),
      vertex_buffer_id(p_vertex_buffer_id.has_value() ? std::optional<std::string>(*p_vertex_buffer_id) : std::nullopt),
      index_buffer_id(p_index_buffer_id.has_value() ? std::optional<std::string>(*p_index_buffer_id) : std::nullopt),
      indexed_draw_region(p_indexed_draw_region), vertex_input(std::move(p_vertex_input)),
      vertex_buffer_offset(p_vertex_buffer_offset) {
}

Vector<ResAccessVK, RenderAllocator> RTRunGraphicsPipelineVK::read_resources() const {
    Vector<ResAccessVK, RenderAllocator> read_accesses(render_data->allocator);
    if (color_attachment_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
        for (const std::string& color_attachment_id : color_attachment_ids) {
            read_accesses.push_back({.res_id = color_attachment_id, .usage = RendResUsageVK::COLOR_ATTACHMENT});
        }
    }
    if (depth_attachment_id.has_value() && depth_attachment_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
        read_accesses.push_back({.res_id = *depth_attachment_id, .usage = RendResUsageVK::DEPTH_ATTACHMENT});
    }
    if (vertex_buffer_id.has_value()) {
        read_accesses.push_back({.res_id = *vertex_buffer_id, .usage = RendResUsageVK::VERTEX_BUFFER});
    }
    if (index_buffer_id.has_value()) {
        read_accesses.push_back({.res_id = *index_buffer_id, .usage = RendResUsageVK::INDEX_BUFFER});
    }
    return read_accesses;
}

void RTRunGraphicsPipelineVK::validate_resources(const RendResViewVK& p_resources) const {
    Ref<ROGraphicsPipelineVK> graphics_pipeline =
        p_resources.get(pipeline_id).container->get().dynamic_cast_ref<ROGraphicsPipelineVK>();
    if (graphics_pipeline == MEM_NULL) {
        throw std::runtime_error("RTRunGraphicsPipelineVK: graphics pipeline resource has the wrong type.");
    }
}

Vector<ResAccessVK, RenderAllocator> RTRunGraphicsPipelineVK::write_resources() const {
    Vector<ResAccessVK, RenderAllocator> write_accesses(render_data->allocator);
    write_accesses.reserve(color_attachment_ids.size() + (depth_attachment_id.has_value() ? 1 : 0));
    for (const std::string& color_attachment_id : color_attachment_ids) {
        write_accesses.push_back({.res_id = color_attachment_id, .usage = RendResUsageVK::COLOR_ATTACHMENT});
    }
    if (depth_attachment_id.has_value()) {
        write_accesses.push_back({.res_id = *depth_attachment_id, .usage = RendResUsageVK::DEPTH_ATTACHMENT});
    }
    return write_accesses;
}

void RTRunGraphicsPipelineVK::record( // NOLINT
    VkCommandBuffer p_command_buffer,
    const RendResViewVK& p_resources,
    double /*p_delta*/) {
    auto graphics_pipeline =
        WBE_REQUIRES_VALID(p_resources.get(pipeline_id).container->get().dynamic_cast_ref<ROGraphicsPipelineVK>());
    Ref<ROBufferVK> vertex_buffer = MEM_NULL;
    Ref<ROBufferVK> index_buffer = MEM_NULL;
    if (vertex_buffer_id.has_value()) {
        vertex_buffer = WBE_REQUIRES_VALID(p_resources.get(*vertex_buffer_id).container->get().dynamic_cast_ref<ROBufferVK>());
    }
    if (index_buffer_id.has_value()) {
        index_buffer = WBE_REQUIRES_VALID(p_resources.get(*index_buffer_id).container->get().dynamic_cast_ref<ROBufferVK>());
    }

    // Begin render.
    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = {.offset = {.x = 0, .y = 0}, .extent = render_extent};
    rendering_info.layerCount = 1;
    const size_t color_attachment_count = graphics_pipeline->get_config().attachment_config.color_formats.size();
    if (color_attachment_ids.size() != color_attachment_count) {
        throw std::runtime_error("RTRunGraphicsPipelineVK: color attachment count does not match graphics pipeline.");
    }
    const size_t expected_clear_value_count = color_attachment_count + (depth_attachment_id.has_value() ? 1 : 0);
    if (clear_values.size() < expected_clear_value_count) {
        throw std::runtime_error("RTRunGraphicsPipelineVK: insufficient clear values for attachments.");
    }
    Vector<VkRenderingAttachmentInfo, RenderAllocator> color_attachments(render_data->allocator);
    color_attachments.reserve(color_attachment_count);
    for (size_t i = 0; i < color_attachment_count; ++i) {
        auto color_attachment =
            WBE_REQUIRES_VALID(p_resources.get(color_attachment_ids[i]).container->get().dynamic_cast_ref<ROImageViewVK>());
        color_attachments.push_back(ROImageViewVK::AttachmentBuilder::color(color_attachment->get_image_view())
                .set_load_op(color_attachment_load_op)
                .set_clear_value(clear_values[i])
                .build());
    }
    rendering_info.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size());
    rendering_info.pColorAttachments = color_attachments.data();
    VkRenderingAttachmentInfo depth_attachment_info{};
    if (depth_attachment_id.has_value()) {
        auto depth_attachment =
            WBE_REQUIRES_VALID(p_resources.get(*depth_attachment_id).container->get().dynamic_cast_ref<ROImageViewVK>());
        depth_attachment_info = ROImageViewVK::AttachmentBuilder::depth(depth_attachment->get_image_view())
                                    .set_load_op(depth_attachment_load_op)
                                    .set_store_op(VK_ATTACHMENT_STORE_OP_STORE)
                                    .set_clear_value(clear_values[color_attachment_count])
                                    .build();
        rendering_info.pDepthAttachment = &depth_attachment_info;
    }
    vkCmdBeginRendering(p_command_buffer, &rendering_info);

    vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline->get_pipeline());
    VkViewport viewport = render_data->viewport;
    viewport.width = static_cast<float>(render_extent.width);
    viewport.height = static_cast<float>(render_extent.height);
    vkCmdSetViewport(p_command_buffer, 0, 1, &viewport);
    VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = render_extent};
    vkCmdSetScissor(p_command_buffer, 0, 1, &scissor);

    graphics_pipeline->bind_descriptor_sets(p_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

    if (vertex_buffer != MEM_NULL && index_buffer != MEM_NULL) {
        if (render_data->command_set_vertex_input == nullptr) {
            throw std::runtime_error("RTRunGraphicsPipelineVK: dynamic vertex-input command is unavailable.");
        }
        VkBuffer vertex_buffer_handle = vertex_buffer->get_buffer();
        VkBuffer index_buffer_handle = index_buffer->get_buffer();
        vkCmdBindVertexBuffers(p_command_buffer, VERTEX_BUFFER_BINDPOINT, 1, &vertex_buffer_handle, &vertex_buffer_offset);
        render_data->command_set_vertex_input(p_command_buffer,
            static_cast<uint32_t>(vertex_input.bindings.size()),
            vertex_input.bindings.data(),
            static_cast<uint32_t>(vertex_input.attributes.size()),
            vertex_input.attributes.data());
        vkCmdBindIndexBuffer(p_command_buffer, index_buffer_handle, 0, VK_INDEX_TYPE_UINT32);
        uint32_t full_index_count = static_cast<uint32_t>(index_buffer->fields().buffer_size / sizeof(uint32_t));
        uint32_t first_index = 0;
        uint32_t index_count = full_index_count;
        int32_t vertex_offset = 0;
        if (indexed_draw_region.has_value()) {
            first_index = indexed_draw_region->first_index;
            index_count = indexed_draw_region->index_count;
            vertex_offset = indexed_draw_region->vertex_offset;
            if (first_index > full_index_count || index_count > full_index_count - first_index) {
                throw std::runtime_error("RTRunGraphicsPipelineVK: indexed draw region exceeds index buffer bounds.");
            }
        }
        vkCmdDrawIndexed(p_command_buffer, index_count, 1, first_index, vertex_offset, 0);
    } else {
        // Screen-triangle / no vertex binding case.
        if (render_data->command_set_vertex_input == nullptr) {
            throw std::runtime_error("RTRunGraphicsPipelineVK: dynamic vertex-input command is unavailable.");
        }
        render_data->command_set_vertex_input(p_command_buffer, 0, nullptr, 0, nullptr);
        vkCmdDraw(p_command_buffer, 3, 1, 0, 0);
    }

    vkCmdEndRendering(p_command_buffer);
}

} // namespace WhiteBirdEngine
