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
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "global/stl_allocator.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <glm/ext/vector_float4.hpp>
#include <optional>
#include <span>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunGraphicsPipelineVK::RTRunGraphicsPipelineVK(
    const BaseRenderDataVK* p_render_data,
    const glm::vec4& p_clear_color,
    RendResID p_framebuffer_id,
    RendResID p_pipeline_id,
    std::optional<RendResID> p_vertex_buffer_id,
    std::optional<RendResID> p_index_buffer_id,
    std::span<const Ref<RODescriptorSetVK>> p_descriptor_sets)
    : render_data(p_render_data),
      clear_value(VkClearValue{.color = {.float32 = {p_clear_color.r,
                                             p_clear_color.g,
                                             p_clear_color.b,
                                             p_clear_color.a}}}),
      framebuffer_id(p_framebuffer_id), pipeline_id(p_pipeline_id),
      vertex_buffer_id(p_vertex_buffer_id), index_buffer_id(p_index_buffer_id),
      descriptor_sets(p_descriptor_sets.begin(),
          p_descriptor_sets.end(),
          p_render_data->allocator),
      descriptor_sets_instances(p_render_data->allocator) {
    descriptor_sets_instances.reserve(descriptor_sets.size());
    for (const auto& descriptor_set : descriptor_sets) {
        descriptor_sets_instances.push_back(
            WBE_REQUIRES_VALID(descriptor_set->get_descriptor_set()));
    }
}

Vector<ResAccessVK, RenderAllocator>
RTRunGraphicsPipelineVK::read_resources() const {
    Vector<ResAccessVK, RenderAllocator> read_accesses(render_data->allocator);
    read_accesses.push_back({.res_id = pipeline_id,
        .access = VK_ACCESS_2_SHADER_READ_BIT,
        .stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT});
    if (vertex_buffer_id.has_value()) {
        read_accesses.push_back({.res_id = *vertex_buffer_id,
            .access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
            .stage = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT});
    }
    if (index_buffer_id.has_value()) {
        read_accesses.push_back({.res_id = *index_buffer_id,
            .access = VK_ACCESS_2_INDEX_READ_BIT,
            .stage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT});
    }
    return read_accesses;
}

Vector<ResAccessVK, RenderAllocator>
RTRunGraphicsPipelineVK::write_resources() const {
    Vector<ResAccessVK, RenderAllocator> write_accesses(render_data->allocator);
    write_accesses.push_back({.res_id = framebuffer_id,
        .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT});
    return write_accesses;
}

void RTRunGraphicsPipelineVK::record(VkCommandBuffer p_command_buffer,
    const Vector<RendResVK, RenderAllocator>& p_resources,
    double /*p_delta*/) {
    // Layout (matches read_resources() then write_resources()):
    //   [0] graphics_pipeline
    //   [1?] vertex_buffer
    //   [2?] index_buffer
    //   [last] framebuffer
    if (p_resources.size() < 2) {
        throw std::runtime_error(
            "RTRunGraphicsPipelineVK: insufficient bound resources.");
    }
    size_t bound_index = 0;
    auto graphics_pipeline = WBE_REQUIRES_VALID(p_resources[bound_index++]
            .res.dynamic_cast_ref<ROGraphicsPipelineVK>());
    Ref<ROBufferVK> vertex_buffer = MEM_NULL;
    Ref<ROBufferVK> index_buffer = MEM_NULL;
    if (vertex_buffer_id.has_value()) {
        vertex_buffer = WBE_REQUIRES_VALID(
            p_resources[bound_index++].res.dynamic_cast_ref<ROBufferVK>());
    }
    if (index_buffer_id.has_value()) {
        index_buffer = WBE_REQUIRES_VALID(
            p_resources[bound_index++].res.dynamic_cast_ref<ROBufferVK>());
    }
    auto framebuffer = WBE_REQUIRES_VALID(
        p_resources[bound_index++].res.dynamic_cast_ref<ROFramebufferVK>());

    // Begin render pass.
    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass =
        graphics_pipeline->get_config().render_pass->get_render_pass();
    render_pass_begin_info.framebuffer = framebuffer->get_framebuffer();
    render_pass_begin_info.renderArea.offset = {.x = 0, .y = 0};
    auto extent = framebuffer->fields().extent;
    render_pass_begin_info.renderArea.extent = {
        .width = extent.x, .height = extent.y};
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;
    vkCmdBeginRenderPass(
        p_command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(p_command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphics_pipeline->get_graphics_pipeline());
    vkCmdSetViewport(p_command_buffer, 0, 1, &(render_data->viewport));
    VkRect2D scissor{};
    scissor.offset = {.x = 0, .y = 0};
    scissor.extent = {.width = extent.x, .height = extent.y};
    vkCmdSetScissor(p_command_buffer, 0, 1, &scissor);

    if (!descriptor_sets_instances.empty()) {
        vkCmdBindDescriptorSets(p_command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphics_pipeline->fields().pipeline_layout->get_pipeline_layout(),
            0,
            static_cast<uint32_t>(descriptor_sets_instances.size()),
            descriptor_sets_instances.data(),
            0,
            nullptr);
    }

    if (vertex_buffer != MEM_NULL && index_buffer != MEM_NULL) {
        VkBuffer vertex_buffer_handle = vertex_buffer->get_buffer();
        VkBuffer index_buffer_handle = index_buffer->get_buffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(p_command_buffer,
            VERTEX_BUFFER_BINDPOINT,
            1,
            &vertex_buffer_handle,
            &offset);
        vkCmdBindIndexBuffer(
            p_command_buffer, index_buffer_handle, 0, VK_INDEX_TYPE_UINT32);
        size_t index_count =
            index_buffer->fields().buffer_size / sizeof(uint32_t);
        vkCmdDrawIndexed(
            p_command_buffer, static_cast<uint32_t>(index_count), 1, 0, 0, 0);
    } else {
        // Screen-triangle / no vertex binding case.
        vkCmdDraw(p_command_buffer, 3, 1, 0, 0);
    }

    vkCmdEndRenderPass(p_command_buffer);
}

} // namespace WhiteBirdEngine
