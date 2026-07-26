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
#ifndef WBE_FILE_RENDER_GRAPH_VK_HH
#define WBE_FILE_RENDER_GRAPH_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/semaphore_vk.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

struct RendGraphDescVK;

/**
 * @class RendResVK
 * @brief A Vulkan resource binding stored in the render graph.
 */
struct RendResVK {
    // The id used to look this resource up.
    RendResID res_id = 0;
    // The runtime type id of the underlying resource (TypeUUID<T>::UUID).
    RendResTID res_type_id = 0;
    // The resource instance.
    Ref<RenderObjectVK> res;
};

/**
 * @class RGNode
 * @brief A node of the render graph.
 */
struct RGNode {
    RGNode(RenderAllocator* p_allocator)
        : exec_before(p_allocator), exec_after(p_allocator), res_before(p_allocator), res_after(p_allocator) {
    }
    // The render task that should be performed during this node. Unique ownership.
    Unique<RenderTaskVK> render_task;
    // The execution process edge before this node. All of these need
    // to be finished before executing this node.
    Vector<RefWeak<struct RGExecEdge>, RenderAllocator> exec_before;
    // The execution process edge after this node.
    Vector<RefWeak<struct RGExecEdge>, RenderAllocator> exec_after;
    // The resource process edge before this node. All of these need
    // to be finished before executing this node.
    Vector<RefWeak<struct RGResEdge>, RenderAllocator> res_before;
    // The resource process edge after this node.
    Vector<RefWeak<struct RGResEdge>, RenderAllocator> res_after;
    // The queue family where this node lives (graphics/compute/...).
    uint32_t in_queue = 0;
    // Topological order for scheduling.
    uint64_t topo_order = 0;
};

/**
 * @class RGExecEdge
 * @brief An edge of execution flow between two graph nodes. A timeline
 * semaphore is allocated per edge: the producer signals it; the consumer
 * waits on it.
 */
struct RGExecEdge {
    // The node before this edge.
    Ref<RGNode> before;
    // The node after this edge.
    Ref<RGNode> after;
    // The pipeline stage at which the producer's work is considered
    // observable.
    VkPipelineStageFlags2 before_finish_pipeline_stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    // The pipeline stage at which the consumer's work needs the producer's
    // output to be visible.
    VkPipelineStageFlags2 after_start_pipeline_stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    // The timeline semaphore used to signal/wait this edge between submits.
    Ref<SemaphoreVK> semaphore;
};

/**
 * @class RGResEdge
 * @brief An edge of resource transformation between two graph nodes.
 */
struct RGResEdge {
    // The resource being transferred along this edge.
    Ref<RendResVK> resource;
    // The node before this edge (writer/last accessor).
    Ref<RGNode> before;
    // The node after this edge (next accessor).
    Ref<RGNode> after;
    // The last access mask applied by the producer.
    VkAccessFlags2 before_last_access = VK_ACCESS_2_NONE;
    // The first access mask applied by the consumer.
    VkAccessFlags2 after_first_access = VK_ACCESS_2_NONE;
    // The pipeline stage at which the producer last touched the resource.
    VkPipelineStageFlags2 before_stage = VK_PIPELINE_STAGE_2_NONE;
    // The pipeline stage at which the consumer first touches the resource.
    VkPipelineStageFlags2 after_stage = VK_PIPELINE_STAGE_2_NONE;
};

/**
 * @class RenderGraphVK
 * @brief Vulkan-specific render graph. Manages per-task command buffers on
 * the appropriate queue family, inserts pipeline barriers from declared
 * resource accesses, and uses one timeline semaphore per execution edge to
 * sequence submits.
 *
 * The graph is built once from a RendGraphDescVK; thereafter, execute()
 * may be called every frame.
 */
class RenderGraphVK {
public:
    RenderGraphVK() = delete;
    ~RenderGraphVK();
    WBE_R6_NDCD_DELETE_COPY_MOVE(RenderGraphVK)

    /**
     * @brief Construct an empty graph bound to the given Vulkan render data.
     *
     * @param p_render_data The base render data.
     */
    RenderGraphVK(const BaseRenderDataVK* p_render_data);

    /**
     * @brief Imperative builder: append a new node wrapping the given task.
     *
     * @param p_task The task to wrap.
     * @return A strong ref to the newly created node.
     */
    Ref<RGNode> add_node(Unique<RenderTaskVK> p_task);

    /**
     * @brief Imperative builder: connect two nodes with an execution edge.
     * Allocates a timeline semaphore for the edge.
     */
    Ref<RGExecEdge> add_exec_edge(
        Ref<RGNode> p_before, Ref<RGNode> p_after, VkPipelineStageFlags2 p_before_finish_stage, VkPipelineStageFlags2 p_after_start_stage);

    /**
     * @brief Imperative builder: connect two nodes with a resource edge.
     */
    Ref<RGResEdge> add_res_edge(Ref<RendResVK> p_resource,
        Ref<RGNode> p_before,
        Ref<RGNode> p_after,
        VkAccessFlags2 p_before_access,
        VkAccessFlags2 p_after_access,
        VkPipelineStageFlags2 p_before_stage,
        VkPipelineStageFlags2 p_after_stage);

