#ifndef __WBE_RENDER_OBJECT_VK_HH__
#define __WBE_RENDER_OBJECT_VK_HH__

#include "function/render/render_object.hh"
#include "function/render_vk/render_vk_utils.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RenderObjectVK
 * @brief Vulkan render object.
 *
 */
class RenderObjectVK : public RenderObject {
public:
    RenderObjectVK(const BaseRenderDataVK* p_render_data);
    virtual ~RenderObjectVK() override;

    /**
     * @brief Get the semaphore that indicates the object is available.
     *
     * @return The semaphore that indicates the object is available.
     */
    VkSemaphore get_available_semaphore() const {
        return available_semaphore;
    }

protected:
    VkSemaphore available_semaphore;
    const BaseRenderDataVK* render_data;
private:
    void init();
};

}

#endif
