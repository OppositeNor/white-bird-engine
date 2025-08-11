#ifndef __WBE_RENDERER_VK_HH__
#define __WBE_RENDERER_VK_HH__

#include "function/render/renderer.hh"
#include "utils/defs.hh"

#include <SDL.h>
#include <vector>
#include <vulkan/vulkan.h>

namespace WhiteBirdEngine {

/**
 * @class RendererVK
 * @brief Vulkan renderer.
 *
 */
class RendererVK : public Renderer {
public:
    RendererVK();
    virtual ~RendererVK() override;
    RendererVK(const RendererVK&) = delete;
    RendererVK(RendererVK&&) = delete;
    RendererVK& operator=(const RendererVK&) = delete;
    RendererVK& operator=(RendererVK&&) = delete;

private:
    void init();
    void terminate();

    void init_window();
    void init_vulkan();

    void create_instance();

    bool check_validation_layer_support();

    SDL_Window* window;
    VkInstance vk_instance;

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
};

}

#endif
