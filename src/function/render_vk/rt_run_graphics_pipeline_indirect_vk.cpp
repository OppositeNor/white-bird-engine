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
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
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
#include "glm/ext/vector_float4.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunGraphicsPipelineIndirectVK::RTRunGraphicsPipelineIndirectVK(const BaseRenderDataVK* p_render_data, // NOLINT
    const glm::vec4& p_clear_color,
    VkExtent2D p_render_extent,
    std::string_view p_color_attachment_id,
    std::string_view p_pipeline_id,
    std::string_view p_vertex_buffer_id,
    std::string_view p_index_buffer_id,
    std::string_view p_indirect_buffer_id,
    uint32_t p_draw_count,
    VkAttachmentLoadOp p_color_attachment_load_op,
    VertexInputDescriptionVK p_vertex_input,
    VkDeviceSize p_vertex_buffer_offset)
    : render_data(p_render_data), clear_values(p_render_data->allocator), color_attachment_load_op(p_color_attachment_load_op),
      render_extent(p_render_extent), color_attachment_ids(p_render_data->allocator), vertex_buffer_id(p_vertex_buffer_id),
      index_buffer_id(p_index_buffer_id), indirect_buffer_id(p_indirect_buffer_id), pipeline_batches(p_render_data->allocator) {
    clear_values.push_back(
        VkClearValue{.color = {.float32 = {p_clear_color.r, p_clear_color.g, p_clear_color.b, p_clear_color.a}}});
    color_attachment_ids.emplace_back(p_color_attachment_id);
    PipelineBatch batch(p_render_data->allocator);
    batch.pipeline_id = p_pipeline_id;
    batch.draw_count = p_draw_count;
    batch.vertex_input = std::move(p_vertex_input);
    batch.vertex_buffer_offset = p_vertex_buffer_offset;
    pipeline_batches.push_back(std::move(batch));
}

RTRunGraphicsPipelineIndirectVK::RTRunGraphicsPipelineIndirectVK(const BaseRenderDataVK* p_render_data, // NOLINT
    std::span<const VkClearValue> p_clear_values,
    VkExtent2D p_render_extent,
    std::span<const std::string_view> p_color_attachment_ids,
    std::optional<std::string_view> p_depth_attachment_id,
    std::string_view p_pipeline_id,
    std::string_view p_vertex_buffer_id,
    std::string_view p_index_buffer_id,
    std::string_view p_indirect_buffer_id,
    uint32_t p_draw_count,
    VkAttachmentLoadOp p_color_attachment_load_op,
    VkAttachmentLoadOp p_depth_attachment_load_op,
    VertexInputDescriptionVK p_vertex_input,
    VkDeviceSize p_vertex_buffer_offset)
    : render_data(p_render_data), clear_values(p_clear_values.begin(), p_clear_values.end(), p_render_data->allocator),
      color_attachment_load_op(p_color_attachment_load_op), depth_attachment_load_op(p_depth_attachment_load_op),
      render_extent(p_render_extent),
      color_attachment_ids(p_color_attachment_ids.begin(), p_color_attachment_ids.end(), p_render_data->allocator),
      depth_attachment_id(p_depth_attachment_id.has_value() ? std::optional<std::string>(*p_depth_attachment_id) : std::nullopt),
      vertex_buffer_id(p_vertex_buffer_id), index_buffer_id(p_index_buffer_id), indirect_buffer_id(p_indirect_buffer_id),
      pipeline_batches(p_render_data->allocator) {
    PipelineBatch batch(p_render_data->allocator);
    batch.pipeline_id = p_pipeline_id;
    batch.draw_count = p_draw_count;
    batch.vertex_input = std::move(p_vertex_input);
    batch.vertex_buffer_offset = p_vertex_buffer_offset;
    pipeline_batches.push_back(std::move(batch));
}

