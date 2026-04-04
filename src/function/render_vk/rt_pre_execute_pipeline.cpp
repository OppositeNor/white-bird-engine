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
#include "core/allocator/i_allocator.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/rt_pre_execute_pipeline_vk.hh"
#include "function/render_vk/semaphore_vk.hh"
#include "utils/defs.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

RTPreExecutePipelineVK::RTPreExecutePipelineVK(const BaseRenderDataVK* p_render_data, Ref<SemaphoreVK> p_prepare_image_semaphore)
: RenderTaskVK(p_render_data), prepare_image_semaphore(p_prepare_image_semaphore) {
    WBE_DEBUG_ASSERT(p_prepare_image_semaphore != MEM_NULL);
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = render_data->graphics_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(render_data->logic_device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer.");
    }
}

RTPreExecutePipelineVK::~RTPreExecutePipelineVK() {
    vkFreeCommandBuffers(render_data->logic_device, render_data->graphics_command_pool, 1, &command_buffer);
}


void RTPreExecutePipelineVK::perform(double p_delta) {
    (void)p_delta;
    WBE_DEBUG_ASSERT(prepare_image_semaphore != VK_NULL_HANDLE);
    vkResetCommandBuffer(command_buffer, 0);
    record_command_buffer();
    submit_command_buffer();
}

void RTPreExecutePipelineVK::record_command_buffer() {
    RecordCommandBufferGuard record_command_buffer_guard(command_buffer);
    // Execute the pre-execution tasks
    for (const auto& task : render_data->pre_execute_tasks_queue) {
        task(command_buffer);
    }
}

void RTPreExecutePipelineVK::submit_command_buffer() {
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

    VkSemaphoreSubmitInfo wait_submit_info = prepare_image_semaphore->get_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    VkSemaphoreSubmitInfo signal_submit_info = task_semaphore->get_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    VkCommandBufferSubmitInfo command_buffer_submit_info{};
    command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_submit_info.commandBuffer = command_buffer;

    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_buffer_submit_info;
    submit_info.waitSemaphoreInfoCount = 1;
    submit_info.pWaitSemaphoreInfos = &wait_submit_info;
    submit_info.signalSemaphoreInfoCount = 1;
    submit_info.pSignalSemaphoreInfos = &signal_submit_info;
    if (vkQueueSubmit2(render_data->graphics_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer to queue.");
    }
}

} // namespace WhiteBirdEngine
