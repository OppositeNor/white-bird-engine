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

#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RenderTaskVK
 * @brief Vulkan render task.
 *
 */
class RenderTaskVK : public RenderTask {
public:
    RenderTaskVK() = default;
    virtual ~RenderTaskVK() {}

    /**
     * @brief Add a render task as dependency. The task only runs after all dependencies
     * finishes running.
     *
     * @param p_task 
     */
    void add_dependency(Ref<RenderTaskVK> p_task) {
        dependent_semaphores.push_back(p_task->get_semaphore());
        dependencies.push_back(p_task);
    }

    /**
     * @brief Get the semaphore for this task to finish.
     *
     * @return The semaphore for the task to finish.
     */
    VkSemaphore get_semaphore() const {
        return semaphore;
    }

protected:
    VkSemaphore semaphore;
    std::vector<VkSemaphore> dependent_semaphores;
    std::vector<Ref<RenderTaskVK>> dependencies;
};

}

#endif
