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
#include <cstdint>
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
        wbe_console_log()->error("Failed to record command buffer.");
        exit(EXIT_FAILURE);
    }
}

VkMemoryBarrier RenderTaskVK::MemoryBarrierBuilder::build() const {
    VkMemoryBarrier result{};
    result.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    result.srcAccessMask = src_access_mask;
    result.dstAccessMask = dst_access_mask;
    return result;
}

VkBufferMemoryBarrier RenderTaskVK::BufferMemoryBarrierBuilder::build() const {
    VkBufferMemoryBarrier result{};
    result.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    result.srcAccessMask = src_access_mask;
    result.dstAccessMask = dst_access_mask;
    result.srcQueueFamilyIndex = src_queue_family_index;
    result.dstQueueFamilyIndex = dst_queue_family_index;
    result.buffer = WBE_REQUIRED(buffer).get_buffer();
    result.offset = offset;
    result.size = size == 0 ? buffer->fields().buffer_size : size;
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
                LabelManager::get_singleton()->get_label_name(*(p_expected_req_res.begin() + i))));
        }
    }
}

const uint64_t* RenderTaskVK::get_depend_semaphore_timeline_wait_values() {
    size_t n = depend_semaphore_timeline_wait_values.size();
    for (size_t i = 0; i < n; ++i) {
        depend_semaphore_timeline_wait_values[i] = render_data->current_frame;
    }
    return depend_semaphore_timeline_wait_values.data();
}

const VkSemaphore* RenderTaskVK::get_dependent_semaphores_instance() {
    size_t n = depend_semaphores_inst.size();
    for (size_t i = 0; i < n; ++i) {
        depend_semaphores_inst[i] = depend_semaphores[i]->get_semaphore();
    }
    return depend_semaphores_inst.data();
}

VkTimelineSemaphoreSubmitInfo RenderTaskVK::get_timeline_semaphore_si() {
    VkTimelineSemaphoreSubmitInfo timeline_semaphore_submit_info{};
    timeline_semaphore_submit_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline_semaphore_submit_info.signalSemaphoreValueCount = 1;
    timeline_semaphore_submit_info.pSignalSemaphoreValues = &render_data->current_frame;
    timeline_semaphore_submit_info.waitSemaphoreValueCount = depend_semaphore_timeline_wait_values.size();
    timeline_semaphore_submit_info.pWaitSemaphoreValues = get_depend_semaphore_timeline_wait_values();
    return timeline_semaphore_submit_info;
}

void RenderTaskVK::submit_command_buffer(VkCommandBuffer p_command_buffer, VkQueue p_queue,
                                         const vector<VkPipelineStageFlags>& p_wait_stages,
                                         bool p_signal_in_flight_fence) {
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore signal_semaphores[] = {task_semaphore->get_semaphore()};
    submit_info.waitSemaphoreCount = depend_semaphores.size();
    submit_info.pWaitSemaphores = get_dependent_semaphores_instance();
    submit_info.pWaitDstStageMask = p_wait_stages.data();

    // Setup timeline semaphore submit info.
    VkTimelineSemaphoreSubmitInfo timeline_semaphore_submit_info = get_timeline_semaphore_si();
    submit_info.pNext = &timeline_semaphore_submit_info;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &p_command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    if (vkQueueSubmit(p_queue, 1, &submit_info, p_signal_in_flight_fence ?
                      render_data->in_flight_fence : VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer to queue.");
    }
}

}