    /**
     * @brief Build the graph from a high-level descriptor: instantiate every
     * task, infer resource edges from the union of read/write accesses, and
     * sequence them with execution edges based on `prev`. After this call,
     * the graph is ready to execute().
     *
     * @param p_desc The pipeline descriptor.
     */
    void build_from_descriptor(const RendGraphDescVK& p_desc);

    /**
     * @brief Execute every node in topological order, submitting each to the
     * command buffer of its queue family. Inserts pipeline barriers per
     * resource edge and signals/waits the timeline semaphore on each
     * execution edge.
     *
     * Wait/signal semaphores in p_extra_waits_first_submit and
     * p_extra_signals_last_submit are attached to the first and last
     * graphics-queue submits, respectively (used to interlock with swap-chain
     * acquire/present). Additionally the last graphics-queue submit signals
     * the graph's terminal binary semaphore for the acquired swap-chain image,
     * retrievable via get_terminal_semaphore().
     *
     * @param p_frame_index The current frame counter (timeline base value).
     * @param p_delta       Frame delta forwarded to RenderTaskVK::record().
     * @param p_extra_waits_first_submit  Extra waits on the first graphics
     * submit.
     * @param p_extra_signals_last_submit Extra signals on the last graphics
     * submit.
     */
    void execute(uint64_t p_frame_index,
        double p_delta,
        std::span<const VkSemaphoreSubmitInfo> p_extra_waits_first_submit,
        std::span<const VkSemaphoreSubmitInfo> p_extra_signals_last_submit);

    /**
     * @brief Insert or override a runtime-bound resource by id. This lets the
     * renderer inject per-frame resources (e.g., the current swap-chain image
     * view) whose identity is fixed by label but whose object changes
     * across frames. If a resource with the given id already exists, its
     * res / res_type_id are updated in place.
     *
     * @param p_res_id      The resource id (label).
     * @param p_resource    The resource instance.
     * @param p_res_type_id The runtime type id of the resource.
     */
    void set_resource(RendResID p_res_id, Ref<RenderObjectVK> p_resource, RendResTID p_res_type_id);

    /**
     * @brief The terminal binary semaphore: signaled by the last graphics-
     * queue submit of the most recent execute(). Caller waits on it at
     * present time. One semaphore is allocated per swap-chain image because
     * presentation may still own the previous render-finished semaphore until
     * that image is acquired again.
     */
    Ref<SemaphoreVK> get_terminal_semaphore() const;

private:
    const BaseRenderDataVK* render_data;

    Vector<Ref<RGNode>, RenderAllocator> nodes;
    Vector<Ref<RGExecEdge>, RenderAllocator> exec_edges;
    Vector<Ref<RGResEdge>, RenderAllocator> res_edges;
    // All resources known to the graph (declared in the pipeline descriptor
    // plus per-frame overrides set via set_resource()), keyed by res_id.
    Map<RendResID, Ref<RendResVK>, RenderAllocator> all_resources;

    // Per-node primary command buffer (parallel to `nodes`).
    Vector<VkCommandBuffer, RenderAllocator> command_buffers;
    // Per-node source command pool (parallel to `nodes`).
    Vector<VkCommandPool, RenderAllocator> command_buffer_pools;

    // Binary semaphores signaled by the last graphics submit; consumed by
    // present. One per swap-chain image.
    Vector<Ref<SemaphoreVK>, RenderAllocator> terminal_semaphores;

    void allocate_command_buffers();
    void free_command_buffers();
    uint32_t terminal_semaphore_index() const;
    void topological_sort();

    Map<RendResID, Ref<RendResVK>, RenderAllocator> index_resources(const RendGraphDescVK& p_desc);
    Vector<RendTaskID, RenderAllocator> instantiate_tasks(const RendGraphDescVK& p_desc);
    void add_exec_edges_from_prev(const RendGraphDescVK& p_desc, const Vector<RendTaskID, RenderAllocator>& p_task_ids);
    void infer_res_edges(const Map<RendResID, Ref<RendResVK>, RenderAllocator>& p_resources);

    std::tuple<size_t, size_t> find_graphics_range() const;
    Vector<RendResVK, RenderAllocator> build_per_node_bound(Ref<RGNode> p_node) const;
    void record_node(
        Ref<RGNode> p_node,
        VkCommandBuffer p_command_buffer,
        const Vector<RendResVK, RenderAllocator>& p_bound,
        double p_delta,
        bool p_record_pre_execute_tasks,
        bool p_record_present_transition);
    void record_general_transition(VkCommandBuffer p_command_buffer, Ref<RGNode> p_node, const Vector<RendResVK, RenderAllocator>& p_bound);
    void record_present_transition(VkCommandBuffer p_command_buffer);
    void collect_edge_semaphores(Ref<RGNode> p_node,
        uint64_t p_timeline_value,
        Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_waits,
        Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_signals) const;
    void submit_node(VkCommandBuffer p_command_buffer,
        QueueFamilyVK p_queue_family,
        const Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_waits,
        const Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_signals,
        VkFence p_fence);
};

} // namespace WhiteBirdEngine

#endif
