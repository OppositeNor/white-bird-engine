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
#ifndef WBE_FILE_RT_TRIGGERED_VK_HH
#define WBE_FILE_RT_TRIGGERED_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include <atomic>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTTriggeredVK
 * @brief Owns a task and records it once per activation. Neither copyable nor movable.
 * @details Binding, initialization, queue selection and resource declarations always reach the child.
 * Inactive frames record no child commands; the graph still submits its synchronization and any
 * required layout transitions. Activation is consumed before recording, so a new activation during
 * recording remains pending. Repeated stores of true coalesce into one request, not a queue.
 * Access this render object on the rendering thread; submit requests from other threads through render jobs.
 */
class RTTriggeredVK final : public IRenderTaskVK {
public:
    RTTriggeredVK() = delete;
    virtual ~RTTriggeredVK() override = default;
    RTTriggeredVK(const RTTriggeredVK&) = delete;
    RTTriggeredVK(RTTriggeredVK&&) noexcept = delete;
    RTTriggeredVK& operator=(const RTTriggeredVK&) = delete;
    RTTriggeredVK& operator=(RTTriggeredVK&&) noexcept = delete;

    /**
     * @brief Take ownership of a non-null child. Starts inactive unless explicitly activated.
     * @param p_task Task owned exclusively by this wrapper; null throws.
     * @param p_activate Whether the next recording should execute the child.
     */
    explicit RTTriggeredVK(Unique<IRenderTaskVK> p_task, bool p_activate = false);

    /**
     * @brief Set true to execute the child on its next graph recording; consumed atomically before execution.
     */
    std::atomic_bool activate{false};

    virtual QueueFamilyVK queue_family() const override;
    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override;
    virtual void bind_resources(const RendResViewVK& p_resources) override;
    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    Unique<IRenderTaskVK> task;
};

} // namespace WhiteBirdEngine

#endif
