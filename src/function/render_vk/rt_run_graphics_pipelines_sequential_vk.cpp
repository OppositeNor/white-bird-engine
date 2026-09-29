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
#include "function/render_vk/render_targets_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunGraphicsPipelinesSequentialVK::DispatchBase::DispatchBase(RenderAllocator* p_allocator) : clear_values(p_allocator) {
}

RTRunGraphicsPipelinesSequentialVK::Dispatch::Dispatch(RenderAllocator* p_allocator, RegularDispatch p_dispatch)
    : base(p_allocator), vertex_input(std::move(p_dispatch.vertex_input)) {
    base = std::move(p_dispatch);
    vertex_buffer_id = std::move(p_dispatch.vertex_buffer_id);
    index_buffer_id = std::move(p_dispatch.index_buffer_id);
    indexed_draw_region = p_dispatch.indexed_draw_region;
    vertex_buffer_offset = p_dispatch.vertex_buffer_offset;
    draw_vertex_count = p_dispatch.draw_vertex_count;
}

RTRunGraphicsPipelinesSequentialVK::Dispatch::Dispatch(RenderAllocator* p_allocator, IndirectDispatch p_dispatch)
    : base(p_allocator), type(DispatchType::INDIRECT), vertex_buffer_id(std::move(p_dispatch.vertex_buffer_id)),
      index_buffer_id(std::move(p_dispatch.index_buffer_id)), indirect_buffer_id(std::move(p_dispatch.indirect_buffer_id)),
      first_indirect_draw(p_dispatch.first_indirect_draw), indirect_draw_count(p_dispatch.indirect_draw_count),
      vertex_input(std::move(p_dispatch.vertex_input)), vertex_buffer_offset(p_dispatch.vertex_buffer_offset) {
    base = std::move(p_dispatch);
}

RTRunGraphicsPipelinesSequentialVK::Sequence::Sequence(RenderAllocator* p_allocator)
    : allocator(p_allocator), dispatches(p_allocator) {
}

RTRunGraphicsPipelinesSequentialVK::Sequence& RTRunGraphicsPipelinesSequentialVK::Sequence::add_dispatch(RegularDispatch p_dispatch) {
    dispatches.emplace_back(allocator, std::move(p_dispatch));
    return *this;
}

RTRunGraphicsPipelinesSequentialVK::Sequence& RTRunGraphicsPipelinesSequentialVK::Sequence::add_dispatch(IndirectDispatch p_dispatch) {
    dispatches.emplace_back(allocator, std::move(p_dispatch));
    return *this;
}

bool RTRunGraphicsPipelinesSequentialVK::Sequence::is_indirect(size_t p_index) const {
    if (p_index >= dispatches.size()) {
        throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: dispatch index is out of bounds.");
    }
    return dispatches[p_index].type == DispatchType::INDIRECT;
}

RTRunGraphicsPipelinesSequentialVK::BoundEntry::BoundEntry(const BaseRenderDataVK* p_render_data, const Dispatch& p_dispatch)
    : dispatch(p_dispatch), render_targets(p_render_data->allocator,
                                *p_dispatch.base.pipeline,
                                RenderTargetsVK::Config{.clear_values = p_dispatch.base.clear_values,
                                    .color_load_op = p_dispatch.base.color_load_op,
                                    .depth_load_op = p_dispatch.base.depth_load_op}) {
    descriptor_bindings = make_unique<PipelineDescriptorBindingsVK>(
        p_render_data->allocator, p_render_data, dispatch.base.pipeline->get_descriptor_layout());
}

