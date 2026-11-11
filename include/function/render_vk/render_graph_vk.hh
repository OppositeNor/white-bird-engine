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
#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/i_render_graph.hh"
#include "function/render/i_render_object.hh"
#include "function/render/i_render_task.hh"
#include "function/render/i_resource_description.hh"
#include "function/render/i_resource_registrator.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_graph_resource_registry.hh"
#include "function/render/render_task_resource_map.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/rd_params_compute_pipeline_vk.hh"
#include "function/render_vk/rd_params_graphics_pipeline_vk.hh"
#include "function/render_vk/rd_params_image_vk.hh"
#include "function/render_vk/rd_params_mesh_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_metadata_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_resource_list_vk.hh"
#include "function/render_vk/semaphore_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
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
class MeshResourcesVK;
class ROImageViewVK;
class ROGraphicsPipelineVK;
class ROComputePipelineVK;
struct RUMesh3D;
struct RUTexture;
struct RUGraphicsPipeline;
struct RUShader;
struct RendTaskDescVK;
struct RendSubgraphVK;
enum class RendResRebuildActionVK : uint8_t;
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

class IRenderGraphBindingsVK;
struct RUScript;

/**
 * @class RendGraphRecipeSourceVK
 * @brief What a render graph instance is compiled from, and compiled from again on every rebuild: a definition's recipe and
 * parameters (RURenderGraph), and the instance's bindings. Copyable and movable; copies share the recipe and the bindings.
 */
struct RendGraphRecipeSourceVK {
    /** The recipe script; the resource chunk that holds it must outlive the graph. */
    RefRes<RUScript> recipe;
    /** The recipe's static parameters, a map passed to `build(context, parameters)`. */
    DataValue parameters = DataValue::make_map();
    /** The bindings, which prepare the recipe's context and imports for each build; the graph retains them. */
    Ref<IRenderGraphBindingsVK> bindings;
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
    // The name diagnostics show for the task: its debug name, or its id when it has none.
    std::string name;
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
 * An instance is compiled from a recipe source (build_from_recipe(), which RendererVK::acquire_render_graph() calls): every
 * build runs the recipe and builds the RendGraphDescVK it describes; thereafter, execute() may be called every frame. The
 * graph-owned resources follow the RendGraphContextVK: execute() calls update_context() whenever the context of the render
 * data differs from the one the resources were built for, which compiles the source again. A frame whose context is
 * unchanged runs no script and prepares no bindings. build_from_descriptor() builds a graph from a fixed descriptor instead,
 * for focused tests of the compiler.
 *
 * As an IRenderGraph, the graph also owns the named resources its tasks use. It installs the default registrators RRMeshVK
 * (RDParamsMeshVK), RRImageVK (RDParamsImageVK), RRGraphicsPipelineVK (RDParamsGraphicsPipelineVK), and RRComputePipelineVK
 * (RDParamsComputePipelineVK); registered objects that are replaced or unregistered are released frames_in_flight frames
 * later.
 */
class WBE_META(WBE_RENDER_OBJECT) RenderGraphVK final : public IRenderGraph {
public:
    RenderGraphVK() = delete;
    virtual ~RenderGraphVK() override;
    RenderGraphVK(const RenderGraphVK&) = delete;
    RenderGraphVK(RenderGraphVK&&) noexcept = delete;
    RenderGraphVK& operator=(const RenderGraphVK&) = delete;
    RenderGraphVK& operator=(RenderGraphVK&&) noexcept = delete;

    /**
     * @brief Construct an empty graph bound to the given Vulkan render data.
     *
     * @param p_render_data The base render data.
     * @param p_resource_key The key the graph is registered under. When it is set and the render data has a
     * GlobalRenderCache, every resource the graph allocates is registered as `<p_resource_key>.<resource id>`, with
     * `[<slot>]` appended for each per-frame copy. Every allocated object is named `<p_resource_key>.<debug name>` (with the
     * same suffix), its resource's debug name (RendGraphDescVK::set_debug_name()) or else its id, and only by the debug name
     * when the key is empty.
     */
    explicit RenderGraphVK(const RenderContextVK* p_render_data, std::string_view p_resource_key = {});

