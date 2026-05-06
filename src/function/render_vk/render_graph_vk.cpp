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
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/semaphore_vk.hh"
#include "global/stl_allocator.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

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

} // namespace

RenderGraphVK::RenderGraphVK(const BaseRenderDataVK* p_render_data)
    : render_data(p_render_data), nodes(p_render_data->allocator), exec_edges(p_render_data->allocator),
      res_edges(p_render_data->allocator), all_resources(p_render_data->allocator), command_buffers(p_render_data->allocator),
      command_buffer_pools(p_render_data->allocator), terminal_semaphores(p_render_data->allocator) {
}

RenderGraphVK::~RenderGraphVK() {
    free_command_buffers();
}

Ref<RGNode> RenderGraphVK::add_node(Ref<RenderTaskVK> p_task) {
    auto node = make_ref<RGNode>(render_data->allocator, render_data->allocator);
    node->render_task = p_task;
    node->in_queue = static_cast<uint32_t>(p_task->queue_family());
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
    // Timeline semaphore for cross-submit ordering.
    edge->semaphore = make_ref<SemaphoreVK>(render_data->allocator, render_data, false);
    p_before->exec_after.emplace_back(edge);
    p_after->exec_before.emplace_back(edge);
    exec_edges.emplace_back(edge);
    return edge;
}

Ref<RGResEdge> RenderGraphVK::add_res_edge(Ref<RendResVK> p_resource,
    Ref<RGNode> p_before,
    Ref<RGNode> p_after,
    VkAccessFlags2 p_before_access,
    VkAccessFlags2 p_after_access,
    VkPipelineStageFlags2 p_before_stage,
    VkPipelineStageFlags2 p_after_stage) {
    auto edge = make_ref<RGResEdge>(render_data->allocator);
    edge->resource = p_resource;
    edge->before = p_before;
    edge->after = p_after;
    edge->before_last_access = p_before_access;
    edge->after_first_access = p_after_access;
    edge->before_stage = p_before_stage;
    edge->after_stage = p_after_stage;
    p_before->res_after.emplace_back(edge);
    p_after->res_before.emplace_back(edge);
    res_edges.emplace_back(edge);
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
        auto queue_family = static_cast<QueueFamilyVK>(nodes[i]->in_queue);
        VkCommandPool command_pool = pool_for(render_data, queue_family);
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
    for (size_t i = 0; i < command_buffers.size(); ++i) {
        if (command_buffers[i] != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(render_data->logic_device, command_buffer_pools[i], 1, &command_buffers[i]);
            command_buffers[i] = VK_NULL_HANDLE;
        }
    }
    command_buffers.clear();
    command_buffer_pools.clear();
}

void RenderGraphVK::build_from_descriptor(const RendPipelineDescVK& p_desc) {
    free_command_buffers();
    nodes.clear();
    exec_edges.clear();
    res_edges.clear();
    all_resources.clear();

    auto resources_by_position = index_resources(p_desc);
    all_resources = resources_by_position;
    auto task_ids = instantiate_tasks(p_desc);
    add_exec_edges_from_prev(p_desc, task_ids);

    // Topo-sort once; resource-edge inference walks nodes in execution order.
    topological_sort();
    infer_res_edges(resources_by_position);

    allocate_command_buffers();

    // Allocate one terminal binary semaphore per frame-in-flight slot.
    const uint32_t frames = std::max<uint32_t>(render_data->frames_in_flight, 1U);
    terminal_semaphores.clear();
    terminal_semaphores.reserve(frames);
    for (uint32_t f = 0; f < frames; ++f) {
        terminal_semaphores.emplace_back(make_ref<SemaphoreVK>(render_data->allocator, render_data, true));
    }
}

void RenderGraphVK::set_resource(RendResID p_res_id, Ref<RenderObjectVK> p_resource, RendResTID p_res_type_id) {
    for (auto& resource_ref : all_resources) {
        if (resource_ref->res_id == p_res_id) {
            resource_ref->res = p_resource;
            resource_ref->res_type_id = p_res_type_id;
            return;
        }
    }
    auto resource_ref = make_ref<RendResVK>(render_data->allocator);
    resource_ref->res_id = p_res_id;
    resource_ref->res_type_id = p_res_type_id;
    resource_ref->res = p_resource;
    all_resources.emplace_back(resource_ref);
}