RTRunGraphicsPipelinesSequentialVK::RTRunGraphicsPipelinesSequentialVK(const BaseRenderDataVK* p_render_data, const Sequence& p_sequence)
    : render_data(p_render_data), entries(p_render_data->allocator) {
    if (p_sequence.dispatches.empty()) {
        throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: at least one graphics pass is required.");
    }
    entries.reserve(p_sequence.dispatches.size());
    for (const Dispatch& dispatch : p_sequence.dispatches) {
        const DispatchBase& dispatch_base = dispatch.base;
        if (dispatch_base.pipeline == MEM_NULL) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: graphics pipeline is null.");
        }
        if (dispatch_base.render_extent.width == 0 || dispatch_base.render_extent.height == 0) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: render extent must be non-zero.");
        }
        if (!entries.empty() && !entries.front().render_targets.matches(*dispatch_base.pipeline)) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: passes do not share one attachment layout.");
        }
        if (dispatch.type == DispatchType::REGULAR) {
            if (dispatch.vertex_buffer_id.has_value() != dispatch.index_buffer_id.has_value()) {
                throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: vertex and index buffers must be configured together.");
            }
            if (dispatch.indexed_draw_region.has_value() && !dispatch.index_buffer_id.has_value()) {
                throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: indexed draw region requires draw buffers.");
            }
            if (!dispatch.vertex_buffer_id.has_value() && dispatch.draw_vertex_count == 0) {
                throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: vertex-less draw count must be non-zero.");
            }
        } else if (!dispatch.vertex_buffer_id.has_value() || !dispatch.index_buffer_id.has_value() ||
                   dispatch.indirect_buffer_id.empty() || dispatch.indirect_draw_count == 0) {
            throw std::runtime_error(
                "RTRunGraphicsPipelinesSequentialVK: indirect draws require vertex, index, and command buffers and a non-zero draw count.");
        }
        entries.emplace_back(p_render_data, dispatch);
    }
}

