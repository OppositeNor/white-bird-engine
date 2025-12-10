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
#ifndef __WBE_SEMAPHORE_VK_HH__
#define __WBE_SEMAPHORE_VK_HH__

#include "function/render_vk/render_vk_utils.hh"
#include <cstdint>
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
        : render_data(p_render_data), is_binary(p_binary) {
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

    /**
     * @brief Get the semaphore's submit info.
     *
     * @param p_pipeline_stage The stage that the semaphore is applied on.
     * @param p_value The value to wait/signal if this semaphore is a timeline semaphore. If not, this value is ignored.
     * @return The semaphore's create info.
     */
    VkSemaphoreSubmitInfo get_submit_info(VkPipelineStageFlags2 p_pipeline_stage, uint64_t p_value = 0) const {
        VkSemaphoreSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        submit_info.semaphore = semaphore;
        submit_info.stageMask = p_pipeline_stage;
        submit_info.value = p_value;
        return submit_info;
    }

private:
    VkSemaphore semaphore;
    const BaseRenderDataVK* render_data;
    bool is_binary;
};

}

#endif
