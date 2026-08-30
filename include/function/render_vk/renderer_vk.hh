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
#ifndef WBE_FILE_RENDERER_VK_HH
#define WBE_FILE_RENDERER_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/job/job_buffer_ring_mpsc.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_graph_resource_binding.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_plugin_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_staging_buffer_vk.hh"
#include "function/render_vk/semaphore_vk.hh"
#include "function/render_vk/vma_vk.hh"
#include "function/window/i_window.hh"
#include "utils/defs.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

inline constexpr std::string_view REND_RES_SWAP_CHAIN_IMAGE = "swap_chain_image";
inline constexpr std::string_view REND_RES_STATIC_VERTICES = "static_vertices";
inline constexpr std::string_view REND_RES_STATIC_INDICES = "static_indices";
inline constexpr std::string_view REND_RES_STATIC_OBJECT_IDS = "static_object_ids";

/**
 * @class RendererVK
 * @brief Vulkan renderer.
 *
 */
class RendererVK final : public IRenderer, public ThisRef<RendererVK> {
public:
    using AllocType = RenderAllocator;

    /**
     * @brief The version of Vulkan will be used for the renderer.
     */
    static constexpr uint32_t VULKAN_API_VERSION = VK_API_VERSION_1_3;

    /**
     * @brief The number of jobs that would be runned per frame.
     */
    static constexpr size_t RUN_MAX_JOBS_PER_FRAME = 32;
    static constexpr size_t MAX_DELAYED_CALLS = 1024;

