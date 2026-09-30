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
#include "function/function_utils.hh"
#include "function/render/i_render_task.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render/render_task_resource_map.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/static_set_resources_vk.hh"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RendResPrimaryUsageVK
 * @brief Primary usage of a resource allocated by the render graph; it selects the Vulkan usage flags of the resource.
 */
enum class RendResPrimaryUsageVK : uint32_t {
    /** Image sampled by shaders; its content is written by transfers. Sampled and transfer-destination usage. */
    IMAGE_READ_ONLY,
    /** Image read and written by shaders. Storage and sampled usage. */
    IMAGE_READ_WRITE,
    /** Color attachment that is later sampled. Color-attachment and sampled usage. */
    IMAGE_RENDER_TARGET,
    /** Depth/stencil attachment that is later sampled. Depth-stencil-attachment and sampled usage. */
    IMAGE_DEPTH_TARGET,
    /** Uniform buffer. */
    BUFFER_UNIFORM,
    /** Storage buffer read or written by shaders. */
    BUFFER_STORAGE,
    /** Indirect command buffer written by shaders. Indirect and storage usage. */
    BUFFER_INDIRECT,
    /** Vertex buffer. */
    BUFFER_VERTEX,
    /** Index buffer. */
    BUFFER_INDEX,
};

/**
 * @class RendBufferDescVK
 * @brief Configuration of a buffer allocated by the render graph during graph compilation.
 */
struct RendBufferDescVK {
    /** Primary usage; must be one of the BUFFER_* usages. */
    RendResPrimaryUsageVK usage = RendResPrimaryUsageVK::BUFFER_STORAGE;
    /** Vulkan usage flags added to the flags selected by the primary usage. */
    VkBufferUsageFlags additional_usage = 0;
    /** Buffer size in bytes. Zero selects the size of initial_data. */
    size_t size = 0;
    /** Memory domain where the buffer is allocated. */
    DeviceDomain domain = DeviceDomain::DEVICE;
    /** Whether one buffer is allocated per frame in flight instead of one for the whole graph. */
    bool per_frame = false;
    /** Bytes uploaded into every allocated buffer during graph compilation. */
    std::vector<uint8_t> initial_data;
};

/**
 * @class RendImageDescVK
 * @brief Configuration of a device-local image allocated by the render graph during graph compilation.
 */
struct RendImageDescVK {
    /** Primary usage; must be one of the IMAGE_* usages. */
    RendResPrimaryUsageVK usage = RendResPrimaryUsageVK::IMAGE_READ_WRITE;
    /** Vulkan usage flags added to the flags selected by the primary usage. */
    VkImageUsageFlags additional_usage = 0;
    /** Image format. */
    VkFormat format = VK_FORMAT_UNDEFINED;
    /** Image extent. A two-dimensional image uses a depth of one. */
    VkExtent3D extent{.width = 0, .height = 0, .depth = 1};
    /** Image dimensionality. */
    VkImageType image_type = VK_IMAGE_TYPE_2D;
    /** Number of mip levels. */
    uint32_t mip_levels = 1;
    /** Number of array layers. */
    uint32_t layers = 1;
    /** Whether one image is allocated per frame in flight instead of one for the whole graph. */
    bool per_frame = false;
};

/**
 * @class RendImageViewDescVK
 * @brief Configuration of an image view created by the render graph over a graph image resource.
 *
 * Unset optional fields are derived from the viewed image.
 */
struct RendImageViewDescVK {
    /** View type. Defaults to the image's default view type. */
    std::optional<VkImageViewType> view_type;
    /** View format. Defaults to the image format. */
    std::optional<VkFormat> format;
    /** Aspect mask. Defaults to depth for depth formats and color otherwise. */
    std::optional<VkImageAspectFlags> aspect_mask;
    /** First viewed mip level. */
    uint32_t base_mip_level = 0;
    /** Number of viewed mip levels; VK_REMAINING_MIP_LEVELS views every level from base_mip_level. */
    uint32_t mip_level_count = 1;
    /** First viewed array layer. */
    uint32_t base_array_layer = 0;
    /** Number of viewed array layers; VK_REMAINING_ARRAY_LAYERS views every layer from base_array_layer. */
    uint32_t layer_count = 1;
};

