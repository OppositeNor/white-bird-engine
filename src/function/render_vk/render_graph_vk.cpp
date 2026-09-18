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
#include "function/render_vk/render_graph_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/descriptor_heap_allocator_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/roc_per_frame_vk.hh"
#include "function/render_vk/semaphore_vk.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {


namespace {

Ref<IRenderObjectVK> resolve_render_object(const RendResVK& p_resource) {
    if (p_resource.container == MEM_NULL) {
        return MEM_NULL;
    }
    return p_resource.container->get().dynamic_cast_ref<IRenderObjectVK>();
}

DebugData make_descriptor_data(const DebugDataRows& p_rows) {
    DebugDataArray rows;
    rows.reserve(p_rows.size());
    for (const DebugData& row : p_rows) {
        rows.push_back(row);
    }
    DebugData root;
    root.set(std::move(rows));
    return root;
}

uint32_t descriptor_heap_index(const DebugData& p_row) {
    return p_row.get_value<uint32_t>("heap_index");
}

std::string descriptor_heap(const DebugData& p_row) {
    return p_row.get_value<std::string>("heap");
}

std::string descriptor_resource_id(const DebugData& p_row) {
    return p_row.get_value<std::string>("resource_id");
}

int64_t descriptor_frame_index(const DebugData& p_row) {
    if (!p_row.contains("frame_index")) {
        return -1;
    }
    return static_cast<int64_t>(p_row.get_value<uint32_t>("frame_index"));
}

std::string descriptor_type_name(const DebugData& p_row) {
    return p_row.get_value<std::string>("descriptor_type");
}

VkQueue queue_for(const BaseRenderDataVK* p_render_data, QueueFamilyVK p_queue_family) {
    switch (p_queue_family) {
    case QueueFamilyVK::GRAPHICS:
        return p_render_data->graphics_queue;
    case QueueFamilyVK::COMPUTE:
        return p_render_data->compute_queue;
    }
    return VK_NULL_HANDLE;
}

VkCommandPool pool_for(const BaseRenderDataVK* p_render_data, QueueFamilyVK p_queue_family) {
    switch (p_queue_family) {
    case QueueFamilyVK::GRAPHICS:
        return p_render_data->graphics_command_pool;
    case QueueFamilyVK::COMPUTE:
        return p_render_data->compute_command_pool;
    }
    return VK_NULL_HANDLE;
}

struct ResourceScopeVK {
    VkAccessFlags2 access = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
};

QueueFamilyVK node_queue_family(Ref<RGNode> p_node) {
    return p_node->in_queue;
}

VkPipelineStageFlags2 shader_stage_scope(QueueFamilyVK p_queue_family) {
    switch (p_queue_family) {
    case QueueFamilyVK::GRAPHICS:
        return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    case QueueFamilyVK::COMPUTE:
        return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    throw std::runtime_error("RenderGraphVK: unknown queue family.");
}

void ensure_graphics_queue_usage(QueueFamilyVK p_queue_family, RendResUsageVK p_usage) {
    if (p_queue_family == QueueFamilyVK::COMPUTE) {
        throw std::runtime_error(
            std::format("RenderGraphVK: resource usage {} is not valid on the compute queue.", std::to_underlying(p_usage)));
    }
}

ResourceScopeVK resource_scope(RendResUsageVK p_usage, bool p_write, QueueFamilyVK p_queue_family) {
    switch (p_usage) {
    case RendResUsageVK::SAMPLED_IMAGE:
        if (p_write) {
            break;
        }
        return {.access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = shader_stage_scope(p_queue_family)};
    case RendResUsageVK::COLOR_ATTACHMENT:
        ensure_graphics_queue_usage(p_queue_family, p_usage);
        return {.access = p_write ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
            .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
    case RendResUsageVK::DEPTH_ATTACHMENT:
        ensure_graphics_queue_usage(p_queue_family, p_usage);
        return {.access = p_write ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT};
    case RendResUsageVK::STORAGE_IMAGE:
        return {.access = p_write ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT : VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            .stage = shader_stage_scope(p_queue_family)};
    case RendResUsageVK::UNIFORM_BUFFER:
        if (p_write) {
            break;
        }
        return {.access = VK_ACCESS_2_UNIFORM_READ_BIT, .stage = shader_stage_scope(p_queue_family)};
    case RendResUsageVK::STORAGE_BUFFER:
        return {.access = p_write ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT : VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            .stage = shader_stage_scope(p_queue_family)};
    case RendResUsageVK::TRANSFER_SRC:
        if (p_write) {
            break;
        }
        return {.access = VK_ACCESS_2_TRANSFER_READ_BIT, .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    case RendResUsageVK::TRANSFER_DST:
        if (!p_write) {
            break;
        }
        return {.access = VK_ACCESS_2_TRANSFER_WRITE_BIT, .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    case RendResUsageVK::INDIRECT_BUFFER:
        if (p_write) {
            break;
        }
        ensure_graphics_queue_usage(p_queue_family, p_usage);
        return {.access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, .stage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT};
    case RendResUsageVK::VERTEX_BUFFER:
        if (p_write) {
            break;
        }
        ensure_graphics_queue_usage(p_queue_family, p_usage);
        return {.access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, .stage = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT};
    case RendResUsageVK::INDEX_BUFFER:
        if (p_write) {
            break;
        }
        ensure_graphics_queue_usage(p_queue_family, p_usage);
        return {.access = VK_ACCESS_2_INDEX_READ_BIT, .stage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT};
    }
    throw std::runtime_error("RenderGraphVK: resource usage is invalid for the declared read/write direction.");
}

void validate_declared_resource(
    const Map<std::string, Ref<RendResVK>, RenderAllocator>& p_resources, const RGNode& p_node, const ResAccessVK& p_access, bool p_is_write) {
    if (!p_resources.contains(p_access.res_id)) {
        throw std::runtime_error(std::format("RenderGraphVK: task '{}' {} undeclared resource '{}'.",
            p_node.task_id,
            p_is_write ? "writes" : "reads",
            p_access.res_id));
    }
}

bool accesses_conflict(bool p_before_is_write, bool p_after_is_write) {
    return p_before_is_write || p_after_is_write;
}

Ref<RGExecEdge> find_direct_exec_edge(Ref<RGNode> p_before, Ref<RGNode> p_after) {
    for (auto& weak_edge : p_before->exec_after) {
        auto edge = weak_edge.lock();
        if (edge != MEM_NULL && edge->after == p_after) {
            return edge;
        }
    }
    return MEM_NULL;
}

void update_edge_resource_scopes(Ref<RGExecEdge> p_edge,
    QueueFamilyVK p_before_queue,
    const ResAccessVK& p_before_access,
    bool p_before_is_write,
    QueueFamilyVK p_after_queue,
    const ResAccessVK& p_after_access,
    bool p_after_is_write) {
    if (p_edge == MEM_NULL || !accesses_conflict(p_before_is_write, p_after_is_write)) {
        return;
    }
    ResourceScopeVK before_scope = resource_scope(p_before_access.usage, p_before_is_write, p_before_queue);
    ResourceScopeVK after_scope = resource_scope(p_after_access.usage, p_after_is_write, p_after_queue);
    p_edge->before_finish_pipeline_stage |= before_scope.stage;
    p_edge->after_start_pipeline_stage |= after_scope.stage;
}

} // namespace

RenderGraphVK::RenderGraphVK(const BaseRenderDataVK* p_render_data)
    : render_data(p_render_data), nodes(p_render_data->allocator), exec_edges(p_render_data->allocator),
      all_resources(p_render_data->allocator), command_buffers(p_render_data->allocator),
      command_buffer_pools(p_render_data->allocator), terminal_semaphores(p_render_data->allocator) {
}

RenderGraphVK::~RenderGraphVK() {
    free_command_buffers();
}

bool RendResViewVK::contains(std::string_view p_resource_id) const {
    auto resource = resources->find(p_resource_id);
    return resource != resources->end() && resource->second != MEM_NULL && resource->second->container != MEM_NULL;
}

const RendResVK& RendResViewVK::get(std::string_view p_resource_id) const {
    auto resource = resources->find(p_resource_id);
    if (resource == resources->end() || resource->second == MEM_NULL || resource->second->container == MEM_NULL) {
        throw std::runtime_error(std::format("Render graph resource was not declared: {}.", p_resource_id));
    }
    return *resource->second;
}

DebugDataRows RendResViewVK::descriptor_debug_data() const {
    RenderAllocator* allocator = resources->get_allocator().allocator;
    DebugDataRows rows(allocator);
    for (const auto& [resource_id, resource] : *resources) {
        if (resource == MEM_NULL || resource->container == MEM_NULL) {
            continue;
        }
        DebugDataRows resource_rows = resource->container->descriptor_debug_data();
        for (DebugData& row : resource_rows) {
            row.set_value("resource_id", resource_id);
            rows.emplace_back(std::move(row));
        }
    }
    std::sort(rows.begin(), rows.end(), [](const DebugData& p_left, const DebugData& p_right) {
        return std::make_tuple(descriptor_heap_index(p_left),
                   descriptor_heap(p_left),
                   descriptor_resource_id(p_left),
                   descriptor_frame_index(p_left),
                   descriptor_type_name(p_left)) < std::make_tuple(descriptor_heap_index(p_right),
                                                       descriptor_heap(p_right),
                                                       descriptor_resource_id(p_right),
                                                       descriptor_frame_index(p_right),
                                                       descriptor_type_name(p_right));
    });
    return rows;
}

Ref<RGNode> RenderGraphVK::add_node(Unique<RenderTaskVK> p_task) {
    auto node = make_ref<RGNode>(render_data->allocator, render_data->allocator);
    node->render_task = std::move(p_task);
    node->in_queue = node->render_task->queue_family();
    nodes.emplace_back(node);
    return node;
}

Ref<RGExecEdge> RenderGraphVK::add_exec_edge(
    Ref<RGNode> p_before, Ref<RGNode> p_after, VkPipelineStageFlags2 p_before_finish_stage, VkPipelineStageFlags2 p_after_start_stage) {
    auto edge = make_ref<RGExecEdge>(render_data->allocator);
    edge->before = p_before;
    edge->after = p_after;
    edge->before_finish_pipeline_stage = p_before_finish_stage;
    edge->after_start_pipeline_stage = p_after_start_stage;
    // TODO: Use a pipeline barrier for execution edges whose nodes run on the
    // same queue, and reserve semaphores for cross-queue edges.
    edge->semaphore = make_ref<SemaphoreVK>(render_data->allocator, render_data, false);
    p_before->exec_after.emplace_back(edge);
    p_after->exec_before.emplace_back(edge);
    exec_edges.emplace_back(edge);
    return edge;
}

void RenderGraphVK::topological_sort() { // NOLINT
    // Kahn's algorithm using exec_before edges.
    Vector<size_t, RenderAllocator> in_degree(nodes.size(), 0, render_data->allocator);
    for (size_t i = 0; i < nodes.size(); ++i) {
        in_degree[i] = nodes[i]->exec_before.size();
    }
    Vector<Ref<RGNode>, RenderAllocator> sorted(render_data->allocator);
    sorted.reserve(nodes.size());
    Vector<size_t, RenderAllocator> ready(render_data->allocator);
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (in_degree[i] == 0) {
            ready.push_back(i);
        }
    }
    while (!ready.empty()) {
        size_t node_index = ready.back();
        ready.pop_back();
        auto node = nodes[node_index];
        node->topo_order = sorted.size();
        sorted.push_back(node);
        for (auto& weak_edge : node->exec_after) {
            auto edge = weak_edge.lock();
            if (edge == MEM_NULL) {
                continue;
            }
            // Find consumer index.
            for (size_t i = 0; i < nodes.size(); ++i) {
                if (nodes[i] == edge->after) {
                    if (in_degree[i] > 0) {
                        --in_degree[i];
                    }
                    if (in_degree[i] == 0) {
                        ready.push_back(i);
                    }
                    break;
                }
            }
        }
    }
    if (sorted.size() != nodes.size()) {
        throw std::runtime_error("RenderGraphVK: cycle detected in render graph.");
    }
    nodes = std::move(sorted);
}

void RenderGraphVK::allocate_command_buffers() {
    const uint32_t frames = std::max<uint32_t>(render_data->frames_in_flight, 1U);
    const size_t total = static_cast<size_t>(frames) * nodes.size();
    command_buffers.assign(total, VK_NULL_HANDLE);
    command_buffer_pools.assign(total, VK_NULL_HANDLE);
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto queue_family = nodes[i]->in_queue;
        VkCommandPool command_pool = pool_for(render_data, queue_family);
        if (command_pool == VK_NULL_HANDLE) {
            throw std::runtime_error(
                std::format("Failed to create command pool for queue family: {}.", std::to_underlying(queue_family)));
        }
        for (uint32_t f = 0; f < frames; ++f) {
            const size_t slot = static_cast<size_t>(f) * nodes.size() + i;
            command_buffer_pools[slot] = command_pool;
            VkCommandBufferAllocateInfo alloc_info{};
            alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            alloc_info.commandPool = command_pool;
            alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            alloc_info.commandBufferCount = 1;
            if (vkAllocateCommandBuffers(render_data->logic_device, &alloc_info, &command_buffers[slot]) != VK_SUCCESS) {
                throw std::runtime_error("RenderGraphVK: failed to allocate command buffer.");
            }
        }
    }
}

void RenderGraphVK::free_command_buffers() {
    if (!command_buffers.empty()) {
        vkDeviceWaitIdle(render_data->logic_device);
    }
    for (size_t i = 0; i < command_buffers.size(); ++i) {
        if (command_buffers[i] != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(render_data->logic_device, command_buffer_pools[i], 1, &command_buffers[i]);
            command_buffers[i] = VK_NULL_HANDLE;
        }
    }
    command_buffers.clear();
    command_buffer_pools.clear();
}

uint32_t RenderGraphVK::terminal_semaphore_index() const {
    if (terminal_semaphores.empty()) {
        throw std::runtime_error("RenderGraphVK: terminal semaphores are not allocated.");
    }
    if (render_data->current_swap_chain_index >= terminal_semaphores.size()) {
        throw std::runtime_error("RenderGraphVK: swap-chain image index exceeds terminal semaphore count.");
    }
    return render_data->current_swap_chain_index;
}

Ref<SemaphoreVK> RenderGraphVK::get_terminal_semaphore() const {
    return terminal_semaphores[terminal_semaphore_index()];
}

DebugData RenderGraphVK::list_descriptors() const {
    return make_descriptor_data(RendResViewVK(&all_resources).descriptor_debug_data());
}

void RenderGraphVK::build_from_descriptor(const RendGraphDescVK& p_desc) {
    free_command_buffers();
    nodes.clear();
    exec_edges.clear();
    all_resources.clear();

    all_resources = index_resources(p_desc);
    auto task_ids = instantiate_tasks(p_desc);
    add_exec_edges_from_prev(p_desc, task_ids);

    topological_sort();
    validate_explicit_resource_dependencies();

    allocate_command_buffers();

    // Allocate one terminal binary semaphore per swap-chain image. Presentation
    // may retain the wait semaphore until that exact image is acquired again.
    const uint32_t swap_chain_images = std::max<uint32_t>(render_data->swap_chain_images_count, 1U);
    terminal_semaphores.clear();
    terminal_semaphores.reserve(swap_chain_images);
    for (uint32_t i = 0; i < swap_chain_images; ++i) {
        terminal_semaphores.emplace_back(make_ref<SemaphoreVK>(render_data->allocator, render_data, true));
    }
}

Map<std::string, Ref<RendResVK>, RenderAllocator> RenderGraphVK::index_resources(const RendGraphDescVK& p_desc) {
    Map<std::string, Ref<RendResVK>, RenderAllocator> resources_by_id(render_data->allocator);
    resources_by_id.reserve(p_desc.resources.size());
    for (const auto& resource_desc : p_desc.resources) {
        if (resource_desc.res_id.empty()) {
            throw std::runtime_error("RenderGraphVK: resource id is empty.");
        }
        if (resource_desc.container == MEM_NULL) {
            throw std::runtime_error(
                std::format("RenderGraphVK: resource '{}' must be bound before graph build.", resource_desc.res_id));
        }
        if (resource_desc.container->get_resource_type_id() != resource_desc.res_type_id) {
            throw std::runtime_error(
                std::format("RenderGraphVK: resource container type does not match declaration for '{}'.", resource_desc.res_id));
        }
        if (resources_by_id.contains(resource_desc.res_id)) {
            throw std::runtime_error(std::format("RenderGraphVK: duplicate resource id: {}.", resource_desc.res_id));
        }
        auto resource_ref = make_ref<RendResVK>(render_data->allocator);
        resource_ref->res_id = resource_desc.res_id;
        resource_ref->res_type_id = resource_desc.res_type_id;
        resource_ref->container = resource_desc.container;
        resources_by_id.emplace(resource_desc.res_id, std::move(resource_ref));
    }
    return resources_by_id;
}

Vector<RendTaskID, RenderAllocator> RenderGraphVK::instantiate_tasks(const RendGraphDescVK& p_desc) {
    Vector<RendTaskID, RenderAllocator> task_ids(render_data->allocator);
    task_ids.reserve(p_desc.tasks.size());
    for (const auto& task_desc : p_desc.tasks) {
        if (task_desc.task_id.empty()) {
            throw std::runtime_error("RenderGraphVK: task id is empty.");
        }
        if (std::find(task_ids.begin(), task_ids.end(), task_desc.task_id) != task_ids.end()) {
            throw std::runtime_error(std::format("RenderGraphVK: duplicate task id: {}.", task_desc.task_id));
        }
        if (!task_desc.create) {
            throw std::runtime_error(std::format("RenderGraphVK: task factory is missing for: {}.", task_desc.task_id));
        }
        auto task = task_desc.create();
        if (task.is_null()) {
            throw std::runtime_error("RenderGraphVK: task factory returned null.");
        }
        Ref<RGNode> node = add_node(std::move(task));
        node->task_id = task_desc.task_id;
        node->render_task->bind_resources(RendResViewVK(&all_resources));
        node->reads = node->render_task->read_resources();
        node->writes = node->render_task->write_resources();
        node->reads.insert(node->reads.end(), task_desc.reads.begin(), task_desc.reads.end());
        node->writes.insert(node->writes.end(), task_desc.writes.begin(), task_desc.writes.end());
        task_ids.emplace_back(task_desc.task_id);
    }
    return task_ids;
}

void RenderGraphVK::add_exec_edges_from_prev(const RendGraphDescVK& p_desc, const Vector<RendTaskID, RenderAllocator>& p_task_ids) {
    auto node_by_task_id = [&](const RendTaskID& p_id) -> Ref<RGNode> {
        for (size_t i = 0; i < p_task_ids.size(); ++i) {
            if (p_task_ids[i] == p_id) {
                return nodes[i];
            }
        }
        return MEM_NULL;
    };
    for (size_t i = 0; i < p_desc.tasks.size(); ++i) {
        auto consumer = nodes[i];
        for (const auto& prev_id : p_desc.tasks[i].prev) {
            auto producer = node_by_task_id(prev_id);
            if (producer == MEM_NULL) {
                throw std::runtime_error(
                    std::format("RenderGraphVK: unknown prev task id: {} for task: {}", prev_id, consumer->task_id));
            }
            add_exec_edge(producer, consumer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
        }
    }
}

bool RenderGraphVK::has_exec_path(Ref<RGNode> p_from, Ref<RGNode> p_to) const {
    if (p_from == p_to) {
        return true;
    }

    Vector<uint8_t, RenderAllocator> visited(nodes.size(), 0, render_data->allocator);
    Vector<Ref<RGNode>, RenderAllocator> stack(render_data->allocator);
    stack.push_back(p_from);
    visited[p_from->topo_order] = 1;
    while (!stack.empty()) {
        Ref<RGNode> node = stack.back();
        stack.pop_back();
        for (auto& weak_edge : node->exec_after) {
            auto edge = weak_edge.lock();
            if (edge == MEM_NULL || edge->after == MEM_NULL) {
                continue;
            }
            if (edge->after == p_to) {
                return true;
            }
            if (visited[edge->after->topo_order] != 0) {
                continue;
            }
            visited[edge->after->topo_order] = 1;
            stack.push_back(edge->after);
        }
    }
    return false;
}

void RenderGraphVK::validate_explicit_resource_dependencies() {
    auto validate_access_list = [&](Ref<RGNode> p_node, const Vector<ResAccessVK, RenderAllocator>& p_accesses, bool p_is_write) {
        for (const auto& access : p_accesses) {
            validate_declared_resource(all_resources, *p_node, access, p_is_write);
        }
    };

    for (auto& node : nodes) {
        validate_access_list(node, node->reads, false);
        validate_access_list(node, node->writes, true);
    }

    auto check_conflicts = [&](Ref<RGNode> p_before,
                               const Vector<ResAccessVK, RenderAllocator>& p_before_accesses,
                               bool p_before_is_write,
                               Ref<RGNode> p_after,
                               const Vector<ResAccessVK, RenderAllocator>& p_after_accesses,
                               bool p_after_is_write) {
        Ref<RGExecEdge> direct_edge = find_direct_exec_edge(p_before, p_after);
        for (const auto& before_access : p_before_accesses) {
            for (const auto& after_access : p_after_accesses) {
                if (before_access.res_id != after_access.res_id || !accesses_conflict(p_before_is_write, p_after_is_write)) {
                    continue;
                }
                if (!has_exec_path(p_before, p_after)) {
                    throw std::runtime_error(std::format("RenderGraphVK: tasks '{}' and '{}' both access resource '{}' without an explicit execution path.",
                        p_before->task_id,
                        p_after->task_id,
                        before_access.res_id));
                }
                update_edge_resource_scopes(
                    direct_edge, p_before->in_queue, before_access, p_before_is_write, p_after->in_queue, after_access, p_after_is_write);
            }
        }
    };

    for (size_t before_index = 0; before_index < nodes.size(); ++before_index) {
        Ref<RGNode> before = nodes[before_index];
        for (size_t after_index = before_index + 1; after_index < nodes.size(); ++after_index) {
            Ref<RGNode> after = nodes[after_index];
            check_conflicts(before, before->reads, false, after, after->writes, true);
            check_conflicts(before, before->writes, true, after, after->reads, false);
            check_conflicts(before, before->writes, true, after, after->writes, true);
        }
    }
}

void RenderGraphVK::execute(uint64_t p_frame_index,
    double p_delta,
    std::span<const VkSemaphoreSubmitInfo> p_extra_waits_first_submit,
    std::span<const VkSemaphoreSubmitInfo> p_extra_signals_last_submit) {
    if (nodes.empty()) {
        return;
    }
    const uint64_t timeline_value = p_frame_index + 1;
    const auto [first_graphics_idx, last_graphics_idx] = find_graphics_range();

    // Each graph node owns one command buffer per frame slot and is submitted independently.
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto node = nodes[i];
        auto queue_family = node->in_queue;
        const uint32_t frame_slot = render_data->current_frame_slot;
        const size_t command_buffer_index = static_cast<size_t>(frame_slot) * nodes.size() + i;
        VkCommandBuffer command_buffer = command_buffers[command_buffer_index];

        if (vkResetCommandBuffer(command_buffer, 0) != VK_SUCCESS) {
            throw std::runtime_error("RenderGraphVK: failed to reset command buffer.");
        }

        RendResViewVK resource_view(&all_resources);
        const bool is_first_graphics = (i == first_graphics_idx && queue_family == QueueFamilyVK::GRAPHICS);
        const bool is_last_graphics = (i == last_graphics_idx && queue_family == QueueFamilyVK::GRAPHICS);
        // The first/last graphics nodes bridge external acquire/present synchronization.
        record_node(node, command_buffer, resource_view, p_delta, is_last_graphics);

        // Build wait/signal semaphore lists from exec edges + extras.
        Vector<VkSemaphoreSubmitInfo, RenderAllocator> wait_infos(render_data->allocator);
        Vector<VkSemaphoreSubmitInfo, RenderAllocator> signal_infos(render_data->allocator);
        collect_edge_semaphores(node, timeline_value, wait_infos, signal_infos);

        if (is_first_graphics) {
            for (const auto& wait_info : p_extra_waits_first_submit) {
                wait_infos.push_back(wait_info);
            }
        }
        if (is_last_graphics) {
            for (const auto& signal_info : p_extra_signals_last_submit) {
                signal_infos.push_back(signal_info);
            }
            // Also signal the terminal binary semaphore for this swap-chain image.
            signal_infos.push_back(get_terminal_semaphore()->get_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT));
        }

        // Last graphics submit signals the in-flight fence too, so the renderer
        // may wait for the entire frame's GPU work to complete next time
        // around.
        VkFence fence = is_last_graphics ? render_data->in_flight_fence : VK_NULL_HANDLE;
        submit_node(command_buffer, queue_family, wait_infos, signal_infos, fence);
    }
}

std::tuple<size_t, size_t> RenderGraphVK::find_graphics_range() const {
    size_t first_index = nodes.size();
    size_t last_index = nodes.size();
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i]->in_queue == QueueFamilyVK::GRAPHICS) {
            if (first_index == nodes.size()) {
                first_index = i;
            }
            last_index = i;
        }
    }
    return {first_index, last_index};
}