    virtual ~RendererVK() override;
    RendererVK(const RendererVK&) = delete;
    RendererVK(RendererVK&&) = delete;
    RendererVK& operator=(const RendererVK&) = delete;
    RendererVK& operator=(RendererVK&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator that the render resources will be
     * allocated on. This will also be the allocator for Vulkan.
     * @param p_window The window whose surface and swap chain are rendered.
     * @param p_job_buffer The render job buffer. Used for committing jobs for
     * the renderer to perform.
     * @param p_capabilities The requested capabilities. Unsupported requested capabilities are disabled during construction.
     */
    RendererVK(AllocType* p_allocator, Ref<IWindow> p_window, Ref<JobBufferRingMPSC> p_job_buffer, RendererCapabilities& p_capabilities);

    virtual uint32_t get_worker_nums() override {
        return 1;
    }

    virtual void tick(double p_delta) override;

    virtual void register_render_graph(std::string_view p_resource_key) override;

    virtual void unregister_render_graph(std::string_view p_resource_key) override;

    virtual void activate_render_graph(std::string_view p_resource_key) override;

    virtual void update_render_graph_resources(
        std::string_view p_resource_key, const RenderGraphResourceBinding* p_bindings, size_t p_binding_count) override;

    virtual Ref<RenderDatabase> get_render_database() override {
        return render_database;
    }

    virtual void update_static_opaque_triangles() override;

    /**
     * @brief Register one-shot Vulkan work to be recorded and submitted before graph execution.
     *
     * Registered jobs are batched into the renderer's reusable graphics command buffer. Captured resources stay alive until
     * the job is recorded, submitted, and waited by the renderer.
     *
     * @param p_job The GPU job to record.
     */
    void register_gpu_job(GPUJobVK p_job);

    virtual void add_job(std::function<void()> p_job) override {
        job_buffer->add_job(std::move(p_job));
    }

    /**
     * @brief Register a Vulkan render graph descriptor under a resource key.
     *
     * @param p_resource_key The resource key to register under.
     * @param p_desc The render graph descriptor.
     */
    void register_render_graph(std::string_view p_resource_key, RendGraphDescVK p_desc);

    /**
     * @brief Execute a render graph through this renderer's swap-chain path.
     *
     * This is used by render tasks that own their own graph instead of going
     * through the named pipeline registry.
     *
     * @param p_graph The graph to execute.
     * @param p_delta The frame delta forwarded to graph tasks.
     */
    void execute_graph(Ref<RenderGraphVK> p_graph, double p_delta);

    /**
     * @brief Get the singleton instance.
     *
     * @return The singleton instance.
     */
    static RendererVK* get_singleton() {
        return singleton;
    }

    /**
     * @brief Get the base render data of this renderer.
     *
     * @return The base render data of this renderer.
     */
    const BaseRenderDataVK* get_base_render_data() const {
        return &base_render_data;
    }

    /**
     * @brief Register a task to be called after a number of frames. This could
     * be used for resource deallocation to prevent destroying resources that
     * are currently in-flight. Note that when the renderer terminates, all
     * delayed calls will be called together at once.
     *
     * @param p_task The task to be called.
     * @param p_delayed_frames The number of frames to be delayed.
     */
    void register_delay_call(std::function<void()>&& p_task, uint32_t p_delayed_frames) {
        for (auto& delayed_call : delayed_calls) {
            if (delayed_call.second < 0) {
                delayed_call.first = std::move(p_task);
                delayed_call.second = static_cast<int32_t>(p_delayed_frames);
                return;
            }
        }
        throw std::runtime_error("Failed to register delay call: buffer overflow.");
    }

private:
    static constexpr size_t MAX_DESCRIPTOR_SET_COUNT = 1024;
    static constexpr size_t MAX_UNIFORM_DESCRIPTOR_COUNT = 16;
    static constexpr size_t MAX_SAMPLED_IMAGE_DESCRIPTOR_COUNT = 256;
    static constexpr size_t MAX_STORAGE_IMAGE_DESCRIPTOR_COUNT = 64;
    static constexpr size_t MAX_SAMPLER_DESCRIPTOR_COUNT = 256;
    static constexpr size_t MAX_COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT = 32;
    Ref<JobBufferRingMPSC> job_buffer;

    template <typename T>
    using Vector = ::WhiteBirdEngine::Vector<T, AllocType>;
    template <typename T>
    using Set = ::WhiteBirdEngine::Set<T, AllocType>;
    template <typename T>
    using HashSet = ::WhiteBirdEngine::HashSet<T, AllocType>;
    template <typename T>
    using Deque = ::WhiteBirdEngine::Deque<T, AllocType>;
    template <typename TKey, typename TVal>
    using Map = ::WhiteBirdEngine::Map<TKey, TVal, AllocType>;
    using String = ::WhiteBirdEngine::String<AllocType>;

    inline static RendererVK* singleton = nullptr;

    template <typename T>
    using STLAlloc = STLAllocator<T, AllocType, false, true, false>;

    AllocType* allocator = nullptr;
    Ref<RenderDatabase> render_database;

    Ref<IWindow> window;

    // Vulkan properties
    VkInstance vk_instance = VK_NULL_HANDLE;

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice logical_device = VK_NULL_HANDLE;
    VmaAllocator vma_allocator = VK_NULL_HANDLE;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
    uint32_t swap_chain_images_count = 0;
    uint32_t frames_in_flight = 0;

    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue compute_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;

    VkFormat swap_chain_image_format = VK_FORMAT_UNDEFINED;
    VkExtent2D swap_chain_extent{};

    VkViewport viewport{};
    VkRect2D scissor{};

    VkCommandPool graphics_command_pool = VK_NULL_HANDLE;
    VkCommandPool compute_command_pool = VK_NULL_HANDLE;

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    Vector<VkFence> in_flight_fences;
    std::array<std::pair<std::function<void()>, int32_t>, MAX_DELAYED_CALLS> delayed_calls;

    // Per-frame-in-flight binary semaphore signaled by vkAcquireNextImageKHR.
    Vector<Ref<SemaphoreVK>> image_available_semaphores;

    struct RegisteredRenderGraph {
        Ref<RendGraphDescVK> descriptor;
        Ref<RenderGraphVK> graph;
        uint32_t reference_count = 0;
    };

    // Registered compiled render graphs, keyed by resource key.
    Map<std::string, RegisteredRenderGraph> render_graphs;
    // The currently active cached render graph.
    Ref<RenderGraphVK> active_graph;

    std::vector<const char*> enabled_layers{
#ifdef WBE_VK_ENABLE_VALIDATION
        "VK_LAYER_KHRONOS_validation"
#endif
    };

    const std::vector<const char*> enabled_instance_extensions;

    std::vector<const char*> enabled_device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,

        // To make VK_IMAGE_LAYOUT_GENERAL works as well as other layouts.
        VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME,
    };

    struct QueueFamilyIndices {
        AllocType* allocator = nullptr;
        std::optional<uint32_t> graphics; // For basic pipeline rendering
        std::optional<uint32_t> compute;  // For compute shader
        std::optional<uint32_t> present;  // For presenting contents in window
        bool is_complete() const {
            return graphics.has_value() && compute.has_value() && present.has_value();
        }
        uint32_t get_graphics() const {
            return get_required_index(graphics, "graphics");
        }
        uint32_t get_compute() const {
            return get_required_index(compute, "compute");
        }
        uint32_t get_present() const {
            return get_required_index(present, "present");
        }
        Set<uint32_t> get_unique_indices() const {
            WBE_DEBUG_ASSERT(is_complete());
            Set<uint32_t> result(allocator);
            result.insert(get_graphics());
            result.insert(get_compute());
            result.insert(get_present());
            return result;
        }

