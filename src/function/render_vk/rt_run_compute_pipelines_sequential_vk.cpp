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
#include "function/render_vk/rt_run_compute_pipelines_sequential_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunComputePipelinesSequentialVK::RTRunComputePipelinesSequentialVK(const BaseRenderDataVK* p_render_data,
    std::span<const Dispatch> p_dispatches,
    QueueFamilyVK p_queue_family)
    : render_data(p_render_data), queue_family_id(p_queue_family), dispatches(p_render_data->allocator) {
    if (p_dispatches.empty()) {
        throw std::runtime_error("RTRunComputePipelinesSequentialVK: at least one compute pipeline is required.");
    }
    dispatches.assign(p_dispatches.begin(), p_dispatches.end());
    for (const Dispatch& dispatch : dispatches) {
        if (dispatch.pipeline == MEM_NULL) {
            throw std::runtime_error("RTRunComputePipelinesSequentialVK: compute pipeline is null.");
        }
    }
}

Vector<ResAccessVK, RenderAllocator> RTRunComputePipelinesSequentialVK::read_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

Vector<ResAccessVK, RenderAllocator> RTRunComputePipelinesSequentialVK::write_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

void RTRunComputePipelinesSequentialVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) {
    for (size_t i = 0; i < dispatches.size(); ++i) {
        Dispatch& dispatch = dispatches[i];
        vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.pipeline->get_pipeline());
        dispatch.pipeline->push_descriptor_indices(p_command_buffer);
        vkCmdDispatch(p_command_buffer, dispatch.group_count.width, dispatch.group_count.height, dispatch.group_count.depth);

        if (i + 1 >= dispatches.size()) {
            continue;
        }

        VkMemoryBarrier2 pass_barrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask =
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        };
        VkDependencyInfo dependency_info{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &pass_barrier,
        };
        vkCmdPipelineBarrier2(p_command_buffer, &dependency_info);
    }
}

} // namespace WhiteBirdEngine