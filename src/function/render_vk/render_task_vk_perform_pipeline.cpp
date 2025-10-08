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
#include "function/render_vk/render_task_vk_perform_graphics_pipeline.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RenderTaskVKPerformGraphicsPipeline::RenderTaskVKPerformGraphicsPipeline(const BaseRenderDataVK* p_render_data,
                                                                         Ref<ROGraphicsPipelineVK> p_graphics_pipeline)
    : graphics_pipeline(p_graphics_pipeline), render_data(p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Render data is nullptr.");
    }
}

RenderTaskVKPerformGraphicsPipeline::~RenderTaskVKPerformGraphicsPipeline() {
    vkFreeCommandBuffers(render_data->logic_device, render_data->graphics_command_pool, 1, &command_buffer);
}

void RenderTaskVKPerformGraphicsPipeline::involke(double p_delta) {
    // TODO
}

void RenderTaskVKPerformGraphicsPipeline::init(const BaseRenderDataVK* p_render_data) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    // Allocate on the graphics command pool.
    alloc_info.commandPool = p_render_data->graphics_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(p_render_data->logic_device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers.");
    }
}

}


