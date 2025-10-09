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
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_task_vk_perform_graphics_pipeline.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RenderTaskVKPerformGraphicsPipeline::RenderTaskVKPerformGraphicsPipeline(const BaseRenderDataVK* p_render_data,
                                                                         Ref<RORenderPassVK> p_render_pass,
                                                                         Ref<ROGraphicsPipelineVK> p_graphics_pipeline)
    : RenderTaskVK(p_render_data), render_pass(p_render_pass), graphics_pipeline(p_graphics_pipeline)  {
    init();
}

RenderTaskVKPerformGraphicsPipeline::~RenderTaskVKPerformGraphicsPipeline() {
    vkFreeCommandBuffers(render_data->logic_device, render_data->graphics_command_pool, 1, &command_buffer);
}

void RenderTaskVKPerformGraphicsPipeline::involke(double p_delta) {
    record_command_buffer();
}

void RenderTaskVKPerformGraphicsPipeline::record_command_buffer() {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;
    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer.");
    }
    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
}

void RenderTaskVKPerformGraphicsPipeline::init() {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    // Allocate on the graphics command pool.
    alloc_info.commandPool = render_data->graphics_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(render_data->logic_device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers.");
    }
}

}


