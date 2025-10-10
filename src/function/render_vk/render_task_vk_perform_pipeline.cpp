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
                                                                         Ref<ROGraphicsPipelineVK> p_graphics_pipeline,
                                                                         bool p_fenced)
    : RenderTaskVK(p_render_data), render_pass(p_render_pass), graphics_pipeline(p_graphics_pipeline), is_fenced(p_fenced) {
    init();
}

RenderTaskVKPerformGraphicsPipeline::~RenderTaskVKPerformGraphicsPipeline() {
    vkFreeCommandBuffers(render_data->logic_device, render_data->graphics_command_pool, 1, &command_buffer);
    if (is_fenced) {
        vkDestroyFence(render_data->logic_device, fence, render_data->allocation_callbacks);
    }
}

void RenderTaskVKPerformGraphicsPipeline::involke(double p_delta) {
    if (is_fenced) {
        vkWaitForFences(render_data->logic_device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkResetFences(render_data->logic_device, 1, &fence);
    }
    vkResetCommandBuffer(command_buffer, 0);
    record_command_buffer();

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // For graphics pipeline, wait till the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage.
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &semaphore;
    submit_info.pWaitDstStageMask = wait_stages;
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
    
    if (is_fenced) {
        // Create fence.
        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // Set the initial state of the fence to "signaled", since we don't have a "previous frame" at the first frame.
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (vkCreateFence(render_data->logic_device, &fence_create_info, render_data->allocation_callbacks, &fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create in flight fence");
        }
    }
}

}

