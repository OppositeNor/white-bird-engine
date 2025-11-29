#ifndef __WBE_SEMAPHORE_VK_HH__
#define __WBE_SEMAPHORE_VK_HH__

#include "function/render_vk/render_vk_utils.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
/**
 * @class SemaphoreVK
 * @brief Vulkan semaphore container.
 *
 */
class SemaphoreVK {
public:
    SemaphoreVK(const BaseRenderDataVK* p_render_data, bool p_binary = false)
        : render_data(p_render_data) {
        VkSemaphoreCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (!p_binary) {
            // Create timeline semaphore
            VkSemaphoreTypeCreateInfo type_create_info{};
            type_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
            type_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            type_create_info.initialValue = 0;
            create_info.pNext = &type_create_info;
        }
        if (vkCreateSemaphore(render_data->logic_device, &create_info, render_data->allocation_callbacks, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphore.");
        }
    }
    ~SemaphoreVK() {
        vkDestroySemaphore(render_data->logic_device, semaphore, render_data->allocation_callbacks);
    }

    /**
     * @brief Is this a binary semaphore.
     *
     * @return True if this is a binary semaphore, false if this is a binary semaphore.
     */
    bool binary() const {
        return is_binary;
    }

    /**
     * @brief Get semaphore.
     *
     * @return The vulkan semaphore instance.
     */
    VkSemaphore get_semaphore() const {
        return semaphore;
    }

private:
    VkSemaphore semaphore;
    const BaseRenderDataVK* render_data;
    bool is_binary;
};

}

#endif
