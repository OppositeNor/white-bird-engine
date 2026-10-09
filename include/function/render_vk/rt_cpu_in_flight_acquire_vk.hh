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
#include "core/engine_core.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <cstdint>
#include <functional>
#include <future>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTCPUInFlightAcquireVK
 * @brief Starts an asynchronous CPU acquisition of a graph image on every recording.
 *
 * Every recording starts an independent acquisition into memory it allocates itself, and hands the result to a callback
 * that takes ownership of it. Acquisitions never share memory, so consecutive recordings may be in flight together; wrap
 * in RTTriggeredVK for on-demand acquisition, and use RTTriggeredVK::enabled_for() to acquire a frame sequence.
 * Instances are neither copyable nor movable.
 */
class RTCPUInFlightAcquireVK final : public IRenderTaskVK {
public:
    /**
     * @struct AcquiredImage
     * @brief One completed acquisition. Movable; copying duplicates the content.
     */
    struct AcquiredImage {
        /**
         * @brief The renderer's current frame when the acquisition was recorded. Consecutive recorded frames have
         * consecutive values.
         */
        uint64_t frame = 0;
        /**
         * @brief The format of the source image.
         */
        VkFormat format = VK_FORMAT_UNDEFINED;
        /**
         * @brief The extent of the source image's first mip level.
         */
        VkExtent3D extent{};
        /**
         * @brief The tightly packed image bytes, laid out as ROImageVK::acquire_image_content() describes. Allocated from
         * the global allocator.
         */
        Vector<uint8_t, HeapAllocatorGlobal> content;
    };

    /**
     * @brief Callback receiving the ownership of one completed acquisition.
     *
     * Runs on an engine worker thread. Callbacks of different acquisitions may run concurrently and out of recording
     * order; order them by AcquiredImage::frame. An exception thrown by the callback is rethrown by a later recording.
     */
    using Callback = std::function<void(AcquiredImage p_image)>;

    RTCPUInFlightAcquireVK() = delete;
    virtual ~RTCPUInFlightAcquireVK() override = default;
    RTCPUInFlightAcquireVK(const RTCPUInFlightAcquireVK&) = delete;
    RTCPUInFlightAcquireVK(RTCPUInFlightAcquireVK&&) noexcept = delete;
    RTCPUInFlightAcquireVK& operator=(const RTCPUInFlightAcquireVK&) = delete;
    RTCPUInFlightAcquireVK& operator=(RTCPUInFlightAcquireVK&&) noexcept = delete;

    /**
     * @brief Construct an in-flight CPU image-acquisition task.
     *
     * Each acquisition holds its own copy of the callback, so the task may be destroyed while acquisitions are in flight;
     * everything the callback references must outlive every acquisition the task starts.
     *
     * @param p_render_data Base Vulkan render data.
     * @param p_image_view_id Render-graph resource id of the source image view.
     * @param p_on_acquired Callback receiving each completed acquisition; empty throws.
     */
    RTCPUInFlightAcquireVK(const BaseRenderDataVK* p_render_data, std::string_view p_image_view_id, Callback p_on_acquired);

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override;

    virtual void bind_resources(const RendResViewVK& p_resources) override;

    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    std::string image_view_id;
    Callback on_acquired;
    Ref<RenderObjectContainerVK> image_view_container;
    Vector<std::future<void>, RenderAllocator> pending_acquisitions;
};

} // namespace WhiteBirdEngine

#endif