RTRunGraphicsPipelineIndirectVK::RTRunGraphicsPipelineIndirectVK(const BaseRenderDataVK* p_render_data, // NOLINT
    std::span<const VkClearValue> p_clear_values,
    VkExtent2D p_render_extent,
    std::span<const std::string_view> p_color_attachment_ids,
    std::optional<std::string_view> p_depth_attachment_id,
    std::string_view p_vertex_buffer_id,
    std::string_view p_index_buffer_id,
    std::string_view p_indirect_buffer_id,
    std::span<const PipelineBatch> p_pipeline_batches,
    VkAttachmentLoadOp p_color_attachment_load_op,
    VkAttachmentLoadOp p_depth_attachment_load_op)
    : render_data(p_render_data), clear_values(p_clear_values.begin(), p_clear_values.end(), p_render_data->allocator),
      color_attachment_load_op(p_color_attachment_load_op), depth_attachment_load_op(p_depth_attachment_load_op),
      render_extent(p_render_extent),
      color_attachment_ids(p_color_attachment_ids.begin(), p_color_attachment_ids.end(), p_render_data->allocator),
      depth_attachment_id(p_depth_attachment_id.has_value() ? std::optional<std::string>(*p_depth_attachment_id) : std::nullopt),
      vertex_buffer_id(p_vertex_buffer_id), index_buffer_id(p_index_buffer_id), indirect_buffer_id(p_indirect_buffer_id),
      pipeline_batches(p_pipeline_batches.begin(), p_pipeline_batches.end(), p_render_data->allocator) {
}

RTRunGraphicsPipelineIndirectVK::RTRunGraphicsPipelineIndirectVK(const BaseRenderDataVK* p_render_data, // NOLINT
    std::span<const VkClearValue> p_clear_values,
    VkExtent2D p_render_extent,
    std::span<const std::string_view> p_color_attachment_ids,
    std::optional<std::string_view> p_depth_attachment_id,
    std::string_view p_vertex_buffer_id,
    std::string_view p_index_buffer_id,
    Ref<ROBufferVK> p_indirect_buffer,
    std::span<const PipelineBatch> p_pipeline_batches,
    VkAttachmentLoadOp p_color_attachment_load_op,
    VkAttachmentLoadOp p_depth_attachment_load_op)
    : render_data(p_render_data), clear_values(p_clear_values.begin(), p_clear_values.end(), p_render_data->allocator),
      color_attachment_load_op(p_color_attachment_load_op), depth_attachment_load_op(p_depth_attachment_load_op),
      render_extent(p_render_extent),
      color_attachment_ids(p_color_attachment_ids.begin(), p_color_attachment_ids.end(), p_render_data->allocator),
      depth_attachment_id(p_depth_attachment_id.has_value() ? std::optional<std::string>(*p_depth_attachment_id) : std::nullopt),
      vertex_buffer_id(p_vertex_buffer_id), index_buffer_id(p_index_buffer_id), indirect_buffer(std::move(p_indirect_buffer)),
      pipeline_batches(p_pipeline_batches.begin(), p_pipeline_batches.end(), p_render_data->allocator) {
    if (indirect_buffer == nullptr) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: task-owned indirect command buffer is null.");
    }
    for (const auto& batch : pipeline_batches) {
        if (batch.graphics_pipeline == nullptr) {
            throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: task-owned graphics pipeline is null.");
        }
    }
}

Vector<ResAccessVK, RenderAllocator> RTRunGraphicsPipelineIndirectVK::read_resources() const {
    Vector<ResAccessVK, RenderAllocator> read_accesses(render_data->allocator);
    for (const std::string& color_attachment_id : color_attachment_ids) {
        if (color_attachment_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
            read_accesses.push_back({.res_id = color_attachment_id, .usage = RendResUsageVK::COLOR_ATTACHMENT});
        }
    }
    if (depth_attachment_id.has_value() && depth_attachment_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
        read_accesses.push_back({.res_id = *depth_attachment_id, .usage = RendResUsageVK::DEPTH_ATTACHMENT});
    }
    read_accesses.push_back({.res_id = vertex_buffer_id, .usage = RendResUsageVK::VERTEX_BUFFER});
    read_accesses.push_back({.res_id = index_buffer_id, .usage = RendResUsageVK::INDEX_BUFFER});
    if (indirect_buffer == nullptr) {
        read_accesses.push_back({.res_id = indirect_buffer_id, .usage = RendResUsageVK::INDIRECT_BUFFER});
    }
    return read_accesses;
}

void RTRunGraphicsPipelineIndirectVK::validate_resources(const RendResViewVK& p_resources) const {
    for (const PipelineBatch& pipeline_batch : pipeline_batches) {
        if (pipeline_batch.graphics_pipeline != MEM_NULL) {
            continue;
        }
        Ref<ROGraphicsPipelineVK> graphics_pipeline =
            p_resources.get(pipeline_batch.pipeline_id).container->get().dynamic_cast_ref<ROGraphicsPipelineVK>();
        if (graphics_pipeline == MEM_NULL) {
            throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: graphics pipeline resource has the wrong type.");
        }
    }
}

