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
#ifndef __WBE_RT_RUN_GRAPHICS_PIPELINE_VK_HH__
#define __WBE_RT_RUN_GRAPHICS_PIPELINE_VK_HH__

#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RTRunGraphicsPipelineVK
 * @brief Render task that runs a graphics pipeline and produces an image.
 *
 */
struct RTRunGraphicsPipelineVK final : public RenderTaskVK {
    /**
     * @brief Constructor.
     *
     * @param p_render_data The render data.
     * @param p_signal_in_flight Should signal in flight fence.
     */
    RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data, bool p_signal_in_flight = false);
    virtual ~RTRunGraphicsPipelineVK() override;

    virtual void init() override;

    virtual void perform(double p_delta) override;

private:
    VkCommandBuffer command_buffer;
    bool signal_in_flight = false;
    vector<VkPipelineStageFlags> wait_stages;
    void record_command_buffer();
};

/**
 * @class RTRunGraphicsPipelineFactoryVK
 * @brief Factory for RTRunGraphicsPipeline.
 *
 */
class RTRunGraphicsPipelineFactoryVK final : public RenderTaskFactoryVK {
public:
    /**
     * @brief Constructor.
     *
     * @param p_render_data The render data.
     * @param p_signal_in_flight Should signal in flight fence.
     */
    RTRunGraphicsPipelineFactoryVK(const BaseRenderDataVK* p_render_data, bool p_signal_in_flight = false)
        : RenderTaskFactoryVK(p_render_data), signal_in_flight(p_signal_in_flight) {}
    virtual ~RTRunGraphicsPipelineFactoryVK() override {}

    virtual Ref<RenderTask> create() const override {
        return make_ref<RTRunGraphicsPipelineVK>(render_data->allocator, render_data, signal_in_flight);      
    }
private:
    bool signal_in_flight;

};
}

#endif
