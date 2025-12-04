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
#ifndef __WBE_RENDERER_VK_HH__
#define __WBE_RENDERER_VK_HH__

#include "core/core_utils.hh"
#include "core/job/job_buffer_ring_spsc.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/render_task.hh"
#include "function/render/render_utils.hh"
#include "function/render/renderer.hh"
#include "function/render/render_database.hh"
#include "function/render_vk/allocator_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_render_pass_vk.hh"
#include "global/stl_allocator.hh"

#include <SDL.h>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

WBE_LABEL(REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK, WBE_RENDER_RESOURCE);
WBE_LABEL(REND_RES_BASE_GRAPHICS_PIPELINE_VK, WBE_RENDER_RESOURCE);
WBE_LABEL(REND_RES_STATIC_OPAQUE_TRIANGLES_VK, WBE_RENDER_RESOURCE);

WBE_LABEL(RT_PREPARE_SWAP_CHAIN_IMAGE_VK, WBE_RENDER_TASK);

/**
 * @class RendererVK
 * @brief Vulkan renderer.
 *
 */
class RendererVK final : public Renderer, public ThisRef<RendererVK> {
public:

    using AllocType = RenderAllocator;

    /**
     * @brief The version of Vulkan will be used for the renderer.
     */
    static constexpr uint32_t VULKAN_API_VERSION = VK_API_VERSION_1_2;

    /**
     * @brief The number of jobs that would be runned per frame.
     */
    static constexpr size_t RUN_MAX_JOBS_PER_FRAME = 10;

