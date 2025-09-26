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

#include "core/allocator/allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_database.hh"
#include "function/render/render_pass.hh"
#include "function/render_vk/allocator_vk.hh"
#include "function/render/renderer.hh"
#include "global/stl_allocator.hh"
#include "utils/defs.hh"

#include <SDL.h>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RendererVK
 * @brief Vulkan renderer.
 *
 */
class RendererVK : public Renderer {
public:
    using AllocType = RenderAllocator;

    struct ImageGPU {
        VkImage image;
        bool is_null = true;
    };

    struct RenderDatabaseTypes {
        using AllocType = RendererVK::AllocType;
        using ImageTypeGPU = ImageGPU;
        WBE_REQUIRES_CONCEPT(RenderDatabaseTypeConcept, RenderDatabaseTypes);
    };

    struct BaseRenderDataVK {
        // The render allocator.
        AllocType* allocator;
        // The logic device.
        VkDevice logic_device;
        // The image format of the swap chain.
        VkFormat swap_chain_format;
        // The size of the swap chain.
        VkExtent2D swap_chain_extent;
        // Callback for allocator.
        VkAllocationCallbacks* allocation_callbacks;
        // Graphics queue family index.
        uint32_t queue_family_index_graphics;
        // Compute queue family index.
        uint32_t queue_family_index_compute;
        // The viewport for the final presentation.
        VkViewport viewport;
        // The scissor for the final presentation.
        VkRect2D scissor;
    };


    virtual ~RendererVK() override;
    RendererVK(const RendererVK&) = delete;
    RendererVK(RendererVK&&) = delete;
    RendererVK& operator=(const RendererVK&) = delete;
    RendererVK& operator=(RendererVK&&) = delete;

    RendererVK(AllocType* p_allocator);
    virtual Ref<RenderPass> get_render_pass(const std::string& p_pipeline_id) override {
        return MEM_NULL; // TODO
    }

    /**
     * @brief Get the singleton instance.
     *
     * @return The singleton instance.
     */
    static RendererVK* get_singleton() {
        return singleton;
    }

    Ref<RenderDatabase<RenderDatabaseTypes>> get_render_database() {
        return render_database;
    }

    const BaseRenderDataVK* get_base_render_data() const {
        return &base_render_data;
    }

protected:
    virtual void add_render_pass(Ref<RenderPass> p_render_pipeline) override;
private:

    inline static RendererVK* singleton = nullptr;

    template <typename T>
    using STLAlloc = STLAllocator<T, AllocType, false, true, false>;

    template <typename T>
    using vector = ::WhiteBirdEngine::vector<T, AllocType>;
    template <typename T>
    using set = ::WhiteBirdEngine::set<T, AllocType>;
    using string = ::WhiteBirdEngine::string<AllocType>;

    AllocType* allocator;
    Ref<RenderDatabase<RenderDatabaseTypes>> render_database;
    Ref<AllocatorVK<AllocType>> allocator_vk;

    // Vulkan properties
    SDL_Window* window;
    VkInstance vk_instance;

    VkPhysicalDevice physical_device;
    VkDevice logical_device;

    VkSurfaceKHR surface;
    VkSwapchainKHR swap_chain;

    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue present_queue;

    std::vector<VkImage> swap_chain_images;
    VkFormat swap_chain_image_format;
    VkExtent2D swap_chain_extent;

    std::vector<VkImageView> swap_chain_image_views;

    VkViewport viewport{};
    VkRect2D scissor{};

    VkRenderPass result_render_pass;

    std::vector<const char*> validation_layers {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> enabled_device_extensions {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
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
        set<int> get_unique_indices() const {
            set<int> result(allocator);
            result.insert(graphics.value());
            result.insert(compute.value());
            result.insert(present.value());
            return result;
        }
    } queue_family_indices;

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

    BaseRenderDataVK base_render_data;

    void init();
    void terminate();

    void init_window();
    void init_vulkan();
    void setup_base_render_data();

    void create_instance();
    void create_window_surface();
    void create_device();
    void create_swap_chain();
    void create_swap_chain_image_views();
    void create_render_pass();

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
};

}

#endif
