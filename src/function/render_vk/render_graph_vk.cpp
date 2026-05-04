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
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/semaphore_vk.hh"
#include "global/stl_allocator.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

VkQueue queue_for(const BaseRenderDataVK* p_render_data, QueueFamilyVK p_qf) {
    switch (p_qf) {
        case QueueFamilyVK::GRAPHICS: return p_render_data->graphics_queue;
        case QueueFamilyVK::COMPUTE:  return p_render_data->compute_queue;
    }
    return VK_NULL_HANDLE;
}

VkCommandPool pool_for(const BaseRenderDataVK* p_render_data, QueueFamilyVK p_qf) {
    switch (p_qf) {
        case QueueFamilyVK::GRAPHICS: return p_render_data->graphics_command_pool;
        case QueueFamilyVK::COMPUTE:  return p_render_data->compute_command_pool;
    }
    return VK_NULL_HANDLE;
}

} // namespace

RenderGraphVK::RenderGraphVK(const BaseRenderDataVK* p_render_data)
: render_data(p_render_data),
  nodes(p_render_data->allocator),
  exec_edges(p_render_data->allocator),
  res_edges(p_render_data->allocator),
  command_buffers(p_render_data->allocator),
  command_buffer_pools(p_render_data->allocator) {}

RenderGraphVK::~RenderGraphVK() {
    free_command_buffers();
}

Ref<RGNode> RenderGraphVK::add_node(Ref<RenderTaskVK> p_task) {
    auto node = make_ref<RGNode>(render_data->allocator, render_data->allocator);
    node->render_task = p_task;
    node->in_queue    = static_cast<uint32_t>(p_task->queue_family());
    nodes.emplace_back(node);
    return node;
}

Ref<RGExecEdge> RenderGraphVK::add_exec_edge(Ref<RGNode> p_before, Ref<RGNode> p_after,
                                             VkPipelineStageFlags2 p_before_finish_stage,
                                             VkPipelineStageFlags2 p_after_start_stage) {
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
                                           Ref<RGNode> p_before, Ref<RGNode> p_after,
                                           VkAccessFlags2 p_before_access, VkAccessFlags2 p_after_access,
                                           VkPipelineStageFlags2 p_before_stage, VkPipelineStageFlags2 p_after_stage) {
    auto edge = make_ref<RGResEdge>(render_data->allocator);
    edge->resource = p_resource;
    edge->before = p_before;
    edge->after  = p_after;
    edge->before_last_access = p_before_access;
    edge->after_first_access = p_after_access;
    edge->before_stage = p_before_stage;
    edge->after_stage = p_after_stage;
    p_before->res_after.emplace_back(edge);
    p_after->res_before.emplace_back(edge);
    res_edges.emplace_back(edge);
    return edge;
}

