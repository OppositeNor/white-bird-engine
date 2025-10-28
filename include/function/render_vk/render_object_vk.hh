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
#ifndef __WBE_RENDER_OBJECT_VK_HH__
#define __WBE_RENDER_OBJECT_VK_HH__

#include "function/render/render_object.hh"
#include "function/render_vk/render_vk_utils.hh"
#include <stdexcept>
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

protected:
    const BaseRenderDataVK* render_data;
private:
    void init();
};


/**
 * @class SemaphoreVK
 * @brief Vulkan semaphore container.
 *
 */
struct SemaphoreVK {
    VkSemaphore semaphore;
    const BaseRenderDataVK* render_data;
    SemaphoreVK(const BaseRenderDataVK* p_render_data)
        : render_data(p_render_data) {
        VkSemaphoreCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(render_data->logic_device, &create_info, render_data->allocation_callbacks, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphore.");
        }
    }
    ~SemaphoreVK() {
        vkDestroySemaphore(render_data->logic_device, semaphore, render_data->allocation_callbacks);
    }
};

/**
 * @class RendResVK
 * @brief A vulkan resource instance.
 *
 */
struct RendResVK {
    RendResID res_id;
    Ref<RenderObjectVK> res;
    SemaphoreVK available_semaphore;
    RendResVK(const BaseRenderDataVK* p_render_data)
        : available_semaphore(p_render_data) {}
    
};
}

#endif