    /**
     * @brief Compile the graph from a recipe source, and compile the source again on every rebuild.
     *
     * Each build prepares the bindings (IRenderGraphBindingsVK::prepare()) for the renderer facts of the current context,
     * runs the recipe's `build(context, parameters)`, and builds the descriptor the recipe describes with
     * RendGraphRecipeBuilderVK, bound to this graph. When the recipe declares the `REND_RES_SWAP_CHAIN` import and the
     * bindings do not supply it, the renderer's swap-chain container is supplied, acquired again for every build. The
     * descriptor is then built as build_from_descriptor() describes. Call on the rendering thread.
     *
     * @param p_source The source; its recipe and bindings must not be null.
     * @throws std::runtime_error If the source is incomplete, the bindings, the recipe, or the descriptor fail, or the
     * descriptor is invalid, cyclic, or leaves a resource conflict unordered.
     */
    void build_from_recipe(RendGraphRecipeSourceVK p_source);

    /**
     * @brief Build the graph from a fixed descriptor, which every rebuild builds again; for focused tests of the compiler.
     *
     * Takes ownership of every task, adds the resource dependency edges, rejects cycles, validates that every conflicting
     * declared resource access is ordered by edges on that resource, derives barriers and semaphores, and allocates the
     * command buffers. After this call, the graph is ready to execute(). The descriptor's resource and task factories are
     * evaluated against the context of each build. Each rebuild diffs the new descriptor against the current graph (see
     * RendGraphResourceDiffVK): a graph-owned resource whose id, kind, per-frame layout, and configuration are unchanged
     * keeps its container, objects, and content; one whose configuration changed is recreated in place in its container, so
     * RenderGraphVK::acquire_resource() callers observe it; one whose kind or per-frame layout changed, or that replaces an
     * import, gets a new container, which callers must acquire again. Views of recreated images are recreated. Resources
     * missing from the new descriptor are released, and every task is recreated.
     *
     * @param p_descriptor The descriptor; the graph keeps it for rebuilds.
     * @throws std::runtime_error If the descriptor is invalid, cyclic, or leaves a resource conflict unordered.
     */
    void build_from_descriptor(RendGraphDescVK p_descriptor);

    /**
     * @brief Rebuild the graph against a context.
     *
     * Compiles the graph's source for p_context and applies the result as build_from_descriptor() describes: unchanged
     * resources are kept, changed ones are recreated,
     * and reallocated resources lose their content. Then every task is recreated from its factory, and the dependency
     * edges, synchronization, and command buffers are rebuilt for the new tasks. Does nothing when p_context equals the
     * current context. Must be called on the rendering thread while none of the graph's submitted work is pending;
     * execute() calls it before recording when the context changes.
     *
     * @param p_context The context to evaluate the factories against.
     * @throws std::runtime_error If a re-evaluated resource configuration is invalid, or if the recreated tasks do not form
     * a valid graph.
     */
    void update_context(const RendGraphContextVK& p_context);

    /**
     * @brief Rebuild the graph against its current context.
     *
     * Does what update_context() does when the context changes, without a context change: the source is compiled
     * again, every graph-owned resource whose configuration now differs is reallocated, and every task is recreated. Use
     * it after state that the bindings or the factories read has changed structurally. Must be called on the rendering
     * thread while none of the graph's submitted work is pending.
     *
     * @throws std::runtime_error If a re-evaluated resource configuration is invalid, or if the recreated tasks do not form
     * a valid graph.
     */
    virtual void rebuild() override;

    virtual TypeID get_type_uuid() const override;

    /**
     * @brief Get the bindings the graph was compiled with.
     * @return The bindings of the recipe source, or null for a graph built from a fixed descriptor or not built yet.
     */
    Ref<IRenderGraphBindingsVK> get_bindings() const;

    /**
     * @brief Get the render data the graph was constructed for, which identifies its renderer.
     */
    const RenderContextVK* get_render_context() const {
        return render_data;
    }

