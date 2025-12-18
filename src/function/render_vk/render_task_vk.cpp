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
#include "function/render_vk/render_task_vk.hh"
#include "core/allocator/allocator.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "function/render_vk/semaphore_vk.hh"
#include "generated/label_manager.gen.hh"
#include "global/stl_allocator.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <format>
#include <initializer_list>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RenderTaskVK::RecordCommandBufferGuard::RecordCommandBufferGuard(VkCommandBuffer p_command_buffer,
                                                                 VkCommandBufferInheritanceInfo* p_inheritance_info)
: command_buffer(p_command_buffer) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = p_inheritance_info;
    if (vkBeginCommandBuffer(p_command_buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer.");
    }
}

RenderTaskVK::RecordCommandBufferGuard::~RecordCommandBufferGuard() {
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        // Destructor cannot throw an exception. 
        stdout_log()->error("Failed to record command buffer.");
    }
}

VkMemoryBarrier2 RenderTaskVK::MemoryBarrierBuilder::build() const {
    VkMemoryBarrier2 result{};
    result.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    result.srcAccessMask = src_access_mask;
    result.srcStageMask = src_stage_mask;
    result.dstAccessMask = dst_access_mask;
    result.dstStageMask = dst_stage_mask;
    return result;
}

void RenderTaskVK::check_req_res(std::initializer_list<RendResTID> p_expected_req_res) {
    if (req_rend_res.size() != p_expected_req_res.size()) {
        throw std::runtime_error(std::format(
            "Number of required resources doesn't match: expected: {}, actual: {}.", p_expected_req_res.size(), req_rend_res.size()));
    }
    for (size_t i = 0; i < p_expected_req_res.size(); ++i) {
        if (req_rend_res[i] == MEM_NULL) {
            throw std::runtime_error(std::format(
                "Render task's required resource type mismatch: expected: {}, actual: NULL.",
                LabelManager::get_singleton()->get_label_name(p_expected_req_res.begin()[i])));
        }
    }
}

const Vector<VkSemaphoreSubmitInfo>& RenderTaskVK::get_dependent_semaphores_submit_infos(const Vector<VkPipelineStageFlags2>& p_wait_stages) {
    WBE_DEBUG_ASSERT(depend_semaphores.size() == depend_semaphores_submit_infos.size());
    WBE_DEBUG_ASSERT(p_wait_stages.size() == depend_semaphores_submit_infos.size());
    size_t n = depend_semaphores_submit_infos.size();
    for (size_t i = 0; i < n; ++i) {
        depend_semaphores_submit_infos[i] = depend_semaphores[i]->get_submit_info(p_wait_stages[i], render_data->current_frame);
    }
    return depend_semaphores_submit_infos;
}

void RenderTaskVK::submit_command_buffer(VkCommandBuffer p_command_buffer, VkQueue p_queue,
                                         const Vector<VkPipelineStageFlags2>& p_wait_stages,
                                         VkPipelineStageFlags2 p_signal_stage,
                                         bool p_signal_in_flight_fence) {
    VkSemaphoreSubmitInfo signal_semaphore_submit_info = task_semaphore->get_submit_info(p_signal_stage, render_data->current_frame);
    const auto& wait_semaphores = get_dependent_semaphores_submit_infos(p_wait_stages);
    VkCommandBufferSubmitInfo command_buffer_submit_info{};
    command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_submit_info.commandBuffer = p_command_buffer;

    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_buffer_submit_info;
    submit_info.waitSemaphoreInfoCount = wait_semaphores.size();
    submit_info.pWaitSemaphoreInfos = wait_semaphores.data();
    submit_info.signalSemaphoreInfoCount = 1;
    submit_info.pSignalSemaphoreInfos = &signal_semaphore_submit_info;
    if (vkQueueSubmit2(p_queue, 1, &submit_info, p_signal_in_flight_fence ?
                       render_data->in_flight_fence : VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer to queue.");
    }
}

} // namespace WhiteBirdEngine
