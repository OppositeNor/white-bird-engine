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
#include "function/render/render_task_resource_map.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_metadata_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_resource_list_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/semaphore_vk.hh"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

struct RendGraphDescVK;
struct RendTaskDescVK;
struct RendBufferDeclVK;
struct RendImageDeclVK;
struct RendImageViewDeclVK;
struct RendSamplerDeclVK;
struct RGNode;

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
    Ref<RenderResourceListVK> resource_list;
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
     * The maps are non-owning and must outlive this view.
     *
     * @param p_resources The resource map to view.
     * @param p_resource_map Optional task resource map; lookups resolve task-local identifiers through it first.
     */
    explicit RendResViewVK(const Map<std::string, Ref<RendResVK>, RenderAllocator>* p_resources,
        const RenderTaskResourceMap* p_resource_map = nullptr)
        : resources(p_resources), resource_map(p_resource_map) {
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
    const RenderTaskResourceMap* resource_map;

    std::string_view resolve(std::string_view p_resource_id) const;
};

/**
 * @class RGResEdge
 * @brief A resource dependency between two graph nodes.
 *
 * The edge orders `from` before `to` and makes the previous task's use of `res_id` visible to the next task. Edges whose
 * nodes run on the same Vulkan queue are recorded as pipeline barriers before the next task; edges crossing queues are
 * satisfied with timeline semaphores.
 */
struct RGResEdge {
    // The node that uses the resource first.
    Ref<RGNode> from;
    // The node that uses the resource next.
    Ref<RGNode> to;
    // The graph resource id carried by the edge. Aggregate resource-list ids are expanded to their indexed entries.
    std::string res_id;
    // The pipeline stages at which the previous task uses the resource, derived from its reported usages.
    VkPipelineStageFlags2 src_stage = VK_PIPELINE_STAGE_2_NONE;
    // The writes of the previous task that must be made available, derived from its reported write usages.
    VkAccessFlags2 src_access = VK_ACCESS_2_NONE;
    // The pipeline stages at which the next task uses the resource, derived from its reported usages.
    VkPipelineStageFlags2 dst_stage = VK_PIPELINE_STAGE_2_NONE;
    // The accesses of the next task that must see the previous task's writes, derived from its reported usages.
    VkAccessFlags2 dst_access = VK_ACCESS_2_NONE;
};

/**
 * @class RGBarrierVK
 * @brief A same-queue resource dependency recorded as a pipeline barrier before a node's commands.
 */
struct RGBarrierVK {
    // The graph resource id resolved to a buffer, image, or global memory barrier at record time.
    std::string res_id;
    // Source stages merged from all same-queue incoming edges on the resource.
    VkPipelineStageFlags2 src_stage = VK_PIPELINE_STAGE_2_NONE;
    // Source accesses made available by the barrier.
    VkAccessFlags2 src_access = VK_ACCESS_2_NONE;
    // Destination stages merged from all same-queue incoming edges on the resource.
    VkPipelineStageFlags2 dst_stage = VK_PIPELINE_STAGE_2_NONE;
    // Destination accesses made visible by the barrier.
    VkAccessFlags2 dst_access = VK_ACCESS_2_NONE;
};

/**
 * @class RGSemaphoreWaitVK
 * @brief A cross-queue dependency waited on when a node is submitted.
 */
struct RGSemaphoreWaitVK {
    // The producer node's timeline semaphore.
    Ref<SemaphoreVK> semaphore;
    // Destination stages merged from all incoming edges from the producer.
    VkPipelineStageFlags2 wait_stage = VK_PIPELINE_STAGE_2_NONE;
};

/**
 * @class RGNode
 * @brief A node of the render graph.
 */
