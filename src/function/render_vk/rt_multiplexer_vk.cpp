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

#include "function/render_vk/rt_multiplexer_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/unique.hh"
#include "function/event/event_manager.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTMultiplexerVK::RTMultiplexerVK(Vector<Unique<IRenderTaskVK>, RenderAllocator> p_branches, uint32_t p_branch)
    : branches(std::move(p_branches)), branch(0) {
    if (branches.empty()) {
        throw std::runtime_error("RTMultiplexerVK: at least one branch is required.");
    }
    for (size_t i = 0; i < branches.size(); ++i) {
        if (branches[i] == MEM_NULL) {
            throw std::runtime_error(std::format("RTMultiplexerVK: branch {} is null.", i));
        }
    }
    const QueueFamilyVK family = branches[0]->queue_family();
    for (size_t i = 1; i < branches.size(); ++i) {
        if (branches[i]->queue_family() != family) {
            throw std::runtime_error(std::format("RTMultiplexerVK: branch {} runs on a different queue family than branch 0.", i));
        }
    }
    set_branch(p_branch);
}

RTMultiplexerVK::RTMultiplexerVK(Vector<Unique<IRenderTaskVK>, RenderAllocator> p_branches, std::string_view p_event_id, uint32_t p_branch)
    : RTMultiplexerVK(std::move(p_branches), p_branch) {
    if (p_event_id.empty()) {
        throw std::runtime_error("RTMultiplexerVK: event id is empty.");
    }
    EventManager* event_manager = EventManager::get_singleton();
    if (event_manager == nullptr) {
        throw std::runtime_error("RTMultiplexerVK: no EventManager exists to receive branch commands from.");
    }
    command_registration = event_manager->subscribe<Command>(p_event_id, [this](const Command& p_command) {
        set_branch(p_command.branch);
    });
}

RendTaskFactoryVK RTMultiplexerVK::combine_factories(Vector<RendTaskFactoryVK, RenderAllocator> p_branch_factories,
    std::string p_event_id,
    const RenderContextVK* p_render_data,
    uint32_t p_branch) {
    if (p_branch_factories.empty()) {
        throw std::runtime_error("RTMultiplexerVK: at least one branch factory is required.");
    }
    for (size_t i = 0; i < p_branch_factories.size(); ++i) {
        if (!p_branch_factories[i]) {
            throw std::runtime_error(std::format("RTMultiplexerVK: branch factory {} is empty.", i));
        }
    }
    if (p_branch >= p_branch_factories.size()) {
        throw std::runtime_error(
            std::format("RTMultiplexerVK: branch {} is out of range for {} branches.", p_branch, p_branch_factories.size()));
    }
    if (p_event_id.empty()) {
        throw std::runtime_error("RTMultiplexerVK: event id is empty.");
    }
    if (p_render_data == nullptr || p_render_data->allocator == nullptr) {
        throw std::runtime_error("RTMultiplexerVK: render data is unavailable.");
    }
    if (EventManager::get_singleton() == nullptr) {
        throw std::runtime_error("RTMultiplexerVK: no EventManager exists to receive branch commands from.");
    }
    return [factories = std::move(p_branch_factories), event_id = std::move(p_event_id), p_render_data, p_branch](
               const RendGraphContextVK& p_context) -> Unique<IRenderTaskVK> {
        Vector<Unique<IRenderTaskVK>, RenderAllocator> branch_tasks(p_render_data->allocator);
        branch_tasks.reserve(factories.size());
        for (const RendTaskFactoryVK& factory : factories) {
            branch_tasks.push_back(factory(p_context));
        }
        return make_unique<RTMultiplexerVK>(p_render_data->allocator, std::move(branch_tasks), std::string_view(event_id), p_branch);
    };
}

void RTMultiplexerVK::set_branch(uint32_t p_branch) {
    if (p_branch >= branches.size()) {
        throw std::runtime_error(std::format("RTMultiplexerVK: branch {} is out of range for {} branches.", p_branch, branches.size()));
    }
    branch.store(p_branch, std::memory_order_release);
}

QueueFamilyVK RTMultiplexerVK::queue_family() const {
    return branches[0]->queue_family();
}

void RTMultiplexerVK::append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const {
    for (const Unique<IRenderTaskVK>& task : branches) {
        task->append_accesses(p_reads, p_writes);
    }
}

void RTMultiplexerVK::bind_resources(const RendResViewVK& p_resources) {
    for (Unique<IRenderTaskVK>& task : branches) {
        task->bind_resources(p_resources);
    }
}

void RTMultiplexerVK::record(VkCommandBuffer p_command_buffer, double p_delta) {
    branches[get_branch()]->record(p_command_buffer, p_delta);
}

} // namespace WhiteBirdEngine
