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
#include "core/allocator/allocator.hh"
#include "core/memory/reference_strong.hh"
#include "generated/type_uuid.gen.hh"
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunGraphicsPipelineVK::RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data, bool p_signal_in_flight_fence)
    : RTRunGraphicsPipelineVK(p_render_data, glm::vec4(0.0F, 0.0F, 0.0F, 1.0F), p_signal_in_flight_fence) {}

RTRunGraphicsPipelineVK::RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data, const glm::vec4& p_clear_color,
                                                 bool p_signal_in_flight_fence)
    : RenderTaskVK(p_render_data), clear_color(p_clear_color), signal_in_flight_fence(p_signal_in_flight_fence),
    wait_stages(p_render_data->allocator) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = render_data->graphics_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(render_data->logic_device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer.");
    }
}

RTRunGraphicsPipelineVK::~RTRunGraphicsPipelineVK() {
    vkFreeCommandBuffers(render_data->logic_device, render_data->graphics_command_pool, 1, &command_buffer);
}

void RTRunGraphicsPipelineVK::init() {
    wait_stages.reserve(depend_semaphores.size());
    for (size_t i = 0; i < depend_semaphores.size(); ++i) {
        wait_stages.push_back(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
}

void RTRunGraphicsPipelineVK::perform(double p_delta) {
    (void)p_delta;
    vkResetCommandBuffer(command_buffer, 0);
    // The resources may be loaded in-flight, so we should check if they are valid every single frame.
    if (check_req_res({
        TypeUUID<ROFramebufferVK>::UUID,
        TypeUUID<ROGraphicsPipelineVK>::UUID})) {
        // Vertex/index buffer not provided, draw without vertex binding.
        record_command_buffer_without_vertex_binding();
    }
    else if (check_req_res({
        TypeUUID<ROFramebufferVK>::UUID,
        TypeUUID<ROGraphicsPipelineVK>::UUID,
        TypeUUID<ROBufferVK>::UUID,
        TypeUUID<ROBufferVK>::UUID})) {
        // Vertex and index buffer provided, draw with index
        record_command_buffer_with_vertex_binding();
    }
    submit_command_buffer(command_buffer, render_data->graphics_queue, wait_stages, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, signal_in_flight_fence);
}

void RTRunGraphicsPipelineVK::record_command_buffer_without_vertex_binding() {
    if (req_rend_res[0]->res == MEM_NULL) {
        throw std::runtime_error("Failed to perform render task: framebuffer must be specified.");
    }
    if (req_rend_res[1]->res == MEM_NULL) {
        throw std::runtime_error("Failed to perform render task: graphics pipeline must be specified.");
    }
    RecordCommandBufferGuard record_command_buffer_guard(command_buffer);

    Ref<ROFramebufferVK> framebuffer = WBE_REQUIRES_VALID(req_rend_res[0]->res.dynamic_cast_ref<ROFramebufferVK>());
    Ref<ROGraphicsPipelineVK> graphics_pipeline = WBE_REQUIRES_VALID(req_rend_res[1]->res.dynamic_cast_ref<ROGraphicsPipelineVK>());

    // Setup render pass.
    VkRenderPassBeginInfo render_pass_info = get_render_pass_begin_info(graphics_pipeline->get_config().render_pass, framebuffer, clear_color);
    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline->get_graphics_pipeline());
    vkCmdSetViewport(command_buffer, 0, 1, &(render_data->viewport));
    VkRect2D scissor{};
    scissor.offset = {.x = 0, .y = 0};
    auto extent = framebuffer->fields().extent;
    scissor.extent = {.width = extent.x, .height = extent.y};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // Usually when vertex buffer is not provided, it is used to draw screen triangle for some GPU local rendering.
    // So we draw 3 vertices for the screen triangle.
    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
}

void RTRunGraphicsPipelineVK::record_command_buffer_with_vertex_binding() {
    if (req_rend_res[0]->res == MEM_NULL) {
        throw std::runtime_error("Failed to perform render task: framebuffer must be specified.");
    }
    if (req_rend_res[1]->res == MEM_NULL) {
        throw std::runtime_error("Failed to perform render task: graphics pipeline must be specified.");
    }
    if (req_rend_res[2]->res == MEM_NULL) {
        throw std::runtime_error("Failed to perform render task: vertex buffer must be specified.");
    }
    if (req_rend_res[3]->res == MEM_NULL) {
        throw std::runtime_error("Failed to perform render task: index buffer must be specified.");
    }
    RecordCommandBufferGuard record_command_buffer_guard(command_buffer);

    Ref<ROFramebufferVK> framebuffer = WBE_REQUIRES_VALID(req_rend_res[0]->res.dynamic_cast_ref<ROFramebufferVK>());
    Ref<ROGraphicsPipelineVK> graphics_pipeline = WBE_REQUIRES_VALID(req_rend_res[1]->res.dynamic_cast_ref<ROGraphicsPipelineVK>());
    Ref<ROBufferVK> vertex_buffer = WBE_REQUIRES_VALID(req_rend_res[2]->res.dynamic_cast_ref<ROBufferVK>());
    Ref<ROBufferVK> index_buffer = WBE_REQUIRES_VALID(req_rend_res[3]->res.dynamic_cast_ref<ROBufferVK>());

    // Setup render pass.
    VkRenderPassBeginInfo render_pass_info = get_render_pass_begin_info(graphics_pipeline->get_config().render_pass, framebuffer, clear_color);
    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline->get_graphics_pipeline());
    VkRect2D scissor{};
    scissor.offset = {.x = 0, .y = 0};
    auto extent = framebuffer->fields().extent;
    scissor.extent = {.width = extent.x, .height = extent.y};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // Setup vertex and index buffer.
    VkBuffer vertex_buffer_inst = vertex_buffer->get_buffer();
    VkBuffer index_buffer_inst = vertex_buffer->get_buffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(command_buffer, VERTEX_BUFFER_BINDPOINT, 1, &vertex_buffer_inst, &offset);
    vkCmdBindIndexBuffer(command_buffer, index_buffer_inst, 0, VK_INDEX_TYPE_UINT32);
    vkCmdSetViewport(command_buffer, 0, 1, &(render_data->viewport));


    // Draw the entire index buffer.
    vkCmdDrawIndexed(command_buffer, index_buffer->fields().buffer_size / sizeof(uint32_t), 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);
}

} // namespace WhiteBirdEngine
