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
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_task.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

struct RendTaskDescVKDirectTaskState {
    explicit RendTaskDescVKDirectTaskState(Unique<IRenderTaskVK> p_task) : task(std::move(p_task)) {
        if (task.is_null()) {
            throw std::runtime_error("Failed to construct render task descriptor: direct task is null.");
        }
    }
    ~RendTaskDescVKDirectTaskState() = default;
    RendTaskDescVKDirectTaskState(const RendTaskDescVKDirectTaskState&) = delete;
    RendTaskDescVKDirectTaskState(RendTaskDescVKDirectTaskState&&) noexcept = delete;
    RendTaskDescVKDirectTaskState& operator=(const RendTaskDescVKDirectTaskState&) = delete;
    RendTaskDescVKDirectTaskState& operator=(RendTaskDescVKDirectTaskState&&) noexcept = delete;

    Unique<IRenderTaskVK> take_task() {
        if (task.is_null()) {
            throw std::runtime_error("Failed to build render graph: direct task descriptor was already consumed.");
        }
        return std::move(task);
    }

    Unique<IRenderTaskVK> task;
};

RendTaskDescVK::RendTaskDescVK(RenderAllocator* p_allocator) : prev(p_allocator), reads(p_allocator), writes(p_allocator) {
}

RendTaskDescVK::RendTaskDescVK(RenderAllocator* p_allocator, std::string_view p_task_id, Unique<IRenderTaskVK> p_task)
    : task_id(p_task_id), prev(p_allocator), reads(p_allocator), writes(p_allocator) {
    Ref<RendTaskDescVKDirectTaskState> direct_task_state = make_ref<RendTaskDescVKDirectTaskState>(p_allocator, std::move(p_task));
    create = [direct_task_state = std::move(direct_task_state)]() mutable -> Unique<IRenderTaskVK> {
        return direct_task_state->take_task();
    };
}

RendTaskDescVK& RendTaskDescVK::after(std::string_view p_task_id) {
    prev.emplace_back(p_task_id);
    return *this;
}

RendTaskDescVK& RendTaskDescVK::read(std::string_view p_resource_id, RendResUsageVK p_usage) {
    reads.push_back({.res_id = std::string(p_resource_id), .usage = p_usage});
    return *this;
}

RendTaskDescVK& RendTaskDescVK::write(std::string_view p_resource_id, RendResUsageVK p_usage) {
    writes.push_back({.res_id = std::string(p_resource_id), .usage = p_usage});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_task(RendTaskDescVK p_task_desc) {
    tasks.emplace_back(std::move(p_task_desc));
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_task(std::string_view p_task_id, Unique<IRenderTaskVK> p_task) {
    tasks.emplace_back(allocator, p_task_id, std::move(p_task));
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_resource(RendResVK p_resource) {
    if (p_resource.container != MEM_NULL) {
        RendResTID concrete_type_id = p_resource.container->get_resource_type_id();
        if (concrete_type_id == 0) {
            throw std::runtime_error("Failed to add render graph resource: container has no concrete resource type.");
        }
        if (p_resource.res_type_id != 0 && p_resource.res_type_id != concrete_type_id) {
            throw std::runtime_error("Failed to add render graph resource: container type does not match the declared type.");
        }
        p_resource.res_type_id = concrete_type_id;
    }
    resources.emplace_back(std::move(p_resource));
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, Ref<IRenderObjectVK> p_resource) {
    if (p_resource == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to add render graph resource: render object for slot: {} is null.", p_resource_id));
    }
    return add_resource(p_resource_id, make_ref<ROCSingleVK>(allocator, std::move(p_resource)));
}

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, RendResTID p_resource_type_id) {
    return add_resource(RendResVK{.res_id = std::string(p_resource_id), .res_type_id = p_resource_type_id, .container = MEM_NULL});
}

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, Ref<RenderObjectContainerVK> p_resource_container) {
    if (p_resource_container == MEM_NULL) {
        throw std::runtime_error("Failed to add render graph resource: resource container is null.");
    }
    return add_resource(RendResVK{
        .res_id = std::string(p_resource_id),
        .res_type_id = p_resource_container->get_resource_type_id(),
        .container = std::move(p_resource_container),
    });
}

} // namespace WhiteBirdEngine