    private:
        static uint32_t get_required_index(const std::optional<uint32_t>& p_index, const char* p_name) {
            if (!p_index.has_value()) {
                throw std::runtime_error(std::string("RendererVK: missing ") + p_name + " queue family index.");
            }
            return *p_index;
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        Vector<VkSurfaceFormatKHR> formats;
        Vector<VkPresentModeKHR> present_modes;
        bool is_adequate() const {
            return !formats.empty() && !present_modes.empty();
        }
    };

    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    struct TaskInfo {
        RendTaskID task_id;
        Ref<RenderTaskVK> render_task;
    };

    BaseRenderDataVK base_render_data;
    QueueFamilyIndices queue_family_indices;
    Vector<Ref<RenderPluginVK>> registered_plugins;

    // Render objects for the renderer.
    struct RenderObjects {
        RenderObjects(AllocType* p_allocator) : swap_chain_images(p_allocator), swap_chain_image_views(p_allocator) {
        }
        ~RenderObjects() = default;
        RenderObjects(const RenderObjects&) = delete;
        RenderObjects(RenderObjects&&) noexcept = delete;
        RenderObjects& operator=(const RenderObjects&) = delete;
        RenderObjects& operator=(RenderObjects&&) noexcept = delete;
        // Graphics pipeline layout.
        Ref<ROPipelineLayoutVK> graphics_pipeline_layout;
        // Swap chain images
        Vector<Ref<ROImageVK>> swap_chain_images;
        // Swap chain image views.
        Vector<Ref<ROImageViewVK>> swap_chain_image_views;
        // The staging buffer for data transfer.
        Ref<ROStagingBufferVK> staging_buffer;
        // Global static-opaque mesh GPU buffers, populated by
        // RendererVK::update_static_opaque_triangles() from the RenderDatabase
        // and exposed to render tasks via the global resource IDs
        // REND_RES_STATIC_{VERTICES,INDICES,OBJECT_IDS}.
        Ref<ROBufferVK> static_opaque_vertices;
        Ref<ROBufferVK> static_opaque_indices;
        Ref<ROBufferVK> static_opaque_obj_ids;
    };
    Unique<RenderObjects> render_objects;

    static constexpr size_t MAX_GPU_JOBS = 1024;
    VkCommandBuffer gpu_job_command_buffer = VK_NULL_HANDLE;
    std::array<GPUJobVK, MAX_GPU_JOBS> gpu_jobs;

    void init(RendererCapabilities& p_capabilities);
    void terminate();
    void register_requested_plugins(const RendererCapabilities& p_capabilities);

    void init_vulkan(RendererCapabilities& p_capabilities);
    void setup_base_render_data();
    void init_swap_chain_images();
    void init_swap_chain_image_views();
    void recreate_swap_chain();

    void create_instance();
    void create_device(RendererCapabilities& p_capabilities);
    void create_vma_allocator();
    void create_swap_chain();
    void clean_up_swap_chain();
    void create_command_pools();
    void create_gpu_job_command_buffer();
    void create_descriptor_pool();
    void create_async_objects();

    bool check_layer_support();
    void pick_physical_device(const Vector<VkPhysicalDevice>& p_physical_devices);
    void create_physical_device();
    void create_logical_device();

    bool is_device_suitable(VkPhysicalDevice p_device);

    QueueFamilyIndices get_queue_family_indices(VkPhysicalDevice p_device);
    SwapChainSupportDetails get_swap_chain_support_details(VkPhysicalDevice p_device);
    VkSurfaceFormatKHR choose_swap_surface_format(const Vector<VkSurfaceFormatKHR>& p_available_formats);
    VkPresentModeKHR choose_swap_present_mode(const Vector<VkPresentModeKHR>& p_available_present_modes);
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& p_capabilities);
    void setup_viewport_and_scissor_info(VkExtent2D p_swap_chain_extent);

    VkAllocationCallbacks* get_allocation_callbacks() {
        return nullptr;
    }

    void run_stored_jobs();
    std::optional<uint32_t> acquire_swapchain_next_image(VkSemaphore p_image_avialable_semaphore);
    std::optional<uint32_t> tick_setup_pipeline();
    void run_delayed_calls();
    void run_gpu_jobs();
    void synchronize_gpu_and_flush_delayed_calls();
    void submit_gpu_job_command_buffer();
    void set_graph_static_opaque_resources(Ref<RenderGraphVK> p_graph);
    void present(uint32_t p_image_index, Ref<RenderGraphVK> p_graph);
};

} // namespace WhiteBirdEngine

#endif
