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
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunGraphicsPipeline::RTRunGraphicsPipeline(const BaseRenderDataVK* p_render_data, Ref<ROGraphicsPipelineVK> p_graphics_pipeline,
                                             Ref<ROFramebufferVK> p_framebuffer, VkFence p_wait_fence)
    : RenderTaskVK(p_render_data), graphics_pipeline(p_graphics_pipeline), framebuffer(p_framebuffer), wait_fence(p_wait_fence) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = render_data->graphics_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(render_data->logic_device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer.");
    }
}

RTRunGraphicsPipeline::~RTRunGraphicsPipeline() {
    vkFreeCommandBuffers(render_data->logic_device, render_data->graphics_command_pool, 1, &command_buffer);
}

void RTRunGraphicsPipeline::perform(double p_delta) {
    check_req_res({});
    check_prod_res({ROT_IMAGE_VK});
    if (req_rend_res[0]->res == MEM_NULL) {
        ROImageVK::Builder builder;
        builder.image_2d(render_data->swap_chain_extent)
            .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .set_format(render_data->swap_chain_format);
        Ref<ROImageVK> image = make_ref<ROImageVK>(render_data->allocator, render_data, builder);
        req_rend_res[0]->res = image;
    }
    record_command_buffer();

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    vectorp<VkSemaphore> wait_semaphores(render_data->allocator);
    vectorp<VkPipelineStageFlags> wait_stages(render_data->allocator);
    wait_semaphores.reserve(depend_tasks.size());
    wait_stages.reserve(depend_tasks.size());
    for (auto dep_task : depend_tasks) {
        wait_semaphores.push_back(dep_task->task_semaphore.semaphore);
        wait_stages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
    VkSemaphore signal_semaphores[] = {task_semaphore.semaphore};

    submit_info.waitSemaphoreCount = wait_semaphores.size();
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    if (vkQueueSubmit(render_data->graphics_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer to queue.");
    }

}

void RTRunGraphicsPipeline::record_command_buffer() {
    auto result = prod_rend_res[0];
    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = graphics_pipeline->get_config().render_pass->get_render_pass();
    render_pass_info.framebuffer = framebuffer->get_framebuffer();
    render_pass_info.renderArea.offset.x = 0;
    render_pass_info.renderArea.offset.y = 0;
    auto extent = framebuffer->fields().extent;
    render_pass_info.renderArea.extent = {extent.x, extent.y};
    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline->get_graphics_pipeline());
    vkCmdSetViewport(command_buffer, 0, 1, &(render_data->viewport));

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {extent.x, extent.y};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer.");
    }
}

}
