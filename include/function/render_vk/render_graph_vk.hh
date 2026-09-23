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
#include "function/render/i_render_task.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/semaphore_vk.hh"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
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
    std::string res_id;
    // The runtime type id of the underlying resource (TypeUUID<T>::UUID).
    RendResTID res_type_id = 0;
    // The resource container bound during graph construction.
    Ref<RenderObjectContainerVK> container;
    // Aggregate CPU-side resource list. Aggregate accesses expand to its indexed graph resources.
    Ref<RenderResourceList> resource_list;
};

/**
 * @class RendResViewVK
 * @brief Read-only lookup of stable resource bindings for a Vulkan render graph.
 */
class RendResViewVK {
public:
    RendResViewVK() = delete;

    /**
     * @brief Construct a view over a graph resource map.
     *
     * The map is non-owning and must outlive this view.
     *
     * @param p_resources The resource map to view.
     */
    RendResViewVK(const Map<std::string, Ref<RendResVK>, RenderAllocator>* p_resources, std::string_view p_task_id = {})
        : resources(p_resources), task_id(p_task_id) {
    }

    /**
     * @brief Check whether a stable resource binding is declared under an id.
     *
     * @param p_resource_id The resource id to query.
     * @return True when the resource binding is declared, otherwise false.
     */
    bool contains(std::string_view p_resource_id) const;

    /**
     * @brief Get a stable resource binding by id.
     *
     * @param p_resource_id The resource id to query.
     * The binding may not have a current render object until runtime resources are populated.
     *
     * @return The resource binding stored under the id.
     */
    const RendResVK& get(std::string_view p_resource_id) const;

    /**
     * @brief Return structured descriptor-debug rows for all declared graph resources.
     *
     * Each row is annotated with its graph resource id and deterministically sorted.
     *
     * @return Descriptor-debug rows for this resource view.
     */
    DebugDataRows descriptor_debug_data() const;

private:
    const Map<std::string, Ref<RendResVK>, RenderAllocator>* resources;
    std::string task_id;
};

/**
 * @class RendResRegistrarVK
 * @brief Registers task-private render resources into a graph-owned namespace.
 */
class RendResRegistrarVK {
public:
    RendResRegistrarVK() = delete;
    ~RendResRegistrarVK() = default;
    RendResRegistrarVK(const RendResRegistrarVK&) = delete;
    RendResRegistrarVK(RendResRegistrarVK&&) noexcept = delete;
    RendResRegistrarVK& operator=(const RendResRegistrarVK&) = delete;
    RendResRegistrarVK& operator=(RendResRegistrarVK&&) noexcept = delete;

    /**
     * @brief Register one task-private logical resource id.
     * @param p_resource_id Logical id used by shader reflection and task accesses.
     * @param p_resource_type_id Runtime type id of the contained render object.
     * @param p_container Stable container retained by the graph.
     */
    void add_resource(std::string_view p_resource_id, RendResTID p_resource_type_id, Ref<RenderObjectContainerVK> p_container);

    /**
     * @brief Register one task-private aggregate resource list and its indexed entries.
     * @param p_resource_id Logical aggregate id used by task accesses.
     * @param p_resource_list Resource list retained by the graph.
     */
    void add_resource(std::string_view p_resource_id, Ref<RenderResourceList> p_resource_list);

private:
    friend class RenderGraphVK;
    RendResRegistrarVK(Map<std::string, Ref<RendResVK>, RenderAllocator>* p_resources, RenderAllocator* p_allocator, std::string_view p_task_id);

    Map<std::string, Ref<RendResVK>, RenderAllocator>* resources;
    RenderAllocator* allocator;
    std::string task_id;
};

/**
 * @class RGNode
 * @brief A node of the render graph.
 */
struct RGNode {
    RGNode(RenderAllocator* p_allocator)
        : exec_before(p_allocator), exec_after(p_allocator), reads(p_allocator), writes(p_allocator) {
    }
    // The render task that should be performed during this node. Unique ownership.
    Unique<IRenderTaskVK> render_task;
    // String identifier copied from the graph descriptor for diagnostics.
    RendTaskID task_id;
    // The execution process edge before this node. All of these need
    // to be finished before executing this node.
    Vector<RefWeak<struct RGExecEdge>, RenderAllocator> exec_before;
    // The execution process edge after this node.
    Vector<RefWeak<struct RGExecEdge>, RenderAllocator> exec_after;
    // Compact resource declarations, including intrinsic task usage and descriptor additions.
    Vector<ResAccessVK, RenderAllocator> reads;
    Vector<ResAccessVK, RenderAllocator> writes;
    // The queue family where this node lives (graphics/compute/...).
    QueueFamilyVK in_queue = QueueFamilyVK::GRAPHICS;
    // Topological order for scheduling.
    uint64_t topo_order = 0;
};

