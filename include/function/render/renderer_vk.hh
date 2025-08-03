#ifndef __WBE_RENDERER_VK_H__
#define __WBE_RENDERER_VK_H__

#include "function/render/renderer.h"
// #include <vulkan/vulkan.h>

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
};

}

#endif
