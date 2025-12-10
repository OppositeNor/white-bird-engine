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
#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/semaphore_vk.hh"
#include "global/stl_allocator.hh"
#include <cstdint>
#include <initializer_list>
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
        depend_semaphores_submit_infos(p_render_data->allocator),
        render_data(p_render_data) {}
    virtual ~RenderTaskVK() {}

    /**
     * @brief Setup the render task.
     */
    void setup() {
        WBE_DEBUG_ASSERT(task_semaphore != MEM_NULL);
        depend_semaphores_submit_infos.resize(depend_semaphores.size(), VkSemaphoreSubmitInfo{});
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
    vector<VkSemaphoreSubmitInfo> depend_semaphores_submit_infos;

    /**
     * @brief The depth of node of this task in the render graph.
     */
    uint64_t node_depth = 0;

    Ref<SemaphoreVK> task_semaphore;

protected:
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
        RecordCommandBufferGuard(VkCommandBuffer p_command_buffer, VkCommandBufferInheritanceInfo* p_inheritance_info = nullptr);

        ~RecordCommandBufferGuard();

        VkCommandBuffer command_buffer;
    };

    /**
     * @class MemoryBarrierBuilder
     * @brief Builder for memory barriers.
     *
     */
    struct MemoryBarrierBuilder {
        // The source access.
        VkAccessFlags2 src_access_mask = VK_ACCESS_MEMORY_WRITE_BIT;
        // The destination access.
        VkAccessFlags2 dst_access_mask = VK_ACCESS_MEMORY_READ_BIT;
        // TODO: add barrier stage.

        /**
         * @brief Set the source access.
         *
         * @param p_src_access_mask The access bit mask.
         * @return *this.
         */
        MemoryBarrierBuilder& set_src_access(VkAccessFlags p_src_access_mask) {
            src_access_mask = p_src_access_mask;
            return *this;
        }

        /**
         * @brief Set the destination access.
         *
         * @param p_dst_access_mask The access bit mask.
         * @return *this.
         */
        MemoryBarrierBuilder& set_dst_access(VkAccessFlags p_dst_access_mask) {
            dst_access_mask = p_dst_access_mask;
            return *this;
        }

        /**
         * @brief Set access.
         *
         * @param p_src_access_mask The source access bit mask.
         * @param p_dst_access_mask The destination access bit mask.
         * @return 
         */
        MemoryBarrierBuilder& set_access(VkAccessFlags p_src_access_mask, VkAccessFlags p_dst_access_mask) {
            src_access_mask = p_src_access_mask;
            dst_access_mask = p_dst_access_mask;
            return *this;
        }

        /**
         * @brief Build the barrier.
         *
         * @return The barrier instance.
         */
        VkMemoryBarrier2 build() const;
    };

    /**
     * @class BufferMemoryBarrierBuilder
     * @brief Builder for buffer memory barriers.
     *
     */
    struct BufferMemoryBarrierBuilder {
        // The source access.
        VkAccessFlags src_access_mask = VK_ACCESS_MEMORY_WRITE_BIT;
        // The destination access.
        VkAccessFlags dst_access_mask = VK_ACCESS_MEMORY_READ_BIT;
        // The source queue family to tranfer ownership. If all the ownerships in ROBufferVK is set
        // in queue_family_indices, this field could be set to VK_QUEUE_FAMILY_IGNORED.
        uint32_t src_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
        // The destination queue family to tranfer ownership. If all the ownerships in ROBufferVK is
        // set in queue_family_indices, this field could be set to VK_QUEUE_FAMILY_IGNORED.
        uint32_t dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
        // The buffer to set barrier on.
        Ref<ROBufferVK> buffer;
        // The offset of the data to apply barrier on in the buffer.
        VkDeviceSize offset = 0;
        // The size of the data to apply barrier on in the buffer. If set to 0, the barrier is 
        // going to be applied from offset to the end of barrier.
        VkDeviceSize size = 0;

        /**
         * @brief Set the source access.
         *
         * @param p_src_access_mask The access bit mask.
         * @return *this.
         */
        BufferMemoryBarrierBuilder& set_src_access(VkAccessFlags p_src_access_mask) {
            src_access_mask = p_src_access_mask;
            return *this;
        }

        /**
         * @brief Set the destination access.
         *
         * @param p_dst_access_mask The access bit mask.
         * @return *this.
         */
        BufferMemoryBarrierBuilder& set_dst_access(VkAccessFlags p_dst_access_mask) {
            dst_access_mask = p_dst_access_mask;
            return *this;
        }

        /**
         * @brief Set access.
         *
         * @param p_src_access_mask The source access bit mask.
         * @param p_dst_access_mask The destination access bit mask.
         * @return *this.
         */
        BufferMemoryBarrierBuilder& set_access(VkAccessFlags p_src_access_mask, VkAccessFlags p_dst_access_mask) {
            src_access_mask = p_src_access_mask;
            dst_access_mask = p_dst_access_mask;
            return *this;
        }

        /**
         * @brief Set the buffer to apply the barrier on.
         *
         * @param p_buffer The buffer to apply the barrier on.
         * @return *this.
         */
        BufferMemoryBarrierBuilder& set_buffer(Ref<ROBufferVK> p_buffer) {
            buffer = p_buffer;
            return *this;
        }

        /**
         * @brief Set the offset of the data region in the buffer that this barrier is going to be applied on.
         *
         * @param p_offset The offset.
         * @return *this.
         */
        BufferMemoryBarrierBuilder& set_offset(VkDeviceSize p_offset) {
            offset = p_offset;
            return *this;
        }

        /**
         * @brief Set the size of the data region in the buffer that this barrier is going to be applied on.
         *
         * @param p_size The size. If set to 0, map the region from p_offset to the end of the buffer.
         * @return *this.
         */
        BufferMemoryBarrierBuilder& set_size(VkDeviceSize p_size) {
            size = p_size;
            return *this;
        }

        /**
         * @brief Set the data region in the buffer that this barrier is going to be applied on.
         *
         * @param p_offset The offset.
         * @param p_size The size. If set to 0, map the region from p_offset to the end of the buffer.
         * @return *this.
         */
        BufferMemoryBarrierBuilder& set_data_region(VkDeviceSize p_offset, VkDeviceSize p_size) {
            offset = p_offset;
            size = p_size;
            return *this;
        }

        /**
         * @brief Build the barrier.
         *
         * @return The barrier instance.
         */
        VkBufferMemoryBarrier build() const;
    };

    // TODO: Image barrier

    const BaseRenderDataVK* render_data;
    /**
     * @brief Check if the types of required resources matches the resources in req_rend_res.
     *
     * @param p_expected_req_res The expected resources types.
     */
    void check_req_res(std::initializer_list<RendResTID> p_expected_req_res);

    /**
     * @brief Get the dependent semaphores vulkan semaphore instances.
     *
     * @return The dependent semaphores vulkan semaphore instances.
     */
    const vector<VkSemaphoreSubmitInfo>& get_dependent_semaphores_submit_infos(const vector<VkPipelineStageFlags2>& p_wait_stages);

    /**
     * @brief Submit the command buffer to the queue.
     *
     * @param p_command_buffer The command buffer to submit.
     * @param p_queue The queue that the command buffer is submitting to.
     * @param p_wait_stages The stages to wait on.
     * @param p_signal_in_flight_fence Should this command buffer signal the in-flight fence.
     */
    void submit_command_buffer(VkCommandBuffer p_command_buffer, VkQueue p_queue,
                               const vector<VkPipelineStageFlags2>& p_wait_stages,
                               VkPipelineStageFlags2 p_signal_stage,
                               bool p_signal_in_flight_fence);

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
