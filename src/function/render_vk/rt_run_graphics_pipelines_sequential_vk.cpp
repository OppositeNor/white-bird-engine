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
#include "function/render_vk/rt_run_graphics_pipelines_sequential_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/pipeline_descriptor_bindings_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunGraphicsPipelinesSequentialVK::Entry::Entry(RenderAllocator* p_allocator)
    : clear_values(p_allocator), color_attachment_ids(p_allocator) {
}

RTRunGraphicsPipelinesSequentialVK::Dispatch::Dispatch(RenderAllocator* p_allocator) : entries(p_allocator) {
}

RTRunGraphicsPipelinesSequentialVK::Dispatch& RTRunGraphicsPipelinesSequentialVK::Dispatch::add_entry(Entry p_entry) {
    entries.push_back(std::move(p_entry));
    return *this;
}

RTRunGraphicsPipelinesSequentialVK::BoundEntry::BoundEntry(const BaseRenderDataVK* p_render_data, const Entry& p_entry)
    : entry(p_render_data->allocator), color_attachment_containers(p_render_data->allocator) {
    entry = p_entry;
    descriptor_bindings = make_unique<PipelineDescriptorBindingsVK>(
        p_render_data->allocator, p_render_data, entry.pipeline->get_descriptor_layout());
}

RTRunGraphicsPipelinesSequentialVK::RTRunGraphicsPipelinesSequentialVK(const BaseRenderDataVK* p_render_data, const Dispatch& p_dispatch)
    : render_data(p_render_data), entries(p_render_data->allocator) {
    if (p_dispatch.entries.empty()) {
        throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: at least one graphics pass is required.");
    }
    entries.reserve(p_dispatch.entries.size());
    for (const Entry& entry : p_dispatch.entries) {
        if (entry.pipeline == MEM_NULL) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: graphics pipeline is null.");
        }
        if (entry.render_extent.width == 0 || entry.render_extent.height == 0) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: render extent must be non-zero.");
        }
        if (entry.color_attachment_ids.size() != entry.pipeline->get_config().attachment_config.color_formats.size()) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: color attachment count does not match graphics pipeline.");
        }
        const size_t required_clear_value_count = entry.color_attachment_ids.size() + (entry.depth_attachment_id.has_value() ? 1 : 0);
        if (entry.clear_values.size() < required_clear_value_count) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: insufficient clear values for attachments.");
        }
        if (entry.vertex_buffer_id.has_value() != entry.index_buffer_id.has_value()) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: vertex and index buffers must be configured together.");
        }
        if (entry.indexed_draw_region.has_value() && !entry.index_buffer_id.has_value()) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: indexed draw region requires draw buffers.");
        }
        if (!entry.vertex_buffer_id.has_value() && entry.draw_vertex_count == 0) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: vertex-less draw count must be non-zero.");
        }
        entries.emplace_back(p_render_data, entry);
    }
}

Vector<ResAccessVK, RenderAllocator> RTRunGraphicsPipelinesSequentialVK::read_resources() const {
    Vector<ResAccessVK, RenderAllocator> read_accesses(render_data->allocator);
    for (const BoundEntry& bound_entry : entries) {
        const Entry& entry = bound_entry.entry;
        if (entry.color_attachment_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
            for (const std::string& color_attachment_id : entry.color_attachment_ids) {
                read_accesses.push_back({.res_id = color_attachment_id, .usage = RendResUsageVK::COLOR_ATTACHMENT});
            }
        }
        if (entry.depth_attachment_id.has_value() && entry.depth_attachment_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
            read_accesses.push_back({.res_id = *entry.depth_attachment_id, .usage = RendResUsageVK::DEPTH_ATTACHMENT});
        }
        if (entry.vertex_buffer_id.has_value()) {
            read_accesses.push_back({.res_id = *entry.vertex_buffer_id, .usage = RendResUsageVK::VERTEX_BUFFER});
            read_accesses.push_back({.res_id = *entry.index_buffer_id, .usage = RendResUsageVK::INDEX_BUFFER});
        }
    }
    return read_accesses;
}

Vector<ResAccessVK, RenderAllocator> RTRunGraphicsPipelinesSequentialVK::write_resources() const {
    Vector<ResAccessVK, RenderAllocator> write_accesses(render_data->allocator);
    for (const BoundEntry& bound_entry : entries) {
        const Entry& entry = bound_entry.entry;
        for (const std::string& color_attachment_id : entry.color_attachment_ids) {
            write_accesses.push_back({.res_id = color_attachment_id, .usage = RendResUsageVK::COLOR_ATTACHMENT});
        }
        if (entry.depth_attachment_id.has_value()) {
            write_accesses.push_back({.res_id = *entry.depth_attachment_id, .usage = RendResUsageVK::DEPTH_ATTACHMENT});
        }
    }
    return write_accesses;
}

void RTRunGraphicsPipelinesSequentialVK::bind_resources(const RendResViewVK& p_resources) {
    for (BoundEntry& bound_entry : entries) {
        const Entry& entry = bound_entry.entry;
        bound_entry.descriptor_bindings->bind_resources(p_resources);
        bound_entry.color_attachment_containers.clear();
        bound_entry.color_attachment_containers.reserve(entry.color_attachment_ids.size());
        for (const std::string& color_attachment_id : entry.color_attachment_ids) {
            bound_entry.color_attachment_containers.push_back(p_resources.get(color_attachment_id).container);
        }
        if (entry.depth_attachment_id.has_value()) {
            bound_entry.depth_attachment_container = p_resources.get(*entry.depth_attachment_id).container;
        }
        if (entry.vertex_buffer_id.has_value()) {
            bound_entry.vertex_buffer_container = p_resources.get(*entry.vertex_buffer_id).container;
            bound_entry.index_buffer_container = p_resources.get(*entry.index_buffer_id).container;
        }
    }
}

void RTRunGraphicsPipelinesSequentialVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) { // NOLINT
    for (size_t entry_index = 0; entry_index < entries.size(); ++entry_index) {
        BoundEntry& bound_entry = entries[entry_index];
        Entry& entry = bound_entry.entry;

        Vector<VkRenderingAttachmentInfo, RenderAllocator> color_attachments(render_data->allocator);
        color_attachments.reserve(entry.color_attachment_ids.size());
        for (size_t attachment_index = 0; attachment_index < entry.color_attachment_ids.size(); ++attachment_index) {
            Ref<ROImageViewVK> color_attachment = WBE_REQUIRES_VALID(
                bound_entry.color_attachment_containers[attachment_index]->get(render_data->current_frame_slot).dynamic_cast_ref<ROImageViewVK>());
            color_attachments.push_back(ROImageViewVK::AttachmentBuilder::color(color_attachment->get_image_view())
                    .set_load_op(entry.color_attachment_load_op)
                    .set_clear_value(entry.clear_values[attachment_index])
                    .build());
        }

        VkRenderingAttachmentInfo depth_attachment_info{};
        VkRenderingInfo rendering_info{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.offset = {.x = 0, .y = 0}, .extent = entry.render_extent},
            .layerCount = 1,
            .colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
            .pColorAttachments = color_attachments.data(),
        };
        if (entry.depth_attachment_id.has_value()) {
            Ref<ROImageViewVK> depth_attachment = WBE_REQUIRES_VALID(
                bound_entry.depth_attachment_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROImageViewVK>());
            depth_attachment_info = ROImageViewVK::AttachmentBuilder::depth(depth_attachment->get_image_view())
                                        .set_load_op(entry.depth_attachment_load_op)
                                        .set_store_op(VK_ATTACHMENT_STORE_OP_STORE)
                                        .set_clear_value(entry.clear_values[color_attachments.size()])
                                        .build();
            rendering_info.pDepthAttachment = &depth_attachment_info;
        }

        vkCmdBeginRendering(p_command_buffer, &rendering_info);
        vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, entry.pipeline->get_pipeline());
        VkViewport viewport = render_data->viewport;
        viewport.width = static_cast<float>(entry.render_extent.width);
        viewport.height = static_cast<float>(entry.render_extent.height);
        vkCmdSetViewport(p_command_buffer, 0, 1, &viewport);
        VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = entry.render_extent};
        vkCmdSetScissor(p_command_buffer, 0, 1, &scissor);
        bound_entry.descriptor_bindings->push(p_command_buffer);

        if (render_data->command_set_vertex_input == nullptr) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: dynamic vertex-input command is unavailable.");
        }
        if (entry.vertex_buffer_id.has_value()) {
            Ref<ROBufferVK> vertex_buffer = WBE_REQUIRES_VALID(
                bound_entry.vertex_buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>());
            Ref<ROBufferVK> index_buffer = WBE_REQUIRES_VALID(
                bound_entry.index_buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>());
            VkBuffer vertex_buffer_handle = vertex_buffer->get_buffer();
            vkCmdBindVertexBuffers(p_command_buffer, VERTEX_BUFFER_BINDPOINT, 1, &vertex_buffer_handle, &entry.vertex_buffer_offset);
            render_data->command_set_vertex_input(p_command_buffer,
                static_cast<uint32_t>(entry.vertex_input.bindings.size()),
                entry.vertex_input.bindings.data(),
                static_cast<uint32_t>(entry.vertex_input.attributes.size()),
                entry.vertex_input.attributes.data());
            vkCmdBindIndexBuffer(p_command_buffer, index_buffer->get_buffer(), 0, VK_INDEX_TYPE_UINT32);
            const uint32_t full_index_count = static_cast<uint32_t>(index_buffer->fields().buffer_size / sizeof(uint32_t));
            uint32_t first_index = 0;
            uint32_t index_count = full_index_count;
            int32_t vertex_offset = 0;
            if (entry.indexed_draw_region.has_value()) {
                first_index = entry.indexed_draw_region->first_index;
                index_count = entry.indexed_draw_region->index_count;
                vertex_offset = entry.indexed_draw_region->vertex_offset;
                if (first_index > full_index_count || index_count > full_index_count - first_index) {
                    throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: indexed draw region exceeds index buffer bounds.");
                }
            }
            vkCmdDrawIndexed(p_command_buffer, index_count, 1, first_index, vertex_offset, 0);
        } else {
            render_data->command_set_vertex_input(p_command_buffer, 0, nullptr, 0, nullptr);
            vkCmdDraw(p_command_buffer, entry.draw_vertex_count, 1, 0, 0);
        }
        vkCmdEndRendering(p_command_buffer);

        if (entry_index + 1 < entries.size()) {
            VkMemoryBarrier2 pass_barrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                 VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            };
            VkDependencyInfo dependency_info{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &pass_barrier,
            };
            vkCmdPipelineBarrier2(p_command_buffer, &dependency_info);
        }
    }
}

} // namespace WhiteBirdEngine
