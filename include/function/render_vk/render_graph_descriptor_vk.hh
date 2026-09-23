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
#ifndef WBE_FILE_RENDER_GRAPH_DESCRIPTOR_VK_HH
#define WBE_FILE_RENDER_GRAPH_DESCRIPTOR_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_task.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include <functional>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class RendTaskDescVK
 * @brief A high-level Vulkan render task descriptor. A single std::function
 * defers the concrete IRenderTaskVK instantiation until the graph is built.
 */
struct RendTaskDescVK {
    /** @brief Construct an empty task descriptor backed by the supplied allocator. */
    explicit RendTaskDescVK(RenderAllocator* p_allocator);
    /**
     * @brief Construct a descriptor that transfers one prebuilt task into the graph.
     *
     * The resulting factory is one-shot and throws if invoked more than once. Descriptor copies share that one-shot state.
     *
     * @param p_allocator Allocator used by the dependency list and direct-task holder.
     * @param p_task_id Task id used for graph dependencies.
     * @param p_task Task transferred to the graph when the descriptor is built.
     */
    RendTaskDescVK(RenderAllocator* p_allocator, std::string_view p_task_id, Unique<IRenderTaskVK> p_task);
    ~RendTaskDescVK() = default;
    RendTaskDescVK(const RendTaskDescVK&) = default;
    RendTaskDescVK(RendTaskDescVK&&) noexcept = default;
    RendTaskDescVK& operator=(const RendTaskDescVK&) = default;
    RendTaskDescVK& operator=(RendTaskDescVK&&) noexcept = default;

    /**
     * @brief Add a predecessor dependency.
     * @param p_task_id Identifier of the task that must finish first.
     * @return This descriptor.
     */
    RendTaskDescVK& after(std::string_view p_task_id);

    /**
     * @brief Declare an additional resource read performed through pipeline descriptors.
     * @param p_resource_id Graph resource identifier.
     * @param p_usage Compact task-facing usage.
     * @return This descriptor.
     */
    RendTaskDescVK& read(std::string_view p_resource_id, RendResUsageVK p_usage);

    /**
     * @brief Declare an additional resource write performed through pipeline descriptors.
     * @param p_resource_id Graph resource identifier.
     * @param p_usage Compact task-facing usage.
     * @return This descriptor.
     */
    RendTaskDescVK& write(std::string_view p_resource_id, RendResUsageVK p_usage);

    /**
     * @brief Add a task-private resource binding.
     * @param p_resource_id Logical identifier resolved before graph-global resources.
     * @param p_resource Task-private Vulkan render object.
     * @return This descriptor.
     */
    RendTaskDescVK& add_resource(std::string_view p_resource_id, Ref<IRenderObjectVK> p_resource);

    /**
     * @brief Add a task-private resource-container binding.
     * @param p_resource_id Logical identifier resolved before graph-global resources.
     * @param p_resource_container Stable task-private resource container.
     * @return This descriptor.
     */
    RendTaskDescVK& add_resource(std::string_view p_resource_id, Ref<RenderObjectContainerVK> p_resource_container);

    /**
     * @brief Add a task-private aggregate resource-list binding.
     * @param p_resource_id Logical aggregate identifier.
     * @param p_resource_list Resource list expanded into indexed task-private resources.
     * @return This descriptor.
     */
    RendTaskDescVK& add_resource(std::string_view p_resource_id, Ref<RenderResourceList> p_resource_list);

    // The id of this task within the graph.
    RendTaskID task_id;
    // The ids of tasks that must complete before this task starts. Used by the
    // graph to insert execution edges.
    Vector<RendTaskID, RenderAllocator> prev;
    // Additional descriptor-mediated resource reads and writes.
    Vector<ResAccessVK, RenderAllocator> reads;
    Vector<ResAccessVK, RenderAllocator> writes;
    // Resources scoped to this task id and preferred over graph-global resources.
    Vector<RendResVK, RenderAllocator> resources;
    // Factory invoked at graph-build time to construct the concrete task. The
    // resulting Unique<IRenderTaskVK> is owned by the graph.
    std::function<Unique<IRenderTaskVK>()> create;
};

/**
 * @class RendGraphDescVK
 * @brief A high-level Vulkan render pipeline descriptor. Resources are
 * concrete RendResVK (containing RenderObjectContainerVK), and tasks reference
 * each other by id.
 */
struct RendGraphDescVK {
    explicit RendGraphDescVK(RenderAllocator* p_allocator)
        : allocator(p_allocator), tasks(p_allocator), resources(p_allocator) {
    }

    /**
     * @brief Append a task descriptor to the graph.
     * @param p_task_desc The task descriptor to transfer into the graph.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_task(RendTaskDescVK p_task_desc);

    /**
     * @brief Append a prebuilt task to the graph.
     * @param p_task_id The task id used for graph dependencies.
     * @param p_task The concrete task transferred into the graph.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_task(std::string_view p_task_id, Unique<IRenderTaskVK> p_task);

    /**
     * @brief Append a resource binding to the graph.
     * @param p_resource The resource binding to transfer into the graph.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_resource(RendResVK p_resource);

    /**
     * @brief Append a resource binding to the graph.
     * @param p_resource The resource binding to transfer into the graph.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_resource(std::string_view p_resource_id, Ref<IRenderObjectVK> p_resource);

    /**
     * @brief Append a resource binding by id and concrete render object.
     * @param p_resource_id Graph resource identifier.
     * @param p_resource_type_id The type of the resource to be bound.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_resource(std::string_view p_resource_id, RendResTID p_resource_type_id);

    /**
     * @brief Append a resource binding by id and container.
     * @param p_resource_id Graph resource identifier.
     * @param p_resource_container The resource container bound to the id.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_resource(std::string_view p_resource_id, Ref<RenderObjectContainerVK> p_resource_container);

    /**
     * @brief Append an aggregate resource list binding.
     *
     * Graph construction registers the aggregate under p_resource_id and every entry under
     * `<p_resource_id>[<index>]`.
     *
     * @param p_resource_id Graph resource identifier for the aggregate.
     * @param p_resource_list Resource list to retain and expand.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_resource(std::string_view p_resource_id, Ref<RenderResourceList> p_resource_list);

    RenderAllocator* allocator = nullptr;
    // The set of tasks in the pipeline. Order is irrelevant; the graph topo-
    // sorts them based on `prev` edges.
    Vector<RendTaskDescVK, RenderAllocator> tasks;
    // The set of resources used by the pipeline. Looked up by res_id.
    Vector<RendResVK, RenderAllocator> resources;
};

} // namespace WhiteBirdEngine

#endif
