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
#include "function/render_vk/rt_triggered_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTTriggeredVK::RTTriggeredVK(Unique<IRenderTaskVK> p_task, bool p_activate)
    : task(std::move(p_task)), remaining_frames(p_activate ? 1U : 0U) {
    if (task == MEM_NULL) {
        throw std::runtime_error("RTTriggeredVK: wrapped task must not be null.");
    }
}

void RTTriggeredVK::trigger() {
    uint32_t expected = 0;
    remaining_frames.compare_exchange_strong(expected, 1U, std::memory_order_acq_rel);
}

void RTTriggeredVK::enabled_for(uint32_t p_frames) {
    remaining_frames.store(p_frames, std::memory_order_release);
}

QueueFamilyVK RTTriggeredVK::queue_family() const {
    return task->queue_family();
}

void RTTriggeredVK::append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const {
    task->append_accesses(p_reads, p_writes);
}

void RTTriggeredVK::bind_resources(const RendResViewVK& p_resources) {
    task->bind_resources(p_resources);
}

void RTTriggeredVK::record(VkCommandBuffer p_command_buffer, double p_delta) {
    uint32_t frames = remaining_frames.load(std::memory_order_acquire);
    while (frames > 0 && !remaining_frames.compare_exchange_weak(frames, frames - 1, std::memory_order_acq_rel)) {
    }
    if (frames > 0) {
        task->record(p_command_buffer, p_delta);
    }
}

} // namespace WhiteBirdEngine
