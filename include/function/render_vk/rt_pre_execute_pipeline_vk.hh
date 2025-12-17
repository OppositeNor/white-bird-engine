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
#ifndef __WBE_RT_PRE_EXECUTE_PIPELINE_VK_HH__
#define __WBE_RT_PRE_EXECUTE_PIPELINE_VK_HH__

#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "utils/defs.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RTPreExecutePipelineVK
 * @brief A task always inserted as the root node of the graphics pipeline by the renderer,
 * which will be pushed before all the tasks that the pipeline is going to execute.
 *
 */
struct RTPreExecutePipelineVK final: public RenderTaskVK {
    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     */
    RTPreExecutePipelineVK(const BaseRenderDataVK* p_render_data, Ref<SemaphoreVK> p_prepare_image_semaphore);
    virtual ~RTPreExecutePipelineVK() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(RTPreExecutePipelineVK)

    virtual void init() override {}
    virtual void perform(double p_delta) override;

private:
    Ref<SemaphoreVK> prepare_image_semaphore;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    void record_command_buffer();
    void submit_command_buffer();
};

/**
 * @class RTPreExecutePipelineFactoryVK
 * @brief Factory for RTPreExecutePipelineVK.
 *
 */
class RTPreExecutePipelineFactoryVK final: public RenderTaskFactoryVK {
public:
    /**
     * @brief Constructor.
     *
     * @param p_render_data The render data.
     * @param p_signal_in_flight Should signal in flight fence.
     */
    RTPreExecutePipelineFactoryVK(const BaseRenderDataVK* p_render_data, Ref<SemaphoreVK> p_prepare_image_semaphore)
        : RenderTaskFactoryVK(p_render_data), prepare_image_semaphore(p_prepare_image_semaphore) {}
    WBE_R6_NDC_DELETE_COPY_MOVE_OVERRIDE(RTPreExecutePipelineFactoryVK);

    virtual Ref<RenderTask> create() const override {
        return make_ref<RTPreExecutePipelineVK>(render_data->allocator, render_data, prepare_image_semaphore);
    }

private:
    Ref<SemaphoreVK> prepare_image_semaphore;
};
} // namespace WhiteBirdEngine

#endif