struct RGNode {
    explicit RGNode(RenderAllocator* p_allocator)
        : out_edges(p_allocator), resource_map(p_allocator), reads(p_allocator), writes(p_allocator), barriers(p_allocator),
          semaphore_waits(p_allocator) {
    }
    // The render task that should be performed during this node. Unique ownership.
    Unique<IRenderTaskVK> render_task;
    // String identifier copied from the graph descriptor for diagnostics.
    RendTaskID task_id;
    // Resource edges whose previous task is this node.
    Vector<RefWeak<RGResEdge>, RenderAllocator> out_edges;
    // Task resource map copied from the descriptor; the graph resolves every task-local identifier through it.
    RenderTaskResourceMap resource_map;
    // Resource accesses reported by the task, resolved to graph resource identifiers.
    Vector<ResAccessVK, RenderAllocator> reads;
    Vector<ResAccessVK, RenderAllocator> writes;
    // Same-queue incoming dependencies, recorded as barriers before the task.
    Vector<RGBarrierVK, RenderAllocator> barriers;
    // Cross-queue incoming dependencies, waited on by this node's submission.
    Vector<RGSemaphoreWaitVK, RenderAllocator> semaphore_waits;
    // Timeline semaphore signaled by this node when it has cross-queue successors; otherwise null.
    Ref<SemaphoreVK> signal_semaphore;
    // Source stages merged from all cross-queue outgoing edges.
    VkPipelineStageFlags2 signal_stage = VK_PIPELINE_STAGE_2_NONE;
    // The queue family where this node lives (graphics/compute/...).
    QueueFamilyVK in_queue = QueueFamilyVK::GRAPHICS;
    // Topological order for scheduling.
    uint64_t topo_order = 0;
};

/**
 * @class RenderGraphVK
 * @brief Vulkan-specific render graph. Manages per-task command buffers on
 * the appropriate queue family. The graph is built from resource dependency
 * edges declared with RendTaskDescVK::connect(); it must be a DAG. Edges
 * between tasks on the same queue are recorded as pipeline barriers and edges
 * crossing queues use timeline semaphores. Every conflicting (read-write,
 * write-read, or write-write) pair of declared accesses on a resource must be
 * ordered by a chain of edges carrying that same resource; otherwise graph
 * construction fails.
 *
 * The graph is built once from a RendGraphDescVK; thereafter, execute()
 * may be called every frame. The graph-owned resources follow the
 * RendGraphContextVK: execute() calls update_context() whenever the context of
 * the render data differs from the one the resources were built for.
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
    explicit RenderGraphVK(const BaseRenderDataVK* p_render_data);

    /**
     * @brief Build the graph from a high-level descriptor: take ownership of
     * every task, add the resource dependency edges, reject cycles, validate that
     * every conflicting declared resource access is ordered by edges on that
     * resource, derive barriers and semaphores, and allocate the command
     * buffers. After this call, the graph is ready to execute().
     *
     * @throws std::runtime_error If the descriptor is invalid, cyclic, or
     * leaves a resource conflict unordered.
     *
     * @param p_desc The render graph descriptor. The graph keeps copies of its
     * resource and task factories to rebuild them when the context changes.
     */
    void build_from_descriptor(RendGraphDescVK& p_desc);

    /**
     * @brief Rebuild the graph against a context.
     *
     * Every graph-owned resource whose configuration differs from its previous evaluation is reallocated, in declaration
     * order, and every view of a reallocated image is recreated. New objects replace the old ones in place in their
     * containers, so RenderGraphVK::acquire_resource() callers observe them. Reallocated resources lose their content.
     * Then every task is recreated from its factory, and the dependency edges, synchronization, and command buffers are
     * rebuilt for the new tasks. Does nothing when p_context equals the current context. Must be called on the rendering
     * thread while none of the graph's submitted work is pending; execute() calls it before recording when the context
     * changes.
     *
     * @param p_context The context to evaluate the factories against.
     * @throws std::runtime_error If a re-evaluated resource configuration is invalid or changes whether a resource is per
     * frame, or if the recreated tasks do not form a valid graph.
     */
    void update_context(const RendGraphContextVK& p_context);

    /**
     * @brief Rebuild the graph against its current context.
     *
     * Does what update_context() does when the context changes, without a context change: every graph-owned resource
     * whose factory now yields a different configuration is reallocated, and every task is recreated. Use it after state
     * that resource or task factories read has changed. Must be called on the rendering thread while none of the graph's
     * submitted work is pending.
     *
     * @throws std::runtime_error If a re-evaluated resource configuration is invalid or changes whether a resource is per
     * frame, or if the recreated tasks do not form a valid graph.
     */
    void rebuild();

    /**
     * @brief Get the context the graph-owned resources were last built for.
     *
     * @return The current context.
     */
    const RendGraphContextVK& get_context() const {
        return context;
    }

    /**
     * @brief Execute every node in topological order, submitting each to the
     * queue of its queue family. Same-queue resource edges are recorded as
     * barriers at the start of the next task; cross-queue edges signal and
     * wait timeline semaphores.
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

    /**
     * @brief Take a snapshot of the compiled graph's tasks and resource dependency edges.
     *
     * Before build_from_descriptor(), the snapshot is empty. Must be called on the rendering thread.
     *
     * @return The tasks in execution order and the edges with the stages and accesses the graph derived for them.
     */
    RendGraphMetadataVK dump_graph_metadata() const;

    /**
     * @brief Acquire the container bound to a graph resource.
     *
     * Resources allocated by the graph are held in an ROCSingleVK, or in an ROCMultislotsVK with one slot per frame in
     * flight when they were declared per frame. Use the container to update graph-owned buffers on the rendering thread.
     *
     * @param p_resource_id The graph resource identifier.
     * @return The resource container.
     * @throws std::runtime_error If the resource is not declared or is an aggregate resource list.
     */
    Ref<RenderObjectContainerVK> acquire_resource(std::string_view p_resource_id) const;