    virtual ~RendererVK() override;
    RendererVK(const RendererVK&) = delete;
    RendererVK(RendererVK&&) = delete;
    RendererVK& operator=(const RendererVK&) = delete;
    RendererVK& operator=(RendererVK&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator that the render resources will be allocated on. This will also be the allocator
     * for Vulkan.
     * @param p_job_buffer The render job buffer. Used for committing jobs for the renderer to perform.
     */
    RendererVK(AllocType* p_allocator, Ref<JobBufferRingSPSC<JobRender>> p_job_buffer);

    virtual void tick(double p_delta) override;

    virtual void register_pipeline(const std::string& p_pipeline_name, Ref<RendTaskInfo> p_root_task) override {
        pipelines.insert(std::pair(p_pipeline_name, compile_pipeline(p_pipeline_name, p_root_task)));
    }

    virtual void activate_pipeline(const std::string& p_pipeline_name) override {
        auto find_result = pipelines.find(p_pipeline_name);
        if (find_result == pipelines.end()) {
            throw std::runtime_error(std::format("Failed to activate pipeline: {}, pipeline not registered.", p_pipeline_name));
        }
        activated_pipeline = find_result->second;
    }

    virtual Ref<RenderDatabase<RenderAllocator>> get_render_database() override {
        return render_database;
    }

    virtual void update_static_opaque_triangles() override;

    virtual void pool_events(bool& p_should_close, Ref<InputManager> p_input_manager) override;

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

private:
    Ref<JobBufferRingSPSC<JobRender>> job_buffer;

    template <typename T>
    using vector = ::WhiteBirdEngine::vector<T, AllocType>;
    template <typename T>
    using set = ::WhiteBirdEngine::set<T, AllocType>;
    template <typename T>
    using hash_set = ::WhiteBirdEngine::hash_set<T, AllocType>;
    template <typename T>
    using deque = ::WhiteBirdEngine::deque<T, AllocType>;
    using string = ::WhiteBirdEngine::string<AllocType>;

    inline static RendererVK* singleton = nullptr;

    template <typename T>
    using STLAlloc = STLAllocator<T, AllocType, false, true, false>;


    AllocType* allocator;
    Ref<RenderDatabase<RenderAllocator>> render_database;
    Ref<AllocatorVK<AllocType>> allocator_vk;

    // Vulkan properties
    SDL_Window* window;
    VkInstance vk_instance;

    VkPhysicalDevice physical_device;
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
    VkDevice logical_device;

    VkSurfaceKHR surface;
    VkSwapchainKHR swap_chain;
    uint32_t swap_chain_images_count;
    uint32_t frames_in_flight;

    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue present_queue;

    vector<VkImage> swap_chain_images;
    VkFormat swap_chain_image_format;
    VkExtent2D swap_chain_extent;

    VkViewport viewport{};
    VkRect2D scissor{};

    VkCommandPool graphics_command_pool;
    VkCommandPool compute_command_pool;

    vector<VkFence> in_flight_fences;

    std::vector<const char*> validation_layers {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> enabled_device_extensions {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

#ifdef WBE_VK_ENABLE_VALIDATION
    constexpr static bool enable_validation_layer = true;
#else
    constexpr static bool enable_validation_layer = false;
#endif

    struct QueueFamilyIndices {
        AllocType* allocator;
        std::optional<uint32_t> graphics;   // For basic pipeline rendering
        std::optional<uint32_t> compute;    // For compute shader
        std::optional<uint32_t> present;    // For presenting contents in window
        bool is_complete() {
            return graphics.has_value()
                && compute.has_value()
                && present.has_value();
        }
        set<uint32_t> get_unique_indices() const {
            set<uint32_t> result(allocator);
            result.insert(graphics.value());
            result.insert(compute.value());
            result.insert(present.value());
            return result;
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        vector<VkSurfaceFormatKHR> formats;
        vector<VkPresentModeKHR> present_modes;
        bool is_adequate() {
            return !formats.empty() && !present_modes.empty();
        }
    };

    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    struct TaskInfo {
        RendTaskID task_id;
        Ref<RenderTaskVK> render_task;
    };

    struct PipelineInfo {
        string pipeline_name;
        vector<Ref<RendResVK>> rend_res;
        vector<TaskInfo> task_infos;
        Ref<SemaphoreVK> image_available_semaphore;
        vector<Ref<SemaphoreVK>> result_semaphores;
        uint64_t max_depth;
        PipelineInfo(AllocType* p_allocator)
            : pipeline_name(p_allocator), rend_res(p_allocator), task_infos(p_allocator),
            result_semaphores(p_allocator) {}
    };


    BaseRenderDataVK base_render_data;
    QueueFamilyIndices queue_family_indices;
    std::map<std::string, vector<Ref<PipelineInfo>>> pipelines;
    std::unordered_map<RendResID, RefWeak<RendResVK>> rend_res;
    std::map<RendResID, RefWeak<RendResVK>> render_resources;

    vector<Ref<PipelineInfo>> activated_pipeline;

    // Render objects for the renderer.
    struct RenderObjects {
        RenderObjects(AllocType* p_allocator)
            : swap_chain_image_views(p_allocator), swap_chain_framebuffers(p_allocator) {}
        // Render pass.
        Ref<RORenderPassVK> result_render_pass;
        // Graphics pipeline.
        Ref<ROGraphicsPipelineVK> graphics_pipeline;
        // Graphics pipeline layout.
        Ref<ROPipelineLayoutVK> graphics_pipeline_layout;
        // Swap chain image views.
        vector<Ref<ROImageViewVK>> swap_chain_image_views;
        // Framebuffers in the swap chain.
        vector<Ref<ROFramebufferVK>> swap_chain_framebuffers;
        // The swap chain image resource.
        Ref<RendResVK> swap_chain_framebuffer_resource;
        // The base graphics pipeline resource.
        Ref<RendResVK> base_graphics_pipeline_resource;
        // The buffer that stores data of the scenes' static opaque triangles' status.
        Ref<RendResVK> static_opaque_triangles_vertices_buffer_resource;
    };
    Unique<RenderObjects> render_objects;

    void init();
    void terminate();

    void init_window();
    void init_vulkan();
    void setup_base_render_data();
    void init_swap_chain_images();
    void init_swap_chain_image_views();
    void init_render_pass();
    void init_graphics_pipeline();
    void init_swap_chain_framebuffers();
    void setup_render_resource();

    void recreate_swap_chain();

    void create_instance();
    void create_window_surface();
    void create_device();
    void create_swap_chain();
    void clean_up_swap_chain();
    void create_command_pools();
    void create_async_objects();

    bool check_device_extension_support(VkPhysicalDevice p_device);
    bool check_validation_layer_support();
    void pick_physical_device(const vector<VkPhysicalDevice>& p_physical_devices);
    void create_physical_device();
    void create_logical_device();

    bool is_device_suitable(VkPhysicalDevice p_device);

    QueueFamilyIndices get_queue_family_indices(VkPhysicalDevice p_device);
    SwapChainSupportDetails get_swap_chain_support_details(VkPhysicalDevice p_device);
    VkSurfaceFormatKHR choose_swap_surface_format(const vector<VkSurfaceFormatKHR>& p_available_formats);
    VkPresentModeKHR choose_swap_present_mode(const vector<VkPresentModeKHR>& p_available_present_modes);
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& p_capabilities);

    VkAllocationCallbacks* get_allocation_callbacks() {
        return allocator_vk->get_allocation_callbacks();
    }

    vector<Ref<PipelineInfo>> compile_pipeline(const std::string& p_pipeline_name, Ref<RendTaskInfo> p_root_task);
    void compile_tasks(Ref<RendTaskInfo> p_root_task, Ref<PipelineInfo> p_pipeline_info, uint32_t p_index);
    void compile_render_graph(Ref<RendTaskInfo> p_root_task, Ref<PipelineInfo> p_pipeline_info,
                           std::map<RendTaskID, std::pair<Ref<RenderTaskVK>, std::vector<RendTaskID>*>>& p_render_task_map);
    Ref<RendResVK> get_rend_res(RendResID p_res, RendResTID p_type_id);
    void register_render_resources(Ref<RendTaskInfo> p_task, TaskInfo& p_task_info, Ref<PipelineInfo> p_pipeline);

    uint32_t find_memory_type(uint32_t p_type_filter, VkMemoryPropertyFlags p_properties) const;
};

}

#endif
