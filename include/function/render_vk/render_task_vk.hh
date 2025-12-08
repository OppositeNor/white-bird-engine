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
#ifndef __WBE_RENDER_TASK_VK_HH__
#define __WBE_RENDER_TASK_VK_HH__

#include "core/allocator/allocator.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "function/render_vk/semaphore_vk.hh"
#include "generated/label_manager.gen.hh"
#include "global/stl_allocator.hh"
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RenderTaskVK
 * @brief Vulkan render task.
 *
 */
struct RenderTaskVK : public RenderTask {
    RenderTaskVK(const BaseRenderDataVK* p_render_data, bool p_use_binary_semaphore = false)
        : req_rend_res(p_render_data->allocator), depend_semaphores(p_render_data->allocator),
        depend_semaphores_inst(p_render_data->allocator), depend_semaphore_timeline_wait_values(p_render_data->allocator),
        render_data(p_render_data) {}
    virtual ~RenderTaskVK() {}

    /**
     * @brief Setup the render task.
     */
    void setup() {
        WBE_DEBUG_ASSERT(task_semaphore != MEM_NULL);
        // update semaphore_timeline_wait_values size
        depend_semaphore_timeline_wait_values.resize(depend_semaphores.size());
        depend_semaphores_inst.resize(depend_semaphores.size());
        init();
    }

    /**
     * @brief The required resources. This will be filled in the order of the req_res field in RendTaskInfo.
     */
    vector<Ref<RendResVK>> req_rend_res;
    /**
     * @brief The semaphores this task depends on. This will be filled with unique semaphores of the dependent tasks.
     */
    vector<Ref<SemaphoreVK>> depend_semaphores;
    vector<VkSemaphore> depend_semaphores_inst;
    vector<uint64_t> depend_semaphore_timeline_wait_values;

    /**
     * @brief The depth of node of this task in the render graph.
     */
    uint64_t node_depth = 0;

    Ref<SemaphoreVK> task_semaphore;

protected:
    const BaseRenderDataVK* render_data;
    /**
     * @brief Check if the types of required resources matches the resources in req_rend_res.
     *
     * @param p_expected_req_res The expected resources types.
     */
    void check_req_res(std::initializer_list<RendResTID> p_expected_req_res) {
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

    /**
     * @brief Get the dependent semaphore timeline semaphores' wait values.
     *
     * @return The dependent semaphores wait values.
     */
    const uint64_t* get_depend_semaphore_timeline_wait_values() {
        size_t n = depend_semaphore_timeline_wait_values.size();
        for (size_t i = 0; i < n; ++i) {
            depend_semaphore_timeline_wait_values[i] = render_data->current_frame;
        }
        return depend_semaphore_timeline_wait_values.data();
    }

    /**
     * @brief Get the dependent semaphores vulkan semaphore instances.
     *
     * @return The dependent semaphores vulkan semaphore instances.
     */
    const VkSemaphore* get_dependent_semaphores_instance() {
        size_t n = depend_semaphores_inst.size();
        for (size_t i = 0; i < n; ++i) {
            depend_semaphores_inst[i] = depend_semaphores[i]->get_semaphore();
        }
        return depend_semaphores_inst.data();
    }

    /**
     * @class RecordCommandBufferGuard
     * @brief RAII guard for recording command buffer.
     *
     */
    class RecordCommandBufferGuard {
    public:
        /**
         * @brief Constructor.
         *
         * @param p_command_buffer The command buffer to record.
         */
        RecordCommandBufferGuard(VkCommandBuffer p_command_buffer, VkCommandBufferInheritanceInfo* p_inheritance_info = nullptr)
            : command_buffer(p_command_buffer) {
            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = 0;
            begin_info.pInheritanceInfo = p_inheritance_info;
            if (vkBeginCommandBuffer(p_command_buffer, &begin_info) != VK_SUCCESS) {
                throw std::runtime_error("Failed to begin recording command buffer.");
            }
        }

        ~RecordCommandBufferGuard() {
            if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
                // Destructor cannot throw an exception. 
                wbe_console_log()->error("Failed to record command buffer.");
                exit(EXIT_FAILURE);
            }
        }

        VkCommandBuffer command_buffer;
    };

    /**
     * @brief Get the timeline semaphore submit info.
     *
     * @return The timeline semaphore submit info.
     */
    VkTimelineSemaphoreSubmitInfo get_timeline_semaphore_si() {
        VkTimelineSemaphoreSubmitInfo timeline_semaphore_submit_info{};
        timeline_semaphore_submit_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timeline_semaphore_submit_info.signalSemaphoreValueCount = 1;
        timeline_semaphore_submit_info.pSignalSemaphoreValues = &render_data->current_frame;
        timeline_semaphore_submit_info.waitSemaphoreValueCount = depend_semaphore_timeline_wait_values.size();
        timeline_semaphore_submit_info.pWaitSemaphoreValues = get_depend_semaphore_timeline_wait_values();
        return timeline_semaphore_submit_info;
    }

    /**
     * @brief Submit the command buffer to the queue.
     *
     * @param p_command_buffer The command buffer to submit.
     * @param p_queue The queue that the command buffer is submitting to.
     * @param p_wait_stages The stages to wait on.
     * @param p_signal_in_flight_fence Should this command buffer signal the in-flight fence.
     */
    void submit_command_buffer(VkCommandBuffer p_command_buffer, VkQueue p_queue,
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
};

/**
 * @class RenderTaskFactoryVK
 * @brief Factory for RenderTaskVK.
 *
 */
struct RenderTaskFactoryVK : public RenderTaskFactory {
public:
    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     */
    RenderTaskFactoryVK(const BaseRenderDataVK* p_render_data)
        : render_data(p_render_data) {}
    virtual ~RenderTaskFactoryVK() {}

protected:
    const BaseRenderDataVK* render_data;
};

}

#endif
