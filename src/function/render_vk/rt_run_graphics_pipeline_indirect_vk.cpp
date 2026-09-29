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
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
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

RTRunGraphicsPipelineIndirectVK::Builder::Builder(RenderAllocator* p_allocator)
    : allocator(p_allocator), clear_values(p_allocator), pipeline_batches(p_allocator) {
    if (allocator == nullptr) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK::Builder: allocator is null.");
    }
}

RTRunGraphicsPipelineIndirectVK::Builder& RTRunGraphicsPipelineIndirectVK::Builder::set_render_extent(VkExtent2D p_render_extent) {
    render_extent = p_render_extent;
    return *this;
}

RTRunGraphicsPipelineIndirectVK::Builder& RTRunGraphicsPipelineIndirectVK::Builder::set_clear_values(
    std::span<const VkClearValue> p_clear_values) {
    clear_values.assign(p_clear_values.begin(), p_clear_values.end());
    return *this;
}

RTRunGraphicsPipelineIndirectVK::Builder& RTRunGraphicsPipelineIndirectVK::Builder::set_color_load_op(VkAttachmentLoadOp p_load_op) {
    color_load_op = p_load_op;
    return *this;
}

RTRunGraphicsPipelineIndirectVK::Builder& RTRunGraphicsPipelineIndirectVK::Builder::set_depth_load_op(VkAttachmentLoadOp p_load_op) {
    depth_load_op = p_load_op;
    return *this;
}

RTRunGraphicsPipelineIndirectVK::Builder& RTRunGraphicsPipelineIndirectVK::Builder::set_vertex_buffer(std::string_view p_resource_id) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK::Builder: vertex buffer resource id is empty.");
    }
    vertex_buffer_id = p_resource_id;
    return *this;
}

RTRunGraphicsPipelineIndirectVK::Builder& RTRunGraphicsPipelineIndirectVK::Builder::set_index_buffer(std::string_view p_resource_id) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK::Builder: index buffer resource id is empty.");
    }
    index_buffer_id = p_resource_id;
    return *this;
}

RTRunGraphicsPipelineIndirectVK::Builder& RTRunGraphicsPipelineIndirectVK::Builder::set_indirect_buffer(std::string_view p_resource_id) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK::Builder: indirect buffer resource id is empty.");
    }
    indirect_buffer_id = p_resource_id;
    return *this;
}

RTRunGraphicsPipelineIndirectVK::Builder& RTRunGraphicsPipelineIndirectVK::Builder::set_pipeline(
    Ref<ROGraphicsPipelineVK> p_pipeline, uint32_t p_draw_count, VertexInputDescriptionVK p_vertex_input, VkDeviceSize p_vertex_buffer_offset) {
    pipeline_batches.clear();
    return add_pipeline_batch(std::move(p_pipeline), 0, p_draw_count, std::move(p_vertex_input), p_vertex_buffer_offset);
}

RTRunGraphicsPipelineIndirectVK::Builder& RTRunGraphicsPipelineIndirectVK::Builder::add_pipeline_batch(
    Ref<ROGraphicsPipelineVK> p_pipeline, uint32_t p_first_draw, uint32_t p_draw_count, VertexInputDescriptionVK p_vertex_input, VkDeviceSize p_vertex_buffer_offset) {
    PipelineBatch batch;
    batch.graphics_pipeline = std::move(p_pipeline);
    batch.first_draw = p_first_draw;
    batch.draw_count = p_draw_count;
    batch.vertex_input = std::move(p_vertex_input);
    batch.vertex_buffer_offset = p_vertex_buffer_offset;
    return add_pipeline_batch(std::move(batch));
}

RTRunGraphicsPipelineIndirectVK::Builder& RTRunGraphicsPipelineIndirectVK::Builder::add_pipeline_batch(PipelineBatch p_batch) {
    if (p_batch.graphics_pipeline == MEM_NULL) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK::Builder: graphics pipeline is null.");
    }
    pipeline_batches.push_back(std::move(p_batch));
    return *this;
}

Unique<RTRunGraphicsPipelineIndirectVK> RTRunGraphicsPipelineIndirectVK::Builder::create_unique(const BaseRenderDataVK* p_render_data) const {
    if (p_render_data == nullptr || p_render_data->allocator == nullptr) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK::Builder: render data is missing.");
    }
    return make_unique<RTRunGraphicsPipelineIndirectVK>(p_render_data->allocator, p_render_data, *this);
}

namespace {

const ROGraphicsPipelineVK& first_pipeline(const Vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch, RenderAllocator>& p_batches) {
    if (p_batches.empty()) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK::Builder: no pipeline batches were configured.");
    }
    return *p_batches.front().graphics_pipeline;
}

} // namespace

RTRunGraphicsPipelineIndirectVK::RTRunGraphicsPipelineIndirectVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : render_data(p_render_data), render_extent(p_builder.render_extent),
      pipeline_batches(p_builder.pipeline_batches.begin(), p_builder.pipeline_batches.end(), p_render_data->allocator),
      render_targets(p_render_data->allocator,
          first_pipeline(pipeline_batches),
          RenderTargetsVK::Config{
              .clear_values = p_builder.clear_values, .color_load_op = p_builder.color_load_op, .depth_load_op = p_builder.depth_load_op}),
      vertex_buffer_id(p_builder.vertex_buffer_id), index_buffer_id(p_builder.index_buffer_id),
      indirect_buffer_id(p_builder.indirect_buffer_id), descriptor_bindings(p_render_data->allocator) {
    if (render_extent.width == 0 || render_extent.height == 0) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK::Builder: render extent is empty.");
    }
    if (index_buffer_id.empty() || indirect_buffer_id.empty()) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK::Builder: index and indirect buffers require resource ids.");
    }
    initialize_descriptor_bindings();
}

