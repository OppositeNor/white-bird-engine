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
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/pipeline_descriptor_bindings_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunComputePipelinesSequentialVK::Dispatch::Dispatch(RenderAllocator* p_allocator) : entries(p_allocator) {
}

RTRunComputePipelinesSequentialVK::Dispatch& RTRunComputePipelinesSequentialVK::Dispatch::add_entry(Entry p_entry) {
    entries.push_back(std::move(p_entry));
    return *this;
}

RTRunComputePipelinesSequentialVK::Dispatch& RTRunComputePipelinesSequentialVK::Dispatch::add_entry(
    Ref<ROComputePipelineVK> p_pipeline, VkExtent3D p_group_count) {
    return add_entry(Entry{.pipeline = std::move(p_pipeline), .group_count = p_group_count});
}

RTRunComputePipelinesSequentialVK::RTRunComputePipelinesSequentialVK(
    const BaseRenderDataVK* p_render_data, const Dispatch& p_dispatch, QueueFamilyVK p_queue_family)
    : render_data(p_render_data), queue_family_id(p_queue_family), entries(p_render_data->allocator),
      descriptor_bindings(p_render_data->allocator) {
    if (p_dispatch.entries.empty()) {
        throw std::runtime_error("RTRunComputePipelinesSequentialVK: at least one compute pipeline is required.");
    }
    entries.assign(p_dispatch.entries.begin(), p_dispatch.entries.end());
    const PipelineResourceMapVK empty_resource_map(render_data->allocator);
    for (const Entry& entry : entries) {
        if (entry.pipeline == MEM_NULL) {
            throw std::runtime_error("RTRunComputePipelinesSequentialVK: compute pipeline is null.");
        }
        descriptor_bindings.push_back(make_unique<PipelineDescriptorBindingsVK>(render_data->allocator,
            render_data,
            entry.pipeline->get_descriptor_layout(),
            entry.resource_map ? *entry.resource_map : empty_resource_map));
    }
}

void RTRunComputePipelinesSequentialVK::bind_resources(const RendResViewVK& p_resources) {
    for (Unique<PipelineDescriptorBindingsVK>& bindings : descriptor_bindings) {
        bindings->bind_resources(p_resources);
    }
}

Vector<ResAccessVK, RenderAllocator> RTRunComputePipelinesSequentialVK::read_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

Vector<ResAccessVK, RenderAllocator> RTRunComputePipelinesSequentialVK::write_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

void RTRunComputePipelinesSequentialVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) {
    for (size_t i = 0; i < entries.size(); ++i) {
        Entry& entry = entries[i];
        vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, entry.pipeline->get_pipeline());
        descriptor_bindings[i]->push(p_command_buffer);
        vkCmdDispatch(p_command_buffer, entry.group_count.width, entry.group_count.height, entry.group_count.depth);

        if (i + 1 >= entries.size()) {
            continue;
        }

        VkMemoryBarrier2 pass_barrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
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
