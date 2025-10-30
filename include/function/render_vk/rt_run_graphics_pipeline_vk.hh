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
#ifndef __WBE_RT_RUN_GRAPHICS_PIPELINE_HH__
#define __WBE_RT_RUN_GRAPHICS_PIPELINE_HH__

#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RTRunGraphicsPipeline
 * @brief Render task that runs a graphics pipeline and produces an image.
 *
 */
struct RTRunGraphicsPipeline final : public RenderTaskVK {
    RTRunGraphicsPipeline(const BaseRenderDataVK* p_render_data, Ref<ROGraphicsPipelineVK> p_graphics_pipeline, Ref<ROFramebufferVK> p_framebuffer,
                          VkFence p_wait_fence = VK_NULL_HANDLE);
    virtual ~RTRunGraphicsPipeline() override;

    virtual void perform(double p_delta) override;


private:
    Ref<ROGraphicsPipelineVK> graphics_pipeline;
    Ref<ROFramebufferVK> framebuffer;
    VkCommandBuffer command_buffer;
    VkFence wait_fence;
    void record_command_buffer();
};

}

#endif
