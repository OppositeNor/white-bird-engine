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
#include "function/render_vk/i_render_task_vk.hh"
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

// Records the wrapped task only while its post-processing effect is enabled.
class RTToggleableVK final : public IRenderTaskVK {
public:
    RTToggleableVK(Unique<IRenderTaskVK> p_task, const IPostProcessing* p_effect)
        : task(std::move(p_task)), effect(p_effect) {
        if (task == MEM_NULL || effect == nullptr) {
            throw std::runtime_error("Failed to create a toggleable post-processing task: task or effect is null.");
        }
    }
    ~RTToggleableVK() override = default;
    RTToggleableVK(const RTToggleableVK&) = delete;
    RTToggleableVK(RTToggleableVK&&) noexcept = delete;
    RTToggleableVK& operator=(const RTToggleableVK&) = delete;
    RTToggleableVK& operator=(RTToggleableVK&&) noexcept = delete;

    QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }

    void bind_resources(const RendResViewVK& p_resources) override {
        task->bind_resources(p_resources);
    }

    void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        task->append_accesses(p_reads, p_writes);
    }

    void record(VkCommandBuffer p_command_buffer, double p_delta) override {
        if (effect->is_enabled()) {
            task->record(p_command_buffer, p_delta);
        }
    }

private:
    Unique<IRenderTaskVK> task;
    const IPostProcessing* effect;
};

} // namespace

IPostProcessing::IPostProcessing(RenderAllocator* p_allocator)
    : allocator(p_allocator) {
    if (allocator == nullptr) {
        throw std::runtime_error("Failed to create a post-processing effect: allocator is null.");
    }
}

Unique<IRenderTaskVK> IPostProcessing::make_toggleable_task(Unique<IRenderTaskVK> p_task) const {
    return make_unique<RTToggleableVK>(allocator, std::move(p_task), this);
}

} // namespace WhiteBirdEngine
