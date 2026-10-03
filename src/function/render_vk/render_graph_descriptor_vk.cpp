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
#include "function/render_vk/render_resource_list_vk.hh"
#include "function/render/render_task_resource_map.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "function/render_vk/i_bindable_render_resource_vk.hh"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RendTaskDescVK::RendTaskDescVK(RenderAllocator* p_allocator) : next(p_allocator), resource_map(p_allocator) {
}

RendTaskDescVK::RendTaskDescVK(RenderAllocator* p_allocator, std::string_view p_task_id, Unique<IRenderTaskVK> p_task)
    : task_id(p_task_id), next(p_allocator), resource_map(p_allocator), task(std::move(p_task)) {
    if (task.is_null()) {
        throw std::runtime_error("Failed to construct render task descriptor: task is null.");
    }
}

RendTaskDescVK& RendTaskDescVK::connect(std::string_view p_next_task_id, std::string_view p_resource_id) {
    next.push_back({
        .next_task_id = std::string(p_next_task_id),
        .res_id = std::string(p_resource_id),
    });
    return *this;
}

RendTaskDescVK& RendTaskDescVK::alias(std::string_view p_task_resource_id, std::string_view p_graph_resource_id) {
    resource_map.bind(p_task_resource_id, p_graph_resource_id);
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

RendTaskDescVK& RendGraphDescVK::get_task(std::string_view p_task_id) {
    for (RendTaskDescVK& task : tasks) {
        if (task.task_id == p_task_id) {
            return task;
        }
    }
    throw std::runtime_error(std::format("Failed to get render graph task: no task has id '{}'.", p_task_id));
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

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, const IBindableRenderResourceVK& p_resource) {
    p_resource.bind(*this, p_resource_id);
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_buffer(std::string_view p_resource_id, RendBufferDescVK p_desc) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph buffer: resource id is empty.");
    }
    buffers.push_back({.res_id = std::string(p_resource_id), .desc = std::move(p_desc)});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_image(std::string_view p_resource_id, RendImageDescVK p_desc) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph image: resource id is empty.");
    }
    images.push_back({.res_id = std::string(p_resource_id), .desc = p_desc});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_image_view(std::string_view p_resource_id, std::string_view p_image_id, RendImageViewDescVK p_desc) {
    if (p_resource_id.empty() || p_image_id.empty()) {
        throw std::runtime_error("Failed to add render graph image view: resource id or image id is empty.");
    }
    image_views.push_back({.res_id = std::string(p_resource_id), .image_id = std::string(p_image_id), .desc = p_desc});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_sampler(std::string_view p_resource_id, const RendSamplerDescVK& p_desc) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph sampler: resource id is empty.");
    }
    samplers.push_back({.res_id = std::string(p_resource_id), .desc = p_desc});
    return *this;
}

} // namespace WhiteBirdEngine
