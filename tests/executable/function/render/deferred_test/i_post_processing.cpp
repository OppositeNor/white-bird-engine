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
#include "i_post_processing.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/event/event_manager.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/rt_toggle_vk.hh"
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

IPostProcessing::IPostProcessing(RenderAllocator* p_allocator, std::string_view p_toggle_event_id)
    : allocator(p_allocator), toggle_event_id(p_toggle_event_id) {
    if (allocator == nullptr) {
        throw std::runtime_error("Failed to create a post-processing effect: allocator is null.");
    }
    if (toggle_event_id.empty()) {
        throw std::runtime_error("Failed to create a post-processing effect: the toggle event id is empty.");
    }
}

void IPostProcessing::set_enabled(bool p_enabled) {
    enabled = p_enabled;
    EventManager::get_singleton()->emit(toggle_event_id, RTToggleVK::Command{.enabled = p_enabled});
}

Unique<IRenderTaskVK> IPostProcessing::make_toggle_task(Unique<IRenderTaskVK> p_task) const {
    // A task created by a rebuild starts in the current state; later set_enabled() calls reach it through the event.
    return make_unique<RTToggleVK>(allocator, std::move(p_task), std::string_view(toggle_event_id), enabled);
}

} // namespace WhiteBirdEngine