Vector<Ref<RendResVK>, RenderAllocator> RenderGraphVK::index_resources(const RendPipelineDescVK& p_desc) {
    Vector<Ref<RendResVK>, RenderAllocator> resources_by_position(render_data->allocator);
    resources_by_position.reserve(p_desc.resources.size());
    for (const auto& resource_desc : p_desc.resources) {
        auto resource_ref = make_ref<RendResVK>(render_data->allocator);
        resource_ref->res_id = resource_desc.res_id;
        resource_ref->res_type_id = resource_desc.res_type_id;
        resource_ref->res = resource_desc.res;
        resources_by_position.emplace_back(resource_ref);
    }
    return resources_by_position;
}

Vector<RendTaskID, RenderAllocator> RenderGraphVK::instantiate_tasks(const RendPipelineDescVK& p_desc) {
    Vector<RendTaskID, RenderAllocator> task_ids(render_data->allocator);
    task_ids.reserve(p_desc.tasks.size());
    for (const auto& task_desc : p_desc.tasks) {
        auto task = task_desc.create();
        if (task == MEM_NULL) {
            throw std::runtime_error("RenderGraphVK: task factory returned null.");
        }
        add_node(task);
        task_ids.emplace_back(task_desc.task_id);
    }
    return task_ids;
}

