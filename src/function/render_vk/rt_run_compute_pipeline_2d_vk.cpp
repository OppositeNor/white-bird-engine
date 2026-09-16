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
#include "function/render_vk/rt_run_compute_pipeline_2d_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunComputePipeline2DVK::RTRunComputePipeline2DVK(const BaseRenderDataVK* p_render_data,
    Ref<ROComputePipelineVK> p_pipeline_x,
    Ref<ROComputePipelineVK> p_pipeline_y,
    VkExtent3D p_dispatch_group_count_x,
    VkExtent3D p_dispatch_group_count_y,
    QueueFamilyVK p_queue_family)
    : render_data(p_render_data), queue_family_id(p_queue_family), pipeline_x(std::move(p_pipeline_x)),
      pipeline_y(std::move(p_pipeline_y)), dispatch_group_count_x(p_dispatch_group_count_x),
      dispatch_group_count_y(p_dispatch_group_count_y) {
    if (pipeline_x == MEM_NULL || pipeline_y == MEM_NULL) {
        throw std::runtime_error("RTRunComputePipeline2DVK: compute pipeline is null.");
    }
}

Vector<ResAccessVK, RenderAllocator> RTRunComputePipeline2DVK::read_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

Vector<ResAccessVK, RenderAllocator> RTRunComputePipeline2DVK::write_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

void RTRunComputePipeline2DVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) {
    vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_x->get_pipeline());
    pipeline_x->push_descriptor_indices(p_command_buffer);
    vkCmdDispatch(p_command_buffer, dispatch_group_count_x.width, dispatch_group_count_x.height, dispatch_group_count_x.depth);

    VkMemoryBarrier2 pass_barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
    };
    VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &pass_barrier,
    };
    vkCmdPipelineBarrier2(p_command_buffer, &dependency_info);

    vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_y->get_pipeline());
    pipeline_y->push_descriptor_indices(p_command_buffer);
    vkCmdDispatch(p_command_buffer, dispatch_group_count_y.width, dispatch_group_count_y.height, dispatch_group_count_y.depth);
}

} // namespace WhiteBirdEngine