    virtual void add_resource_registrator(Unique<IResourceRegistrator> p_registrator) override;
    virtual bool has_resource_registrator(TypeID p_description_type) const override;
    virtual Ref<IRenderObject> register_resource(std::string_view p_key, const IResourceDescription& p_description) override;
    virtual Ref<IRenderObject> register_resource(std::string_view p_key, TypeID p_description_type, const DataValue& p_description) override;
    virtual void unregister_resource(std::string_view p_key) override;
    virtual bool contains_registered_resource(std::string_view p_key) const override;
    virtual Ref<IRenderObject> try_acquire_registered_resource(std::string_view p_key) const override;

    /**
     * @brief Register a mesh with RRMeshVK; see IRenderGraph::register_resource().
     * @return The registered mesh.
     */
    Ref<MeshResourcesVK> register_mesh(std::string_view p_key, RDParamsMeshVK p_params);

    /**
     * @brief Register a texture with RRImageVK; see IRenderGraph::register_resource().
     * @return The registered view of the uploaded image.
     */
    Ref<ROImageViewVK> register_image(std::string_view p_key, RDParamsImageVK p_params);

    /**
     * @brief Register a graphics pipeline with RRGraphicsPipelineVK; see
     * IRenderGraph::register_resource().
     * @return The registered pipeline.
     */
    Ref<ROGraphicsPipelineVK> register_graphics_pipeline(std::string_view p_key, RDParamsGraphicsPipelineVK p_params);

    /**
     * @brief Register a compute pipeline with RRComputePipelineVK; see
     * IRenderGraph::register_resource().
     * @return The registered pipeline.
     */
    Ref<ROComputePipelineVK> register_compute_pipeline(std::string_view p_key, RDParamsComputePipelineVK p_params);

    /**
     * @brief Register a mesh the ResourceManager loaded; see register_mesh(std::string_view, RDParamsMeshVK).
     *
     * The description names the handle's resource key, so this registers exactly what the key-based call, or a recipe's
     * `graph.mesh(key, description)` handle, registers for the same resource.
     *
     * @param p_key The registration key.
     * @param p_mesh The mesh; its resource key replaces p_options.mesh.
     * @param p_options The compile options.
     * @return The registered mesh.
     * @throws std::runtime_error If p_mesh is null or has no resource key, or under the conditions of the key-based call.
     */
    Ref<MeshResourcesVK> register_mesh(std::string_view p_key, RefRes<RUMesh3D> p_mesh, RDParamsMeshVK p_options = {});

    /**
     * @brief Register a texture the ResourceManager loaded; see register_mesh(std::string_view, RefRes<RUMesh3D>,
     * RDParamsMeshVK).
     * @return The registered view of the uploaded image.
     */
    Ref<ROImageViewVK> register_image(std::string_view p_key, RefRes<RUTexture> p_texture);

    /**
     * @brief Register a graphics pipeline the ResourceManager loaded, compiled with p_options' attachments, topology, and
     * polygon mode; the handle's resource key replaces p_options.pipeline. See register_mesh(std::string_view,
     * RefRes<RUMesh3D>, RDParamsMeshVK).
     * @return The registered pipeline.
     */
    Ref<ROGraphicsPipelineVK> register_graphics_pipeline(
        std::string_view p_key, RefRes<RUGraphicsPipeline> p_pipeline, RDParamsGraphicsPipelineVK p_options);

    /**
     * @brief Register a compute pipeline from a shader the ResourceManager loaded; see register_mesh(std::string_view,
     * RefRes<RUMesh3D>, RDParamsMeshVK).
     * @return The registered pipeline.
     */
    Ref<ROComputePipelineVK> register_compute_pipeline(std::string_view p_key, RefRes<RUShader> p_shader);

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
     * Before the graph is built, the snapshot is empty. Must be called on the rendering thread.
     *
     * @return The tasks in execution order and the edges with the stages and accesses the graph derived for them.
     */
    RendGraphMetadataVK dump_graph_metadata() const;