Vector<ResAccessVK, RenderAllocator> RTRunGraphicsPipelineIndirectVK::write_resources() const {
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

void RTRunGraphicsPipelineIndirectVK::record( // NOLINT
    VkCommandBuffer p_command_buffer,
    const RendResViewVK& p_resources,
    double /*p_delta*/) {
    auto vertex_buffer = WBE_REQUIRES_VALID(p_resources.get(vertex_buffer_id).container->get().dynamic_cast_ref<ROBufferVK>());
    auto index_buffer = WBE_REQUIRES_VALID(p_resources.get(index_buffer_id).container->get().dynamic_cast_ref<ROBufferVK>());
    Ref<ROBufferVK> resolved_indirect_buffer = indirect_buffer;
    if (resolved_indirect_buffer == nullptr) {
        resolved_indirect_buffer =
            WBE_REQUIRES_VALID(p_resources.get(indirect_buffer_id).container->get().dynamic_cast_ref<ROBufferVK>());
    }
    uint64_t required_draw_count = 0;
    for (const auto& batch : pipeline_batches) {
        required_draw_count = std::max(required_draw_count, static_cast<uint64_t>(batch.first_draw) + batch.draw_count);
    }
    const uint64_t required_indirect_size = required_draw_count * sizeof(VkDrawIndexedIndirectCommand);
    if (pipeline_batches.empty() || required_draw_count == 0 || resolved_indirect_buffer->fields().buffer_size < required_indirect_size) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: indirect command buffer is empty or too small.");
    }

    auto resolve_graphics_pipeline = [&p_resources](const PipelineBatch& p_batch) {
        if (p_batch.graphics_pipeline != nullptr) {
            return p_batch.graphics_pipeline;
        }
        return WBE_REQUIRES_VALID(p_resources.get(p_batch.pipeline_id).container->get().dynamic_cast_ref<ROGraphicsPipelineVK>());
    };
    Ref<ROGraphicsPipelineVK> first_graphics_pipeline = resolve_graphics_pipeline(pipeline_batches.front());

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = {.offset = {.x = 0, .y = 0}, .extent = render_extent};
    rendering_info.layerCount = 1;
    const size_t color_attachment_count = first_graphics_pipeline->get_config().attachment_config.color_formats.size();
    if (color_attachment_ids.size() != color_attachment_count) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: color attachment count does not match graphics pipeline.");
    }
    const size_t expected_clear_value_count = color_attachment_count + (depth_attachment_id.has_value() ? 1 : 0);
    if (clear_values.size() < expected_clear_value_count) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: insufficient clear values for attachments.");
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

    VkViewport viewport = render_data->viewport;
    viewport.width = static_cast<float>(render_extent.width);
    viewport.height = static_cast<float>(render_extent.height);
    vkCmdSetViewport(p_command_buffer, 0, 1, &viewport);
    VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = render_extent};
    vkCmdSetScissor(p_command_buffer, 0, 1, &scissor);
    vkCmdBindIndexBuffer(p_command_buffer, index_buffer->get_buffer(), 0, VK_INDEX_TYPE_UINT32);
    if (render_data->command_set_vertex_input == nullptr) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: dynamic vertex-input command is unavailable.");
    }
    for (const auto& batch : pipeline_batches) {
        Ref<ROGraphicsPipelineVK> graphics_pipeline = resolve_graphics_pipeline(batch);
        if (graphics_pipeline->get_config().attachment_config.color_formats.size() != color_attachment_count) {
            throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: pipeline batch attachment counts do not match.");
        }
        vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline->get_pipeline());
        VkBuffer vertex_buffer_handle = vertex_buffer->get_buffer();
        vkCmdBindVertexBuffers(p_command_buffer, VERTEX_BUFFER_BINDPOINT, 1, &vertex_buffer_handle, &batch.vertex_buffer_offset);
        render_data->command_set_vertex_input(p_command_buffer,
            static_cast<uint32_t>(batch.vertex_input.bindings.size()),
            batch.vertex_input.bindings.data(),
            static_cast<uint32_t>(batch.vertex_input.attributes.size()),
            batch.vertex_input.attributes.data());
        graphics_pipeline->bind_descriptor_sets(p_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
        VkDeviceSize indirect_offset = static_cast<VkDeviceSize>(batch.first_draw) * sizeof(VkDrawIndexedIndirectCommand);
        vkCmdDrawIndexedIndirect(
            p_command_buffer, resolved_indirect_buffer->get_buffer(), indirect_offset, batch.draw_count, sizeof(VkDrawIndexedIndirectCommand));
    }
    vkCmdEndRendering(p_command_buffer);
}

} // namespace WhiteBirdEngine