private:
    const BaseRenderDataVK* render_data;

    Vector<Ref<RGNode>, RenderAllocator> nodes;
    Vector<Ref<RGResEdge>, RenderAllocator> resource_edges;
    // All resources known to the graph, keyed by res_id.
    Map<std::string, Ref<RendResVK>, RenderAllocator> all_resources;
    // The context the graph-owned resources were last built for.
    RendGraphContextVK context;
    // One per graph-owned resource, in declaration order: re-evaluates its factory against the current context and
    // recreates its container when its configuration, or the image it views, changed.
    Vector<std::function<void()>, RenderAllocator> resource_refreshers;
    // Recreates every task from the copied task descriptors of the graph descriptor, and rebuilds the edges,
    // synchronization, and command buffers for them.
    std::function<void()> task_builder;

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
    Ref<RGNode> find_node(std::string_view p_task_id) const;
    void add_resource_edge(Ref<RGNode> p_from, Ref<RGNode> p_to, std::string p_resource_id);

    Map<std::string, Ref<RendResVK>, RenderAllocator> index_resources(const RendGraphDescVK& p_desc);
    void allocate_resources(const RendGraphDescVK& p_desc);
    void validate_graph_resource_ids(const RendGraphDescVK& p_desc) const;
    void allocate_buffer(const RendBufferDeclVK& p_buffer_decl);
    void allocate_image(const RendImageDeclVK& p_image_decl);
    void allocate_image_view(const RendImageViewDeclVK& p_view_decl);
    void allocate_sampler(const RendSamplerDeclVK& p_sampler_decl);
    void register_resource(std::string_view p_resource_id, Ref<RenderObjectContainerVK> p_container);
    Ref<RenderObjectContainerVK> create_container(bool p_per_frame, RenderObjectContainerVK::Factory p_factory) const;
    void build_tasks(const Vector<RendTaskDescVK, RenderAllocator>& p_tasks);
    void instantiate_tasks(const Vector<RendTaskDescVK, RenderAllocator>& p_tasks);
    void normalize_task_accesses(Ref<RGNode> p_node) const;
    Vector<std::string, RenderAllocator> expand_resource_id(std::string_view p_resource_id) const;
    void add_resource_edges(const Vector<RendTaskDescVK, RenderAllocator>& p_tasks);
    void validate_resource_conflicts() const;
    void validate_queue_completion() const;
    bool has_resource_path(Ref<RGNode> p_from, Ref<RGNode> p_to, std::string_view p_resource_id) const;
    void build_synchronization();
    bool is_same_queue(QueueFamilyVK p_left, QueueFamilyVK p_right) const;

    std::tuple<size_t, size_t> find_graphics_range() const;
    void record_node(Ref<RGNode> p_node, VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double p_delta, bool p_record_present_transition);
    void record_general_transition(VkCommandBuffer p_command_buffer, Ref<RGNode> p_node, const RendResViewVK& p_resources);
    void record_edge_barriers(VkCommandBuffer p_command_buffer, Ref<RGNode> p_node, const RendResViewVK& p_resources) const;
    void record_present_transition(VkCommandBuffer p_command_buffer);
    void collect_node_semaphores(Ref<RGNode> p_node,
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