void RenderGraphVK::topological_sort() {
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
    Vector<size_t, RenderAllocator> node_index_map(nodes.size(), 0, render_data->allocator);
    for (size_t i = 0; i < nodes.size(); ++i) {
        // Initial map: position in nodes array for each node ref.
    }
    while (!ready.empty()) {
        size_t idx = ready.back();
        ready.pop_back();
        auto node = nodes[idx];
        node->topo_order = sorted.size();
        sorted.push_back(node);
        for (auto& edge_w : node->exec_after) {
            auto edge = edge_w.lock();
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
    command_buffers.assign(nodes.size(), VK_NULL_HANDLE);
    command_buffer_pools.assign(nodes.size(), VK_NULL_HANDLE);
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto qf = static_cast<QueueFamilyVK>(nodes[i]->in_queue);
        VkCommandPool pool = pool_for(render_data, qf);
        command_buffer_pools[i] = pool;
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(render_data->logic_device, &alloc_info, &command_buffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("RenderGraphVK: failed to allocate command buffer.");
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

    // Index resources by id.
    Vector<Ref<RendResVK>, RenderAllocator> resources_by_position(render_data->allocator);
    resources_by_position.reserve(p_desc.resources.size());
    for (const auto& r : p_desc.resources) {
        auto rr = make_ref<RendResVK>(render_data->allocator);
        rr->res_id = r.res_id;
        rr->res_type_id = r.res_type_id;
        rr->res = r.res;
        resources_by_position.emplace_back(rr);
    }
    auto find_res = [&](RendResID p_id) -> Ref<RendResVK> {
        for (auto& r : resources_by_position) {
            if (r->res_id == p_id) {
                return r;
            }
        }
        return MEM_NULL;
    };

    // Instantiate tasks; record id -> node.
    Vector<RendTaskID, RenderAllocator> task_ids(render_data->allocator);
    task_ids.reserve(p_desc.tasks.size());
    for (const auto& td : p_desc.tasks) {
        auto task = td.create();
        if (task == MEM_NULL) {
            throw std::runtime_error("RenderGraphVK: task factory returned null.");
        }
        add_node(task);
        task_ids.emplace_back(td.task_id);
    }
    auto node_by_task_id = [&](RendTaskID p_id) -> Ref<RGNode> {
        for (size_t i = 0; i < task_ids.size(); ++i) {
            if (task_ids[i] == p_id) {
                return nodes[i];
            }
        }
        return MEM_NULL;
    };

    // Add execution edges for `prev`.
    for (size_t i = 0; i < p_desc.tasks.size(); ++i) {
        auto consumer = nodes[i];
        for (auto prev_id : p_desc.tasks[i].prev) {
            auto producer = node_by_task_id(prev_id);
            if (producer == MEM_NULL) {
                throw std::runtime_error("RenderGraphVK: unknown prev task id.");
            }
            add_exec_edge(producer, consumer,
                          VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
        }
    }

    // Add resource edges by walking each task's read set against earlier writers.
    // Producer for each resource = most recent task in `prev` chain that wrote it.
    // We walk topo order after sorting.
    topological_sort();

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
        for (auto& r : reads) {
            // Find last writer.
            for (auto it = last_writers.rbegin(); it != last_writers.rend(); ++it) {
                if (it->id == r.res_id) {
                    auto resource = find_res(r.res_id);
                    if (resource == MEM_NULL) {
                        throw std::runtime_error("RenderGraphVK: read resource not declared in pipeline.");
                    }
                    add_res_edge(resource, it->node, node,
                                 it->access, r.access,
                                 it->stage,  r.stage);
                    break;
                }
            }
        }
        auto writes = node->render_task->write_resources();
        for (auto& w : writes) {
            // Append/replace last writer.
            bool found = false;
            for (auto& wi : last_writers) {
                if (wi.id == w.res_id) {
                    wi.node = node;
                    wi.access = w.access;
                    wi.stage = w.stage;
                    found = true;
                    break;
                }
            }
            if (!found) {
                last_writers.emplace_back(WriteInfo{.id = w.res_id, .node = node, .access = w.access, .stage = w.stage});
            }
        }
    }

    allocate_command_buffers();

    // Allocate the terminal binary semaphore once (re-used every frame).
    terminal_semaphore = make_ref<SemaphoreVK>(render_data->allocator, render_data, true);
}

void RenderGraphVK::execute(uint64_t p_frame_index, double p_delta,
                            std::span<const VkSemaphoreSubmitInfo> p_extra_waits_first_submit,
                            std::span<const VkSemaphoreSubmitInfo> p_extra_signals_last_submit) {
    if (nodes.empty()) {
        return;
    }
    const uint64_t timeline_value = p_frame_index + 1;

    // Build the bound resources vector once, indexed by resource position
    // within the descriptor (the order the task author chose). We pass the
    // full set; tasks index into it in their declared order.
    Vector<RendResVK, RenderAllocator> bound_resources(render_data->allocator);
    bound_resources.reserve(res_edges.size());
    // Construct a flat "all known resources" list from res_edges + task reads.
    // Simplest approach: snapshot from tasks' read sets & known res_edges.
    // Tasks rely on `record(cmd, bound, delta)` indexing by their declared
    // read order, so we instead build per-node bound vectors below.

    // Find first/last graphics submit indices.
    size_t first_graphics_idx = nodes.size();
    size_t last_graphics_idx  = nodes.size();
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (static_cast<QueueFamilyVK>(nodes[i]->in_queue) == QueueFamilyVK::GRAPHICS) {
            if (first_graphics_idx == nodes.size()) {
                first_graphics_idx = i;
            }
            last_graphics_idx = i;
        }
    }

    for (size_t i = 0; i < nodes.size(); ++i) {
        auto node = nodes[i];
        auto qf = static_cast<QueueFamilyVK>(node->in_queue);
        VkCommandBuffer cmd = command_buffers[i];

        if (vkResetCommandBuffer(cmd, 0) != VK_SUCCESS) {
            throw std::runtime_error("RenderGraphVK: failed to reset command buffer.");
        }

        // Build bound resources: in the order declared by the task's read_resources()
        // followed by write_resources().
        auto reads  = node->render_task->read_resources();
        auto writes = node->render_task->write_resources();
        Vector<RendResVK, RenderAllocator> per_node_bound(render_data->allocator);
        per_node_bound.reserve(reads.size() + writes.size());
        auto find_resource_for_id = [&](RendResID p_id) -> Ref<RendResVK> {
            for (auto& e : res_edges) {
                if (e->resource->res_id == p_id) {
                    return e->resource;
                }
            }
            return MEM_NULL;
        };
        auto append_acc = [&](const ResAccessVK& p_a) {
            auto r = find_resource_for_id(p_a.res_id);
            if (r != MEM_NULL) {
                per_node_bound.push_back(*r);
            } else {
                // Resource may exist without any edges (single-task pipelines).
                // The task author is responsible for ensuring res exists.
                RendResVK empty{};
                empty.res_id = p_a.res_id;
                per_node_bound.push_back(empty);
            }
        };
        for (auto& a : reads)  { append_acc(a); }
        for (auto& a : writes) { append_acc(a); }

        // Begin the command buffer.
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("RenderGraphVK: failed to begin command buffer.");
        }

        // Emit memory barriers for incoming resource edges.
        Vector<VkMemoryBarrier2, RenderAllocator> mem_barriers(render_data->allocator);
        mem_barriers.reserve(node->res_before.size());
        for (auto& w : node->res_before) {
            auto e = w.lock();
            if (e == MEM_NULL) {
                continue;
            }
            VkMemoryBarrier2 b{};
            b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            b.srcStageMask  = e->before_stage;
            b.srcAccessMask = e->before_last_access;
            b.dstStageMask  = e->after_stage;
            b.dstAccessMask = e->after_first_access;
            mem_barriers.push_back(b);
        }
        if (!mem_barriers.empty()) {
            VkDependencyInfo dep{};
            dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dep.memoryBarrierCount = static_cast<uint32_t>(mem_barriers.size());
            dep.pMemoryBarriers = mem_barriers.data();
            vkCmdPipelineBarrier2(cmd, &dep);
        }

        // Record the task.
        node->render_task->record(cmd, per_node_bound, p_delta);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            throw std::runtime_error("RenderGraphVK: failed to end command buffer.");
        }

        // Build wait/signal semaphore lists from exec edges + extras.
        Vector<VkSemaphoreSubmitInfo, RenderAllocator> wait_infos(render_data->allocator);
        Vector<VkSemaphoreSubmitInfo, RenderAllocator> signal_infos(render_data->allocator);

        for (auto& w : node->exec_before) {
            auto e = w.lock();
            if (e == MEM_NULL) {
                continue;
            }
            wait_infos.push_back(e->semaphore->get_submit_info(e->after_start_pipeline_stage, timeline_value));
        }
        for (auto& w : node->exec_after) {
            auto e = w.lock();
            if (e == MEM_NULL) {
                continue;
            }
            signal_infos.push_back(e->semaphore->get_submit_info(e->before_finish_pipeline_stage, timeline_value));
        }
        if (i == first_graphics_idx && qf == QueueFamilyVK::GRAPHICS) {
            for (const auto& w : p_extra_waits_first_submit) {
                wait_infos.push_back(w);
            }
        }
        if (i == last_graphics_idx && qf == QueueFamilyVK::GRAPHICS) {
            for (const auto& s : p_extra_signals_last_submit) {
                signal_infos.push_back(s);
            }
            // Also signal the terminal binary semaphore.
            signal_infos.push_back(terminal_semaphore->get_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT));
        }

        VkCommandBufferSubmitInfo cb_submit_info{};
        cb_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cb_submit_info.commandBuffer = cmd;

        VkSubmitInfo2 submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(wait_infos.size());
        submit_info.pWaitSemaphoreInfos    = wait_infos.data();
        submit_info.commandBufferInfoCount = 1;
        submit_info.pCommandBufferInfos    = &cb_submit_info;
        submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(signal_infos.size());
        submit_info.pSignalSemaphoreInfos    = signal_infos.data();

        // Last graphics submit signals the in-flight fence too, so the renderer
        // may wait for the entire frame's GPU work to complete next time around.
        VkFence fence = VK_NULL_HANDLE;
        if (i == last_graphics_idx && qf == QueueFamilyVK::GRAPHICS) {
            fence = render_data->in_flight_fence;
        }

        if (vkQueueSubmit2(queue_for(render_data, qf), 1, &submit_info, fence) != VK_SUCCESS) {
            throw std::runtime_error("RenderGraphVK: vkQueueSubmit2 failed.");
        }
    }

    // Suppress unused-variable warning for bound_resources scratch.
    (void)bound_resources;
}

} // namespace WhiteBirdEngine
