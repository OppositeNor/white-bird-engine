/* Copyright 2026 OppositeNor

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
#ifndef WBE_FILE_RT_CPU_IN_FLIGHT_BUFFER_ACQUIRE_VK_HH
#define WBE_FILE_RT_CPU_IN_FLIGHT_BUFFER_ACQUIRE_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <atomic>
#include <cstdint>
#include <semaphore>
#include <span>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTCPUInFlightBufferAcquireVK
 * @brief Starts an asynchronous CPU acquisition of a graph buffer when requested.
 *
 * An unrequested task records no commands; its empty graph submission still signals the task's outgoing Vulkan semaphores.
 * When requested, the task acquires the complete buffer into caller-owned storage, clears the request flag, and releases the CPU
 * semaphore after the storage has been populated. Instances are neither copyable nor movable.
 */
class RTCPUInFlightBufferAcquireVK final : public IRenderTaskVK {
public:
    RTCPUInFlightBufferAcquireVK() = delete;
    ~RTCPUInFlightBufferAcquireVK() override = default;
    RTCPUInFlightBufferAcquireVK(const RTCPUInFlightBufferAcquireVK&) = delete;
    RTCPUInFlightBufferAcquireVK(RTCPUInFlightBufferAcquireVK&&) noexcept = delete;
    RTCPUInFlightBufferAcquireVK& operator=(const RTCPUInFlightBufferAcquireVK&) = delete;
    RTCPUInFlightBufferAcquireVK& operator=(RTCPUInFlightBufferAcquireVK&&) noexcept = delete;

    /**
     * @brief Construct an in-flight CPU buffer-acquisition task.
     *
     * The source buffer must have transfer-source usage. The request flag, completion semaphore, and destination storage are
     * non-owning and must outlive the task and every acquisition it starts. The semaphore must be acquired before another request
     * is issued.
     *
     * @param p_render_data Base Vulkan render data.
     * @param p_buffer_id Render-graph resource id of the source buffer.
     * @param p_destination CPU storage that receives the complete buffer.
     * @param p_acquire_requested Atomic flag consumed and reset when an acquisition starts.
     * @param p_completion_semaphore CPU semaphore released after the destination has been populated.
     */
    RTCPUInFlightBufferAcquireVK(const BaseRenderDataVK* p_render_data,
        std::string_view p_buffer_id,
        std::span<uint8_t> p_destination,
        std::atomic_bool* p_acquire_requested,
        std::binary_semaphore* p_completion_semaphore);

    QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    Vector<ResAccessVK, RenderAllocator> read_resources() const override;

    Vector<ResAccessVK, RenderAllocator> write_resources() const override;

    void bind_resources(const RendResViewVK& p_resources) override;

    void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    std::string buffer_id;
    std::span<uint8_t> destination;
    std::atomic_bool* acquire_requested = nullptr;
    std::binary_semaphore* completion_semaphore = nullptr;
    Ref<RenderObjectContainerVK> buffer_container;
    Ref<std::atomic_bool> pending_completion;
};

} // namespace WhiteBirdEngine

#endif
