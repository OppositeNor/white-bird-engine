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
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

RTTriggeredVK::RTTriggeredVK(Unique<IRenderTaskVK> p_task, bool p_activate) : activate(p_activate), task(std::move(p_task)) {
    if (task == MEM_NULL) {
        throw std::runtime_error("RTTriggeredVK: wrapped task must not be null.");
    }
}

QueueFamilyVK RTTriggeredVK::queue_family() const {
    return task->queue_family();
}

Vector<ResAccessVK, RenderAllocator> RTTriggeredVK::read_resources() const {
    return task->read_resources();
}

Vector<ResAccessVK, RenderAllocator> RTTriggeredVK::write_resources() const {
    return task->write_resources();
}

void RTTriggeredVK::bind_resources(const RendResViewVK& p_resources) {
    task->bind_resources(p_resources);
}

void RTTriggeredVK::init() {
    task->init();
}

void RTTriggeredVK::record(VkCommandBuffer p_command_buffer, double p_delta) {
    if (activate.exchange(false, std::memory_order_acq_rel)) {
        task->record(p_command_buffer, p_delta);
    }
}

} // namespace WhiteBirdEngine
