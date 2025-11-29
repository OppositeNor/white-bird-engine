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
#include "function/render_vk/rt_prepare_swap_chain_image_vk.hh"
#include "utils/defs.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
void RTPrepareSwapChianImageVK::perform(double p_delta) {
    WBE_DEBUG_ASSERT(depend_semaphores.size() == 1);
    WBE_DEBUG_ASSERT(depend_semaphores[0]->binary());

    // Don't do anything, just signal the task semaphore.
    VkTimelineSemaphoreSubmitInfo semaphore_submit_info{};
    semaphore_submit_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    semaphore_submit_info.waitSemaphoreValueCount = 1;
    semaphore_submit_info.pWaitSemaphoreValues = &(render_data->current_frame);
    semaphore_submit_info.signalSemaphoreValueCount = 1;
    semaphore_submit_info.pSignalSemaphoreValues = &(render_data->current_frame);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    submit_info.pNext = &semaphore_submit_info,
    submit_info.waitSemaphoreCount = 1;
    auto wait_semaphore = depend_semaphores[0]->get_semaphore();
    submit_info.pWaitSemaphores = &wait_semaphore;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.signalSemaphoreCount = 1;
    auto signal_semaphore = task_semaphore->get_semaphore();
    submit_info.pSignalSemaphores = &signal_semaphore;
    vkQueueSubmit(render_data->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
}

}

