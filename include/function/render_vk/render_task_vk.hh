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
#ifndef __WBE_RENDER_TASK_VK_HH__
#define __WBE_RENDER_TASK_VK_HH__

#include "function/render/render_task.hh"
namespace WhiteBirdEngine {

/**
 * @class RenderTaskVK
 * @brief Vulkan render task.
 *
 */
struct RenderTaskVK : public RenderTask {
    RenderTaskVK() = default;
    virtual ~RenderTaskVK() {}
    /**
     * @brief The semaphores of the required resources. This will be filled in the order
     * of the req_res field.
     */
    std::vector<VkSemaphore> req_res_semaphores;
    /**
     * @brief The semaphores of the produced resources. This will be filled in the order
     * of the prod_res field.
     */
    std::vector<VkSemaphore> prod_res_semaphores;
};

}

#endif
