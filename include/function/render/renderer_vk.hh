// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_RENDERER_VK_HH__
#define __WBE_RENDERER_VK_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/allocator_vk.hh"
#include "function/render/renderer.hh"
#include "global/stl_allocator.hh"
#include "utils/defs.hh"

#include <SDL.h>
#include <optional>
#include <set>
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
    virtual ~RendererVK() override;
    RendererVK(const RendererVK&) = delete;
    RendererVK(RendererVK&&) = delete;
    RendererVK& operator=(const RendererVK&) = delete;
    RendererVK& operator=(RendererVK&&) = delete;

    RendererVK(Ref<AllocatorVK> p_allocator);
private:

    Ref<AllocatorVK> allocator;     // TODO: use allocator
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

    std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> enabled_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef WBE_VK_ENABLE_VALIDATION
    constexpr static bool enable_validation_layer = true;
#else
    constexpr static bool enable_validation_layer = false;
#endif

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics;   // For basic pipeline rendering
        std::optional<uint32_t> compute;    // For compute shader
        std::optional<uint32_t> present;    // For presenting contents in window
        bool is_complete() {
            return graphics.has_value()
                && compute.has_value()
                && present.has_value();
        }
        std::set<int> get_unique_indices() {
            std::set<int> result;
            result.insert(graphics.value());
            result.insert(compute.value());
            result.insert(present.value());
            return result;
        }
    } queue_family_indices;

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        vector<VkSurfaceFormatKHR, HeapAllocatorAlignedPool> formats;
        vector<VkPresentModeKHR, HeapAllocatorAlignedPool> present_modes;
        bool is_adequate() {
            return !formats.empty() && !present_modes.empty();
        }
    };

    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    void init();
    void terminate();

    void init_window();
    void init_vulkan();

    void create_instance();
    void create_window_surface();
    void create_device();
    void create_swap_chain();

    bool check_device_extension_support(VkPhysicalDevice p_device);
    bool check_validation_layer_support();
    void pick_physical_device(const vector<VkPhysicalDevice, HeapAllocatorAlignedPool>& p_physical_devices);
    void create_physical_device();
    void create_logical_device();

    bool is_device_suitable(VkPhysicalDevice p_device);

    QueueFamilyIndices get_queue_family_indices(VkPhysicalDevice p_device);
    SwapChainSupportDetails get_swap_chain_support_details(VkPhysicalDevice p_device);
    VkSurfaceFormatKHR choose_swap_surface_format(const vector<VkSurfaceFormatKHR, HeapAllocatorAlignedPool>& p_available_formats);
    VkPresentModeKHR choose_swap_present_mode(const vector<VkPresentModeKHR, HeapAllocatorAlignedPool>& p_available_present_modes);
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& p_capabilities);

    VkAllocationCallbacks* get_allocation_callbacks() {
        return allocator->get_allocation_callbacks();
    }
};

}

#endif