/**
 * @class RGExecEdge
 * @brief An edge of execution flow between two graph nodes. A timeline
 * semaphore is allocated per edge: the producer signals it; the consumer
 * waits on it. When the edge directly orders a resource hazard, its wait and
 * signal stage scopes are tightened to the relevant producer and consumer
 * resource usage.
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
 * @class RenderGraphVK
 * @brief Vulkan-specific render graph. Manages per-task command buffers on
 * the appropriate queue family and uses explicit timeline-semaphore
 * execution edges to sequence submits and make memory visible. Declared
 * resource accesses are validated against those explicit edges during graph
 * build; conflicting accesses that are not ordered by an execution path make
 * graph construction fail.
 *
 * The graph is built once from a RendGraphDescVK; thereafter, execute()
 * may be called every frame.
 */
class RenderGraphVK {
public:
    RenderGraphVK() = delete;
    ~RenderGraphVK();
    RenderGraphVK(const RenderGraphVK&) = delete;
    RenderGraphVK(RenderGraphVK&&) noexcept = delete;
    RenderGraphVK& operator=(const RenderGraphVK&) = delete;
    RenderGraphVK& operator=(RenderGraphVK&&) noexcept = delete;

    /**
     * @brief Construct an empty graph bound to the given Vulkan render data.
     *
     * @param p_render_data The base render data.
     */
    RenderGraphVK(const BaseRenderDataVK* p_render_data);

    /**
     * @brief Build the graph from a high-level descriptor: instantiate every
     * task, add explicit execution edges based on `prev`, validate that those
     * edges order every conflicting declared resource access, and allocate the
     * command buffers. After this call, the graph is ready to execute().
     *
     * @param p_desc The render graph descriptor.
     */
    void build_from_descriptor(const RendGraphDescVK& p_desc);

    /**
     * @brief Execute every node in topological order, submitting each to the
     * command buffer of its queue family and signals/waits the timeline
     * semaphore on each execution edge. Resource hazards must already be
     * ordered by explicit execution paths established during graph build;
     * image layout transitions remain explicit.
     *
     * Wait/signal semaphores in p_extra_waits_first_submit and
     * p_extra_signals_last_submit are attached to the first and last
     * graphics-queue submits, respectively (used to interlock with swap-chain
     * acquire/present). Additionally the last graphics-queue submit signals
     * the graph's terminal binary semaphore for the acquired swap-chain image,
     * retrievable via get_terminal_semaphore().
     *
     * @param p_frame_index The current frame counter (timeline base value).
     * @param p_delta       Frame delta forwarded to IRenderTaskVK::record().
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
     * @brief The terminal binary semaphore: signaled by the last graphics-
     * queue submit of the most recent execute(). Caller waits on it at
     * present time. One semaphore is allocated per swap-chain image because
     * presentation may still own the previous render-finished semaphore until
     * that image is acquired again.
     *
     * @return The terminal semaphore for the current swap-chain image.
     */
    Ref<SemaphoreVK> get_terminal_semaphore() const;

    /**
     * @brief Return descriptors owned by declared graph resources as structured debug data.
     *
     * The returned object is a JSON-array-shaped debug value containing one row per descriptor.
     *
     * @return Structured descriptor debug data.
     */
    DebugData list_descriptors() const;

private:
    const BaseRenderDataVK* render_data;

    Vector<Ref<RGNode>, RenderAllocator> nodes;
    Vector<Ref<RGExecEdge>, RenderAllocator> exec_edges;
    // All resources known to the graph, keyed by res_id.
    Map<std::string, Ref<RendResVK>, RenderAllocator> all_resources;

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
    Ref<RGNode> add_node(Unique<IRenderTaskVK> p_task);
    Ref<RGExecEdge> add_exec_edge(
        Ref<RGNode> p_before, Ref<RGNode> p_after, VkPipelineStageFlags2 p_before_finish_stage, VkPipelineStageFlags2 p_after_start_stage);

    Map<std::string, Ref<RendResVK>, RenderAllocator> index_resources(const RendGraphDescVK& p_desc);
    Vector<RendTaskID, RenderAllocator> instantiate_tasks(const RendGraphDescVK& p_desc);
    void normalize_task_accesses(Ref<RGNode> p_node) const;
    void add_exec_edges_from_prev(const RendGraphDescVK& p_desc, const Vector<RendTaskID, RenderAllocator>& p_task_ids);
    void validate_explicit_resource_dependencies();
    bool has_exec_path(Ref<RGNode> p_from, Ref<RGNode> p_to) const;

    std::tuple<size_t, size_t> find_graphics_range() const;
    void record_node(Ref<RGNode> p_node, VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double p_delta, bool p_record_present_transition);
    void record_general_transition(VkCommandBuffer p_command_buffer, Ref<RGNode> p_node, const RendResViewVK& p_resources);
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