void RenderGraphVK::add_exec_edges_from_prev(const RendPipelineDescVK& p_desc, const Vector<RendTaskID, RenderAllocator>& p_task_ids) {
    auto node_by_task_id = [&](RendTaskID p_id) -> Ref<RGNode> {
        for (size_t i = 0; i < p_task_ids.size(); ++i) {
            if (p_task_ids[i] == p_id) {
                return nodes[i];
            }
        }
        return MEM_NULL;
    };
    for (size_t i = 0; i < p_desc.tasks.size(); ++i) {
        auto consumer = nodes[i];
        for (auto prev_id : p_desc.tasks[i].prev) {
            auto producer = node_by_task_id(prev_id);
            if (producer == MEM_NULL) {
                throw std::runtime_error("RenderGraphVK: unknown prev task id.");
            }
            add_exec_edge(producer, consumer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
        }
    }
}

void RenderGraphVK::infer_res_edges(const Vector<Ref<RendResVK>, RenderAllocator>& p_resources) {
    auto find_res = [&](RendResID p_id) -> Ref<RendResVK> {
        for (const auto& resource_ref : p_resources) {
            if (resource_ref->res_id == p_id) {
                return resource_ref;
            }
        }
        return MEM_NULL;
    };

    // last_writer[res_id] = (node, write access, write stage)
    struct WriteInfo {
        RendResID id;
        Ref<RGNode> node;
        VkAccessFlags2 access;
        VkPipelineStageFlags2 stage;
    };
    Vector<WriteInfo, RenderAllocator> last_writers(render_data->allocator);

    for (auto& node : nodes) {
        auto reads = node->render_task->read_resources();
        for (auto& read_access : reads) {
            // Find last writer.
            for (auto& last_writer : last_writers) {
                if (last_writer.id == read_access.res_id) {
                    auto resource = find_res(read_access.res_id);
                    if (resource == MEM_NULL) {
                        throw std::runtime_error("RenderGraphVK: read resource "
                                                 "not declared in pipeline.");
                    }
                    add_res_edge(resource,
                        last_writer.node,
                        node,
                        last_writer.access,
                        read_access.access,
                        last_writer.stage,
                        read_access.stage);
                    break;
                }
            }
        }
        auto writes = node->render_task->write_resources();
        for (auto& write_access : writes) {
            // Append/replace last writer.
            bool found = false;
            for (auto& write_info : last_writers) {
                if (write_info.id == write_access.res_id) {
                    write_info.node = node;
                    write_info.access = write_access.access;
                    write_info.stage = write_access.stage;
                    found = true;
                    break;
                }
            }
            if (!found) {
                last_writers.emplace_back(WriteInfo{
                    .id = write_access.res_id, .node = node, .access = write_access.access, .stage = write_access.stage});
            }
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

    for (size_t i = 0; i < nodes.size(); ++i) {
        auto node = nodes[i];
        auto queue_family = static_cast<QueueFamilyVK>(node->in_queue);
        const uint32_t frame_slot = render_data->current_frame_slot;
        const size_t command_buffer_index = static_cast<size_t>(frame_slot) * nodes.size() + i;
        VkCommandBuffer command_buffer = command_buffers[command_buffer_index];

        if (vkResetCommandBuffer(command_buffer, 0) != VK_SUCCESS) {
            throw std::runtime_error("RenderGraphVK: failed to reset command buffer.");
        }

        auto per_node_bound = build_per_node_bound(node);
        record_node(node, command_buffer, per_node_bound, p_delta);

        // Build wait/signal semaphore lists from exec edges + extras.
        Vector<VkSemaphoreSubmitInfo, RenderAllocator> wait_infos(render_data->allocator);
        Vector<VkSemaphoreSubmitInfo, RenderAllocator> signal_infos(render_data->allocator);
        collect_edge_semaphores(node, timeline_value, wait_infos, signal_infos);

        const bool is_first_graphics = (i == first_graphics_idx && queue_family == QueueFamilyVK::GRAPHICS);
        const bool is_last_graphics = (i == last_graphics_idx && queue_family == QueueFamilyVK::GRAPHICS);
        if (is_first_graphics) {
            for (const auto& wait_info : p_extra_waits_first_submit) {
                wait_infos.push_back(wait_info);
            }
        }
        if (is_last_graphics) {
            for (const auto& signal_info : p_extra_signals_last_submit) {
                signal_infos.push_back(signal_info);
            }
            // Also signal the per-frame terminal binary semaphore.
            signal_infos.push_back(
                terminal_semaphores[render_data->current_frame_slot]->get_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT));
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
        if (static_cast<QueueFamilyVK>(nodes[i]->in_queue) == QueueFamilyVK::GRAPHICS) {
            if (first_index == nodes.size()) {
                first_index = i;
            }
            last_index = i;
        }
    }
    return {first_index, last_index};
}

Vector<RendResVK, RenderAllocator> RenderGraphVK::build_per_node_bound(Ref<RGNode> p_node) const {
    // Bound resources are emitted in the order declared by the task's
    // read_resources() followed by write_resources().
    auto reads = p_node->render_task->read_resources();
    auto writes = p_node->render_task->write_resources();
    Vector<RendResVK, RenderAllocator> per_node_bound(render_data->allocator);
    per_node_bound.reserve(reads.size() + writes.size());

    auto find_resource_for_id = [&](RendResID p_id) -> Ref<RendResVK> {
        for (const auto& resource_ref : all_resources) {
            if (resource_ref->res_id == p_id) {
                return resource_ref;
            }
        }
        for (const auto& edge : res_edges) {
            if (edge->resource->res_id == p_id) {
                return edge->resource;
            }
        }
        return MEM_NULL;
    };
    auto append_acc = [&](const ResAccessVK& p_access) {
        auto resource_ref = find_resource_for_id(p_access.res_id);
        if (resource_ref != MEM_NULL) {
            per_node_bound.push_back(*resource_ref);
        } else {
            // Resource may exist without any edges (single-task pipelines).
            // The task author is responsible for ensuring res exists.
            RendResVK empty{};
            empty.res_id = p_access.res_id;
            per_node_bound.push_back(empty);
        }
    };
    for (auto& access : reads) {
        append_acc(access);
    }
    for (auto& access : writes) {
        append_acc(access);
    }
    return per_node_bound;
}

void RenderGraphVK::record_node(
    Ref<RGNode> p_node, VkCommandBuffer p_command_buffer, const Vector<RendResVK, RenderAllocator>& p_bound, double p_delta) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(p_command_buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("RenderGraphVK: failed to begin command buffer.");
    }

    // Emit memory barriers for incoming resource edges.
    Vector<VkMemoryBarrier2, RenderAllocator> mem_barriers(render_data->allocator);
    mem_barriers.reserve(p_node->res_before.size());
    for (auto& weak_edge : p_node->res_before) {
        auto edge = weak_edge.lock();
        if (edge == MEM_NULL) {
            continue;
        }
        VkMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        barrier.srcStageMask = edge->before_stage;
        barrier.srcAccessMask = edge->before_last_access;
        barrier.dstStageMask = edge->after_stage;
        barrier.dstAccessMask = edge->after_first_access;
        mem_barriers.push_back(barrier);
    }
    if (!mem_barriers.empty()) {
        VkDependencyInfo dependency_info{};
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.memoryBarrierCount = static_cast<uint32_t>(mem_barriers.size());
        dependency_info.pMemoryBarriers = mem_barriers.data();
        vkCmdPipelineBarrier2(p_command_buffer, &dependency_info);
    }

    p_node->render_task->record(p_command_buffer, p_bound, p_delta);

    if (vkEndCommandBuffer(p_command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("RenderGraphVK: failed to end command buffer.");
    }
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

    if (vkQueueSubmit2(queue_for(render_data, p_queue_family), 1, &submit_info, p_fence) != VK_SUCCESS) {
        throw std::runtime_error("RenderGraphVK: vkQueueSubmit2 failed.");
    }
}

} // namespace WhiteBirdEngine
