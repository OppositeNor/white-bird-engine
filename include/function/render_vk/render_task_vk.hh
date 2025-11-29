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
#include "function/render_vk/semaphore_vk.hh"
#include "generated/label_manager.gen.hh"
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RenderTaskVK
 * @brief Vulkan render task.
 *
 */
struct RenderTaskVK : public RenderTask {
    RenderTaskVK(const BaseRenderDataVK* p_render_data, bool p_use_binary_semaphore = false)
        : req_rend_res(p_render_data->allocator), depend_semaphores(p_render_data->allocator),
        depend_semaphores_inst(p_render_data->allocator), depend_semaphore_timeline_wait_values(p_render_data->allocator),
        render_data(p_render_data) {}
    virtual ~RenderTaskVK() {}

    /**
     * @brief Setup the render task.
     */
    void setup() {
        WBE_DEBUG_ASSERT(task_semaphore != MEM_NULL);
        // update semaphore_timeline_wait_values size
        size_t timeline_semaphores = 0;
        for (auto& semaphore : depend_semaphores) {
            WBE_DEBUG_ASSERT(semaphore != MEM_NULL);
            ++timeline_semaphores;
        }
        depend_semaphore_timeline_wait_values.resize(timeline_semaphores);
        depend_semaphores_inst.resize(depend_semaphores.size());
        init();
    }

    /**
     * @brief The required resources. This will be filled in the order of the req_res field in RendTaskInfo.
     */
    vector<Ref<RendResVK>> req_rend_res;
    /**
     * @brief The semaphores this task depends on. This will be filled with unique semaphores of the dependent tasks.
     */
    vector<Ref<SemaphoreVK>> depend_semaphores;
    vector<VkSemaphore> depend_semaphores_inst;
    vector<uint64_t> depend_semaphore_timeline_wait_values;

    /**
     * @brief The depth of node of this task in the render graph.
     */
    uint64_t node_depth = 0;

    Ref<SemaphoreVK> task_semaphore;

protected:
    const BaseRenderDataVK* render_data;
    /**
     * @brief Check if the types of required resources matches the resources in req_rend_res.
     *
     * @param p_expected_req_res The expected resources types.
     */
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

    /**
     * @brief Get the dependent semaphore timeline semaphores' wait values.
     *
     * @return The dependent semaphores wait values.
     */
    const uint64_t* get_depend_semaphore_timeline_wait_values() {
        size_t n = depend_semaphore_timeline_wait_values.size();
        for (size_t i = 0; i < n; ++i) {
            depend_semaphore_timeline_wait_values[i] = render_data->current_frame;
        }
        return depend_semaphore_timeline_wait_values.data();
    }

    /**
     * @brief Get the dependent semaphores vulkan semaphore instances.
     *
     * @return The dependent semaphores vulkan semaphore instances.
     */
    const VkSemaphore* get_dependent_semaphores_instance() {
        size_t n = depend_semaphores_inst.size();
        for (size_t i = 0; i < n; ++i) {
            depend_semaphores_inst[i] = depend_semaphores[i]->get_semaphore();
        }
        return depend_semaphores_inst.data();
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
