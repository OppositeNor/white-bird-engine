#ifndef __WBE_RENDERER_VK_HH__
#define __WBE_RENDERER_VK_HH__

#include "function/render/renderer.hh"
#include <SDL.h>
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

    SDL_Window* window;
};

}

#endif
