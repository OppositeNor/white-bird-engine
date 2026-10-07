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
#ifndef WBE_FILE_RT_CPU_IN_FLIGHT_ACQUIRE_VK_HH
#define WBE_FILE_RT_CPU_IN_FLIGHT_ACQUIRE_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <cstdint>
#include <future>
#include <semaphore>
#include <span>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTCPUInFlightAcquireVK
 * @brief Starts an asynchronous CPU acquisition of a graph image on every recording.
 *
 * Acquires into caller-owned storage and releases the CPU completion semaphore after population.
 * Wrap in RTTriggeredVK for on-demand acquisition. Instances are neither copyable nor movable.
 */
class RTCPUInFlightAcquireVK final : public IRenderTaskVK {
public:
    RTCPUInFlightAcquireVK() = delete;
    virtual ~RTCPUInFlightAcquireVK() override = default;
    RTCPUInFlightAcquireVK(const RTCPUInFlightAcquireVK&) = delete;
    RTCPUInFlightAcquireVK(RTCPUInFlightAcquireVK&&) noexcept = delete;
    RTCPUInFlightAcquireVK& operator=(const RTCPUInFlightAcquireVK&) = delete;
    RTCPUInFlightAcquireVK& operator=(RTCPUInFlightAcquireVK&&) noexcept = delete;

    /**
     * @brief Construct an in-flight CPU image-acquisition task.
     *
     * The completion semaphore and destination storage are non-owning and must outlive the task and every
     * acquisition it starts. The semaphore must be acquired before recording another acquisition.
     *
     * @param p_render_data Base Vulkan render data.
     * @param p_image_view_id Render-graph resource id of the source image view.
     * @param p_destination CPU storage that receives the complete image.
     * @param p_completion_semaphore CPU semaphore released after the destination has been populated.
     */
    RTCPUInFlightAcquireVK(const BaseRenderDataVK* p_render_data,
        std::string_view p_image_view_id,
        std::span<uint8_t> p_destination,
        std::binary_semaphore* p_completion_semaphore);

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override;

    virtual void bind_resources(const RendResViewVK& p_resources) override;

    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    std::string image_view_id;
    std::span<uint8_t> destination;
    std::binary_semaphore* completion_semaphore = nullptr;
    Ref<RenderObjectContainerVK> image_view_container;
    std::future<void> pending_completion;
};

} // namespace WhiteBirdEngine

#endif