void RenderGraphVK::record_node(
    Ref<RGNode> p_node, VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double p_delta, bool p_record_present_transition) {
    // Record all barriers and task work for one node into its per-frame primary command buffer.
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(p_command_buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("RenderGraphVK: failed to begin command buffer.");
    }
    if (render_data->descriptor_heap_allocator == nullptr) {
        throw std::runtime_error("RenderGraphVK: descriptor heap allocator is unavailable.");
    }
    render_data->descriptor_heap_allocator->bind_heaps(p_command_buffer);

    record_general_transition(p_command_buffer, p_node, p_resources);

    p_node->render_task->record(p_command_buffer, p_delta);

    if (p_record_present_transition) {
        // Present waits on this node's terminal semaphore, so the present layout transition must be recorded before it is signaled.
        record_present_transition(p_command_buffer);
    }

    if (vkEndCommandBuffer(p_command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("RenderGraphVK: failed to end command buffer.");
    }
}

void RenderGraphVK::record_general_transition(VkCommandBuffer p_command_buffer, Ref<RGNode> p_node, const RendResViewVK& p_resources) {
    // We're already using unified shader layout, but we still need an explicit transition out of UNDEFINED/PRESENT.
    QueueFamilyVK queue_family = node_queue_family(p_node);
    for (const auto& write : p_node->writes) {
        const auto& bound_resource = p_resources.get(write.res_id);
        auto image_view = resolve_render_object(bound_resource).dynamic_cast_ref<ROImageViewVK>();
        if (image_view == MEM_NULL || image_view->fields().image == MEM_NULL) {
            continue;
        }
        Ref<ROImageVK> image = image_view->fields().image;
        if (image->fields().image_layout == VK_IMAGE_LAYOUT_GENERAL) {
            // Filter out images that are already in general layout.
            continue;
        }
        ResourceScopeVK write_scope = resource_scope(write.usage, true, queue_family);
        ROImageVK::MemoryBarrierBuilder()
            .set_src_access(VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE)
            .set_dst_access(write_scope.access, write_scope.stage)
            .set_subresource_range(image_view->fields().subresource_range)
            .set_new_layout(VK_IMAGE_LAYOUT_GENERAL)
            .set_image(image)
            .apply_barrier(p_command_buffer);
    }
}

void RenderGraphVK::record_present_transition(VkCommandBuffer p_command_buffer) {
    // WSI presentation still requires PRESENT_SRC_KHR even when unified image layouts are enabled.
    RendererVK* renderer = RendererVK::get_singleton();
    if (renderer == nullptr) {
        throw std::runtime_error("RenderGraphVK: renderer singleton is unavailable.");
    }
    Ref<ROCPerFrameVK> swap_chain_images = renderer->get_swap_chain_image_container();
    if (swap_chain_images == MEM_NULL) {
        throw std::runtime_error("RenderGraphVK: swap-chain image container is unavailable.");
    }

    auto image_view = swap_chain_images->get(render_data->current_frame_slot).dynamic_cast_ref<ROImageViewVK>();
    if (image_view == MEM_NULL || image_view->fields().image == MEM_NULL) {
        throw std::runtime_error("RenderGraphVK: swap-chain image container did not resolve to an image view.");
    }

    Ref<ROImageVK> image = image_view->fields().image;
    if (!image->fields().is_swap_chain_image) {
        throw std::runtime_error("RenderGraphVK: swap-chain image resource does not reference a swap-chain image.");
    }

    // For the swapchain image, apply a barrier that transfers it into VK_IMAGE_LAYOUT_PRESENT_SRC_KHR.
    ROImageVK::MemoryBarrierBuilder()
        .set_src_access(VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT)
        .set_dst_access(VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE)
        .set_subresource_range(image_view->fields().subresource_range)
        .set_new_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        .set_image(image)
        .apply_barrier(p_command_buffer);
}

void RenderGraphVK::collect_edge_semaphores(Ref<RGNode> p_node,
    uint64_t p_timeline_value,
    Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_waits,
    Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_signals) const {
    for (auto& weak_edge : p_node->exec_before) {
        auto edge = weak_edge.lock();
        if (edge == MEM_NULL) {
            continue;
        }
        p_waits.push_back(edge->semaphore->get_submit_info(edge->after_start_pipeline_stage, p_timeline_value));
    }
    for (auto& weak_edge : p_node->exec_after) {
        auto edge = weak_edge.lock();
        if (edge == MEM_NULL) {
            continue;
        }
        p_signals.push_back(edge->semaphore->get_submit_info(edge->before_finish_pipeline_stage, p_timeline_value));
    }
}

void RenderGraphVK::submit_node(VkCommandBuffer p_command_buffer,
    QueueFamilyVK p_queue_family,
    const Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_waits,
    const Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_signals,
    VkFence p_fence) {
    VkCommandBufferSubmitInfo command_buffer_submit_info{};
    command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_submit_info.commandBuffer = p_command_buffer;

    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(p_waits.size());
    submit_info.pWaitSemaphoreInfos = p_waits.data();
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_buffer_submit_info;
    submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(p_signals.size());
    submit_info.pSignalSemaphoreInfos = p_signals.data();

    VkQueue queue = queue_for(render_data, p_queue_family);
    auto result = vkQueueSubmit2(queue, 1, &submit_info, p_fence);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("RenderGraphVK: vkQueueSubmit2 failed: {}", string_VkResult(result)));
    }
}

} // namespace WhiteBirdEngine
