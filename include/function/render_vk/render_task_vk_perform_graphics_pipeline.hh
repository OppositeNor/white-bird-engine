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
#ifndef __WBE_RENDER_TASK_VK_PERFORM_GRAPHICS_PIPELINE_HH__
#define __WBE_RENDER_TASK_VK_PERFORM_GRAPHICS_PIPELINE_HH__

#include "function/render_vk/render_vk_utils.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "render_task_vk.hh"
#include <string>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RenderTaskVKPerformGraphicsPipeline
 * @brief Reder task that performs a graphics pipeline.
 *
 */
class RenderTaskVKPerformGraphicsPipeline final : public RenderTaskVK {
public:

    virtual ~RenderTaskVKPerformGraphicsPipeline() override;
    RenderTaskVKPerformGraphicsPipeline(const RenderTaskVKPerformGraphicsPipeline&) = delete;
    RenderTaskVKPerformGraphicsPipeline(RenderTaskVKPerformGraphicsPipeline&&) = delete;
    RenderTaskVKPerformGraphicsPipeline& operator=(const RenderTaskVKPerformGraphicsPipeline&) = delete;
    RenderTaskVKPerformGraphicsPipeline& operator=(RenderTaskVKPerformGraphicsPipeline&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_graphics_pipeline The graphics pipeline to perform.
     */
    RenderTaskVKPerformGraphicsPipeline(const BaseRenderDataVK* p_render_data, Ref<ROGraphicsPipelineVK> p_graphics_pipeline);

    /**
     * @brief Involke the render task.
     *
     * @param p_delta The difference of time in seconds from the last frame.
     */
    virtual void involke(double p_delta) override;

    /**
     * @brief Get the dependencies of resources.
     *
     * @return The ids of the resources.
     */
    virtual std::vector<std::string> get_resource_dependencies() const override {
        return std::vector<std::string>();
    }

private:
    void init();
    Ref<ROGraphicsPipelineVK> graphics_pipeline;
    VkCommandBuffer command_buffer;
};

}

#endif