void RTRunGraphicsPipelinesSequentialVK::append_accesses(
    Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const {
    for (const BoundEntry& bound_entry : entries) {
        const Dispatch& dispatch = bound_entry.dispatch;
        bound_entry.render_targets.append_accesses(p_reads, p_writes);
        if (dispatch.vertex_buffer_id.has_value()) {
            p_reads.push_back({.res_id = *dispatch.vertex_buffer_id, .usage = RendResUsageVK::VERTEX_BUFFER});
            p_reads.push_back({.res_id = *dispatch.index_buffer_id, .usage = RendResUsageVK::INDEX_BUFFER});
        }
        if (dispatch.type == DispatchType::INDIRECT) {
            p_reads.push_back({.res_id = dispatch.indirect_buffer_id, .usage = RendResUsageVK::INDIRECT_BUFFER});
        }
        bound_entry.descriptor_bindings->append_accesses(p_reads, p_writes);
    }
}

void RTRunGraphicsPipelinesSequentialVK::bind_resources(const RendResViewVK& p_resources) {
    for (BoundEntry& bound_entry : entries) {
        const Dispatch& dispatch = bound_entry.dispatch;
        bound_entry.descriptor_bindings->bind_resources(p_resources);
        bound_entry.render_targets.bind_resources(p_resources);
        if (dispatch.vertex_buffer_id.has_value()) {
            bound_entry.vertex_buffer_container = p_resources.get(*dispatch.vertex_buffer_id).container;
            bound_entry.index_buffer_container = p_resources.get(*dispatch.index_buffer_id).container;
        }
        if (dispatch.type == DispatchType::INDIRECT) {
            bound_entry.indirect_buffer_container = p_resources.get(dispatch.indirect_buffer_id).container;
        }
    }
}

void RTRunGraphicsPipelinesSequentialVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) { // NOLINT
    for (size_t entry_index = 0; entry_index < entries.size(); ++entry_index) {
        BoundEntry& bound_entry = entries[entry_index];
        Dispatch& dispatch = bound_entry.dispatch;
        DispatchBase& dispatch_base = dispatch.base;

        bound_entry.render_targets.begin_rendering(render_data, p_command_buffer, dispatch_base.render_extent);
        vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, dispatch_base.pipeline->get_pipeline());
        VkViewport viewport = render_data->viewport;
        viewport.width = static_cast<float>(dispatch_base.render_extent.width);
        viewport.height = static_cast<float>(dispatch_base.render_extent.height);
        vkCmdSetViewport(p_command_buffer, 0, 1, &viewport);
        VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = dispatch_base.render_extent};
        vkCmdSetScissor(p_command_buffer, 0, 1, &scissor);
        bound_entry.descriptor_bindings->push(p_command_buffer);

        if (render_data->command_set_vertex_input == nullptr) {
            throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: dynamic vertex-input command is unavailable.");
        }
        if (dispatch.type == DispatchType::INDIRECT) {
            Ref<ROBufferVK> vertex_buffer = WBE_REQUIRES_VALID(
                bound_entry.vertex_buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>());
            Ref<ROBufferVK> index_buffer = WBE_REQUIRES_VALID(
                bound_entry.index_buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>());
            Ref<ROBufferVK> indirect_buffer = WBE_REQUIRES_VALID(
                bound_entry.indirect_buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>());
            VkBuffer vertex_buffer_handle = vertex_buffer->get_buffer();
            vkCmdBindVertexBuffers(p_command_buffer, VERTEX_BUFFER_BINDPOINT, 1, &vertex_buffer_handle, &dispatch.vertex_buffer_offset);
            render_data->command_set_vertex_input(p_command_buffer,
                static_cast<uint32_t>(dispatch.vertex_input.bindings.size()),
                dispatch.vertex_input.bindings.data(),
                static_cast<uint32_t>(dispatch.vertex_input.attributes.size()),
                dispatch.vertex_input.attributes.data());
            vkCmdBindIndexBuffer(p_command_buffer, index_buffer->get_buffer(), 0, VK_INDEX_TYPE_UINT32);
            const VkDeviceSize indirect_offset =
                static_cast<VkDeviceSize>(dispatch.first_indirect_draw) * sizeof(VkDrawIndexedIndirectCommand);
            const VkDeviceSize required_size =
                indirect_offset + static_cast<VkDeviceSize>(dispatch.indirect_draw_count) * sizeof(VkDrawIndexedIndirectCommand);
            if (required_size > indirect_buffer->fields().buffer_size) {
                throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: indirect draw range exceeds buffer bounds.");
            }
            vkCmdDrawIndexedIndirect(
                p_command_buffer, indirect_buffer->get_buffer(), indirect_offset, dispatch.indirect_draw_count, sizeof(VkDrawIndexedIndirectCommand));
        } else if (dispatch.vertex_buffer_id.has_value()) {
            Ref<ROBufferVK> vertex_buffer = WBE_REQUIRES_VALID(
                bound_entry.vertex_buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>());
            Ref<ROBufferVK> index_buffer = WBE_REQUIRES_VALID(
                bound_entry.index_buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>());
            VkBuffer vertex_buffer_handle = vertex_buffer->get_buffer();
            vkCmdBindVertexBuffers(p_command_buffer, VERTEX_BUFFER_BINDPOINT, 1, &vertex_buffer_handle, &dispatch.vertex_buffer_offset);
            render_data->command_set_vertex_input(p_command_buffer,
                static_cast<uint32_t>(dispatch.vertex_input.bindings.size()),
                dispatch.vertex_input.bindings.data(),
                static_cast<uint32_t>(dispatch.vertex_input.attributes.size()),
                dispatch.vertex_input.attributes.data());
            vkCmdBindIndexBuffer(p_command_buffer, index_buffer->get_buffer(), 0, VK_INDEX_TYPE_UINT32);
            const uint32_t full_index_count = static_cast<uint32_t>(index_buffer->fields().buffer_size / sizeof(uint32_t));
            uint32_t first_index = 0;
            uint32_t index_count = full_index_count;
            int32_t vertex_offset = 0;
            if (dispatch.indexed_draw_region.has_value()) {
                first_index = dispatch.indexed_draw_region->first_index;
                index_count = dispatch.indexed_draw_region->index_count;
                vertex_offset = dispatch.indexed_draw_region->vertex_offset;
                if (first_index > full_index_count || index_count > full_index_count - first_index) {
                    throw std::runtime_error("RTRunGraphicsPipelinesSequentialVK: indexed draw region exceeds index buffer bounds.");
                }
            }
            vkCmdDrawIndexed(p_command_buffer, index_count, 1, first_index, vertex_offset, 0);
        } else {
            render_data->command_set_vertex_input(p_command_buffer, 0, nullptr, 0, nullptr);
            vkCmdDraw(p_command_buffer, dispatch.draw_vertex_count, 1, 0, 0);
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