/**
 * @class RendSamplerDescVK
 * @brief Configuration of a sampler created by the render graph during graph compilation.
 */
struct RendSamplerDescVK {
    /** Minification filter. */
    VkFilter min_filter = VK_FILTER_LINEAR;
    /** Magnification filter. */
    VkFilter mag_filter = VK_FILTER_LINEAR;
    /** Mipmap sampling mode. */
    VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    /** Address mode for the U coordinate. */
    VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    /** Address mode for the V coordinate. */
    VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    /** Address mode for the W coordinate. */
    VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    /** Mipmap level-of-detail bias. */
    float mip_lod_bias = 0.0F;
    /** Maximum anisotropy; anisotropic filtering is disabled when unset. */
    std::optional<float> max_anisotropy;
    /** Depth-comparison operation; comparison sampling is disabled when unset. */
    std::optional<VkCompareOp> compare_op;
    /** Minimum mip level of detail. */
    float min_lod = 0.0F;
    /** Maximum mip level of detail. */
    float max_lod = VK_LOD_CLAMP_NONE;
    /** Border color used by clamp-to-border address modes. */
    VkBorderColor border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    /** Whether texture coordinates are unnormalized. */
    bool unnormalized_coordinates = false;
};

/**
 * @class RendBufferDeclVK
 * @brief A buffer resource declared on a graph descriptor.
 */
struct RendBufferDeclVK {
    /** Graph resource identifier. */
    std::string res_id;
    /** Buffer configuration. */
    RendBufferDescVK desc;
};

/**
 * @class RendImageDeclVK
 * @brief An image resource declared on a graph descriptor.
 */
struct RendImageDeclVK {
    /** Graph resource identifier. */
    std::string res_id;
    /** Image configuration. */
    RendImageDescVK desc;
};

/**
 * @class RendImageViewDeclVK
 * @brief An image view resource declared on a graph descriptor.
 */
struct RendImageViewDeclVK {
    /** Graph resource identifier of the view. */
    std::string res_id;
    /** Graph resource identifier of the viewed image. */
    std::string image_id;
    /** View configuration. */
    RendImageViewDescVK desc;
};

/**
 * @class RendSamplerDeclVK
 * @brief A sampler resource declared on a graph descriptor.
 */
struct RendSamplerDeclVK {
    /** Graph resource identifier. */
    std::string res_id;
    /** Sampler configuration. */
    RendSamplerDescVK desc;
};

/**
 * @class RendResEdgeDescVK
 * @brief A resource dependency declared from one task descriptor to a later task.
 */
struct RendResEdgeDescVK {
    /** Identifier of the task that uses the resource next. */
    RendTaskID next_task_id;
    /** Graph resource identifier carried by the edge. */
    std::string res_id;
};

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
     * @brief Connect this task to a later task through a resource dependency.
     *
     * The edge orders this task before the next task and synchronizes the resource between them: with a pipeline barrier
     * when both tasks run on the same queue, otherwise with a timeline semaphore. Aggregate resource-list ids apply to every
     * indexed entry. The graph derives the synchronization stages and accesses from the usages both tasks report for the
     * resource through IRenderTaskVK::append_accesses(); graph construction fails if either task does not access it.
     *
     * @param p_next_task_id Identifier of the task that uses the resource next.
     * @param p_resource_id Graph resource identifier carried by the edge.
     * @return This descriptor.
     */
    RendTaskDescVK& connect(std::string_view p_next_task_id, std::string_view p_resource_id);

    /**
     * @brief Map a resource identifier used by the task to a graph resource.
     *
     * The task binds and reports its accesses with task-local identifiers: reflected shader resource ids, render targets
     * (`render_target_colors[<index>]` and `render_target_depth`), and ids passed to its constructor. The graph resolves each
     * of them through this map; unmapped identifiers must name graph resources directly. Every alias must be used by the task.
     *
     * @param p_task_resource_id Identifier used by the task.
     * @param p_graph_resource_id Graph resource identifier it refers to.
     * @return This descriptor.
     * @throws std::runtime_error If an identifier is empty or p_task_resource_id is already mapped.
     */
    RendTaskDescVK& alias(std::string_view p_task_resource_id, std::string_view p_graph_resource_id);

    // The id of this task within the graph.
    RendTaskID task_id;
    // Resource dependencies from this task to later tasks.
    Vector<RendResEdgeDescVK, RenderAllocator> next;
    // Task-local to graph resource identifier map.
    RenderTaskResourceMap resource_map;
    // Factory invoked at graph-build time to construct the concrete task. The
    // resulting Unique<IRenderTaskVK> is owned by the graph.
    std::function<Unique<IRenderTaskVK>()> create;
};