void RTRunGraphicsPipelineIndirectVK::initialize_descriptor_bindings() {
    descriptor_bindings.reserve(pipeline_batches.size());
    for (const PipelineBatch& batch : pipeline_batches) {
        if (!render_targets.matches(*batch.graphics_pipeline)) {
            throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: pipeline batches do not share one attachment layout.");
        }
        descriptor_bindings.push_back(make_unique<PipelineDescriptorBindingsVK>(
            render_data->allocator, render_data, batch.graphics_pipeline->get_descriptor_layout()));
    }
}

void RTRunGraphicsPipelineIndirectVK::append_accesses(
    Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const {
    render_targets.append_accesses(p_reads, p_writes);
    if (!vertex_buffer_id.empty()) {
        p_reads.push_back({.res_id = vertex_buffer_id, .usage = RendResUsageVK::VERTEX_BUFFER});
    }
    p_reads.push_back({.res_id = index_buffer_id, .usage = RendResUsageVK::INDEX_BUFFER});
    p_reads.push_back({.res_id = indirect_buffer_id, .usage = RendResUsageVK::INDIRECT_BUFFER});
    for (const Unique<PipelineDescriptorBindingsVK>& bindings : descriptor_bindings) {
        bindings->append_accesses(p_reads, p_writes);
    }
}

void RTRunGraphicsPipelineIndirectVK::bind_resources(const RendResViewVK& p_resources) {
    for (Unique<PipelineDescriptorBindingsVK>& bindings : descriptor_bindings) {
        bindings->bind_resources(p_resources);
    }
    render_targets.bind_resources(p_resources);
    if (!vertex_buffer_id.empty()) {
        vertex_buffer_container = p_resources.get(vertex_buffer_id).container;
    }
    index_buffer_container = p_resources.get(index_buffer_id).container;
    indirect_buffer_container = p_resources.get(indirect_buffer_id).container;
}

void RTRunGraphicsPipelineIndirectVK::record( // NOLINT
    VkCommandBuffer p_command_buffer,
    double /*p_delta*/) {
    Ref<ROBufferVK> resolved_vertex_buffer = MEM_NULL;
    if (vertex_buffer_container != MEM_NULL) {
        resolved_vertex_buffer =
            WBE_REQUIRES_VALID(vertex_buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>());
    }
    Ref<ROBufferVK> resolved_index_buffer =
        WBE_REQUIRES_VALID(index_buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>());
    Ref<ROBufferVK> resolved_indirect_buffer =
        WBE_REQUIRES_VALID(indirect_buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>());
    uint64_t required_draw_count = 0;
    for (const auto& batch : pipeline_batches) {
        required_draw_count = std::max(required_draw_count, static_cast<uint64_t>(batch.first_draw) + batch.draw_count);
    }
    const uint64_t required_indirect_size = required_draw_count * sizeof(VkDrawIndexedIndirectCommand);
    if (pipeline_batches.empty() || required_draw_count == 0 || resolved_indirect_buffer->fields().buffer_size < required_indirect_size) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: indirect command buffer is empty or too small.");
    }

    render_targets.begin_rendering(render_data, p_command_buffer, render_extent);

    VkViewport viewport = render_data->viewport;
    viewport.width = static_cast<float>(render_extent.width);
    viewport.height = static_cast<float>(render_extent.height);
    vkCmdSetViewport(p_command_buffer, 0, 1, &viewport);
    VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = render_extent};
    vkCmdSetScissor(p_command_buffer, 0, 1, &scissor);
    vkCmdBindIndexBuffer(p_command_buffer, resolved_index_buffer->get_buffer(), 0, VK_INDEX_TYPE_UINT32);
    if (render_data->command_set_vertex_input == nullptr) {
        throw std::runtime_error("RTRunGraphicsPipelineIndirectVK: dynamic vertex-input command is unavailable.");
    }
    for (size_t batch_index = 0; batch_index < pipeline_batches.size(); ++batch_index) {
        const PipelineBatch& batch = pipeline_batches[batch_index];
        vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, batch.graphics_pipeline->get_pipeline());
        if (resolved_vertex_buffer != MEM_NULL) {
            VkBuffer vertex_buffer_handle = resolved_vertex_buffer->get_buffer();
            vkCmdBindVertexBuffers(p_command_buffer, VERTEX_BUFFER_BINDPOINT, 1, &vertex_buffer_handle, &batch.vertex_buffer_offset);
        }
        render_data->command_set_vertex_input(p_command_buffer,
            static_cast<uint32_t>(batch.vertex_input.bindings.size()),
            batch.vertex_input.bindings.data(),
            static_cast<uint32_t>(batch.vertex_input.attributes.size()),
            batch.vertex_input.attributes.data());
        descriptor_bindings[batch_index]->push(p_command_buffer);
        VkDeviceSize indirect_offset = static_cast<VkDeviceSize>(batch.first_draw) * sizeof(VkDrawIndexedIndirectCommand);
        vkCmdDrawIndexedIndirect(
            p_command_buffer, resolved_indirect_buffer->get_buffer(), indirect_offset, batch.draw_count, sizeof(VkDrawIndexedIndirectCommand));
    }
    vkCmdEndRendering(p_command_buffer);
}

} // namespace WhiteBirdEngine
