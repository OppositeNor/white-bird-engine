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

#include "core/allocator/allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "generated/label_manager.gen.hh"
#include <cstddef>
#include <format>
#include <initializer_list>
#include <stdexcept>
namespace WhiteBirdEngine {

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
 * @class RenderTaskVK
 * @brief Vulkan render task.
 *
 */
struct RenderTaskVK : public RenderTask {
    RenderTaskVK(const BaseRenderDataVK* p_render_data)
        : task_semaphore(p_render_data), render_data(p_render_data) {}
    virtual ~RenderTaskVK() {}
    /**
     * @brief The required resources. This will be filled in the order of the req_res field in RendTaskInfo.
     */
    std::vector<Ref<RendResVK>> req_rend_res;
    /**
     * @brief The tasks this task depends on.
     */
    std::vector<Ref<RenderTaskVK>> depend_tasks;

    SemaphoreVK task_semaphore;

protected:
    const BaseRenderDataVK* render_data;
    void check_req_res(std::initializer_list<RendResTID> p_expected_req_res) {
        if (req_rend_res.size() != p_expected_req_res.size()) {
            throw std::runtime_error(std::format(
                "Number of required resources doesn't match: expected: {}, actual: {}.", p_expected_req_res.size(), req_rend_res.size()));
        }
        for (size_t i = 0; i < p_expected_req_res.size(); ++i) {
            if (req_rend_res[i] == MEM_NULL) {
                throw std::runtime_error(std::format(
                    "Render task's required resource type mismatch: expected: {}, actual: NULL.",
                    LabelManager::get_singleton()->get_label_name(*(p_expected_req_res.begin() + i))));
            }
        }
    }
};

/**
 * @class RenderTaskFactoryVK
 * @brief Factory for RenderTaskVK.
 *
 */
struct RenderTaskFactoryVK : public RenderTaskFactory {
public:
    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     */
    RenderTaskFactoryVK(const BaseRenderDataVK* p_render_data)
        : render_data(p_render_data) {}
    virtual ~RenderTaskFactoryVK() {}

protected:
    const BaseRenderDataVK* render_data;
};

}

#endif