/**
 * @class RendGraphDescVK
 * @brief A high-level Vulkan render pipeline descriptor.
 *
 * Every resource is declared once in this graph-wide descriptor. Buffers, images, image views, and samplers declared with
 * add_buffer(), add_image(), add_image_view(), and add_sampler() are allocated and owned by the graph when it is compiled;
 * resources owned elsewhere are imported with add_resource() or add_static_set(). Tasks refer to every resource by id
 * through their reflected shader, attachment, and fixed-function accesses, resolved through each task's resource map.
 */
struct RendGraphDescVK {
    explicit RendGraphDescVK(RenderAllocator* p_allocator)
        : allocator(p_allocator), tasks(p_allocator), resources(p_allocator), buffers(p_allocator), images(p_allocator),
          image_views(p_allocator), samplers(p_allocator) {
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
     * @brief Get a task descriptor that was already added, typically to connect it to later tasks.
     *
     * The reference is invalidated when another task is added.
     *
     * @param p_task_id The task id.
     * @return The stored task descriptor.
     * @throws std::runtime_error If no task has the id.
     */
    RendTaskDescVK& get_task(std::string_view p_task_id);

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

    /**
     * @brief Declare a buffer allocated by the graph during compilation.
     * @param p_resource_id Graph resource identifier.
     * @param p_desc Buffer configuration.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_buffer(std::string_view p_resource_id, RendBufferDescVK p_desc);

    /**
     * @brief Declare an image allocated by the graph during compilation.
     * @param p_resource_id Graph resource identifier.
     * @param p_desc Image configuration.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_image(std::string_view p_resource_id, RendImageDescVK p_desc);

    /**
     * @brief Declare an image view created by the graph during compilation.
     *
     * A view of a per-frame image is itself per-frame.
     *
     * @param p_resource_id Graph resource identifier of the view.
     * @param p_image_id Graph resource identifier of the viewed image, allocated by the graph or imported as an ROImageVK.
     * @param p_desc View configuration.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_image_view(std::string_view p_resource_id, std::string_view p_image_id, RendImageViewDescVK p_desc = {});

    /**
     * @brief Declare a sampler created by the graph during compilation.
     * @param p_resource_id Graph resource identifier.
     * @param p_desc Sampler configuration.
     * @return This graph descriptor.
     */
    RendGraphDescVK& add_sampler(std::string_view p_resource_id, const RendSamplerDescVK& p_desc = {});

    /**
     * @brief Import every resource of a compiled static set under the REND_RES_STATIC_SET_* ids.
     *
     * Optional categories that the static set does not contain are not declared.
     *
     * @param p_static_set Compiled static-set resources, retained by the graph descriptor.
     * @return This graph descriptor.
     * @throws std::runtime_error If p_static_set is null.
     */
    RendGraphDescVK& add_static_set(Ref<StaticSetResourcesVK> p_static_set);

    RenderAllocator* allocator = nullptr;
    // The set of tasks in the pipeline. Order is irrelevant; the graph topo-
    // sorts them based on their resource dependency edges.
    Vector<RendTaskDescVK, RenderAllocator> tasks;
    // Imported resources used by the pipeline. Looked up by res_id.
    Vector<RendResVK, RenderAllocator> resources;
    // Resources allocated and owned by the graph during compilation.
    Vector<RendBufferDeclVK, RenderAllocator> buffers;
    Vector<RendImageDeclVK, RenderAllocator> images;
    Vector<RendImageViewDeclVK, RenderAllocator> image_views;
    Vector<RendSamplerDeclVK, RenderAllocator> samplers;
};

} // namespace WhiteBirdEngine

#endif
