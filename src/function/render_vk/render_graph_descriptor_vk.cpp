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
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_task_vk.hh"
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

struct RendTaskDescVKDirectTaskState {
    explicit RendTaskDescVKDirectTaskState(Unique<RenderTaskVK> p_task) : task(std::move(p_task)) {
        if (task.is_null()) {
            throw std::runtime_error("Failed to construct render task descriptor: direct task is null.");
        }
    }
    ~RendTaskDescVKDirectTaskState() = default;
    RendTaskDescVKDirectTaskState(const RendTaskDescVKDirectTaskState&) = delete;
    RendTaskDescVKDirectTaskState(RendTaskDescVKDirectTaskState&&) noexcept = delete;
    RendTaskDescVKDirectTaskState& operator=(const RendTaskDescVKDirectTaskState&) = delete;
    RendTaskDescVKDirectTaskState& operator=(RendTaskDescVKDirectTaskState&&) noexcept = delete;

    Unique<RenderTaskVK> take_task() {
        if (task.is_null()) {
            throw std::runtime_error("Failed to build render graph: direct task descriptor was already consumed.");
        }
        return std::move(task);
    }

    Unique<RenderTaskVK> task;
};

RendTaskDescVK::RendTaskDescVK(RenderAllocator* p_allocator) : prev(p_allocator) {
}

RendTaskDescVK::RendTaskDescVK(RenderAllocator* p_allocator, RendTaskID p_task_id, Unique<RenderTaskVK> p_task)
    : task_id(p_task_id), prev(p_allocator) {
    Ref<RendTaskDescVKDirectTaskState> direct_task_state = make_ref<RendTaskDescVKDirectTaskState>(p_allocator, std::move(p_task));
    create = [direct_task_state = std::move(direct_task_state)]() mutable -> Unique<RenderTaskVK> {
        return direct_task_state->take_task();
    };
}

} // namespace WhiteBirdEngine