    /**
     * @brief Acquire the container bound to an exported graph resource (RendGraphDescVK::add_export()).
     *
     * Resources allocated by the graph are held in an ROCSingleVK, or in an ROCMultislotsVK with one slot per frame in
     * flight when they were declared per frame. Use the container to update graph-owned buffers on the rendering thread.
     *
     * @param p_export_name The name the resource is exported under.
     * @return The resource container.
     * @throws std::runtime_error If nothing is exported under the name, or the exported resource is an aggregate resource
     * list.
     */
    Ref<RenderObjectContainerVK> acquire_resource(std::string_view p_export_name) const;

private:
    const RenderContextVK* render_data;
    // The key the graph is registered under; empty when its resources are not registered.
    std::string resource_key;
    // The named resources registered on the graph; see IRenderGraph.
    RenderGraphResourceRegistry registry;

    Vector<Ref<RGNode>, RenderAllocator> nodes;
    Vector<Ref<RGResEdge>, RenderAllocator> resource_edges;
    // All resources known to the graph, keyed by res_id.
    Map<std::string, Ref<RendResVK>, RenderAllocator> all_resources;
    // The context the graph-owned resources were last built for.
    RendGraphContextVK context;
    // The recipe source or fixed descriptor, the resource signatures and graph-owned containers of the current build, and
    // its task descriptors; defined in the source file, because it holds descriptor types.
    struct RebuildState;
    Unique<RebuildState> rebuild_state;

    // Per-node primary command buffer (parallel to `nodes`).
    Vector<VkCommandBuffer, RenderAllocator> command_buffers;
    // Per-node source command pool (parallel to `nodes`).
    Vector<VkCommandPool, RenderAllocator> command_buffer_pools;

    // Binary semaphores signaled by the last graphics submit; consumed by
    // present. One per swap-chain image.
    Vector<Ref<SemaphoreVK>, RenderAllocator> terminal_semaphores;

    // Release the current build and reset the context before a first build.
    void reset_build();
    // Build the descriptor of the stored source for the current context, then create the terminal semaphores.
    void finish_first_build();
    // The descriptor of the stored source (the recipe's, or a copy of the fixed descriptor) for the current context.
    RendGraphDescVK make_descriptor();
    void allocate_command_buffers();
    void free_command_buffers();
    uint32_t terminal_semaphore_index() const;
    void topological_sort();
    Ref<RGNode> add_node(Unique<IRenderTaskVK> p_task);
    Ref<RGNode> find_node(std::string_view p_task_id) const;
    void add_resource_edge(Ref<RGNode> p_from, Ref<RGNode> p_to, std::string p_resource_id);

    Map<std::string, Ref<RendResVK>, RenderAllocator> index_resources(const RendGraphDescVK& p_desc);
    void apply_descriptor(const RendGraphDescVK& p_desc);
    void apply_owned_resource(std::string_view p_resource_id, RendResRebuildActionVK p_action);
    Ref<IRenderObjectVK> create_owned_object(std::string_view p_resource_id, uint32_t p_slot) const;
    void bind_owned_resource(std::string_view p_resource_id, Ref<RenderObjectContainerVK> p_container);
    // The debug name of a graph-owned resource of the current build, or p_resource_id when it has none.
    std::string_view resource_name(std::string_view p_resource_id) const;
    // A resource as diagnostics name it: `'<name>' (<id>)`, or `'<id>'` when it has no debug name.
    std::string describe_resource(std::string_view p_resource_id) const;
    // Objects the container creates are registered as `<resource_key>.<p_resource_id>` and named by their debug name; see
    // RenderGraphVK().
    Ref<RenderObjectContainerVK> create_container(
        std::string_view p_resource_id, bool p_per_frame, RenderObjectContainerVK::Factory p_factory) const;
    void build_tasks(const Vector<RendTaskDescVK, RenderAllocator>& p_tasks, const Vector<RendSubgraphVK, RenderAllocator>& p_subgraphs);
    void instantiate_tasks(const Vector<RendTaskDescVK, RenderAllocator>& p_tasks);
    void normalize_task_accesses(Ref<RGNode> p_node) const;
    Vector<std::string, RenderAllocator> expand_resource_id(std::string_view p_resource_id) const;
    // The tasks with every edge to or from a subgraph replaced by edges between its members; see RendSubgraphVK.
    Vector<RendTaskDescVK, RenderAllocator> resolve_subgraph_edges(const Vector<RendTaskDescVK, RenderAllocator>& p_tasks,
        const Vector<RendSubgraphVK, RenderAllocator>& p_subgraphs) const;
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
