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
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
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
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunComputePipelineVK::RTRunComputePipelineVK(
    const BaseRenderDataVK* p_render_data, Ref<ROComputePipelineVK> p_pipeline, VkExtent3D p_dispatch_group_count, QueueFamilyVK p_queue_family)
    : render_data(p_render_data), queue_family_id(p_queue_family), pipeline(std::move(p_pipeline)),
      dispatch_group_count(p_dispatch_group_count) {
    if (pipeline == MEM_NULL) {
        throw std::runtime_error("RTRunComputePipelineVK: compute pipeline is null.");
    }
    descriptor_bindings =
        make_unique<PipelineDescriptorBindingsVK>(render_data->allocator, render_data, pipeline->get_descriptor_layout());
}

void RTRunComputePipelineVK::bind_resources(const RendResViewVK& p_resources) {
    descriptor_bindings->bind_resources(p_resources);
}

void RTRunComputePipelineVK::append_accesses(
    Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const {
    descriptor_bindings->append_accesses(p_reads, p_writes);
}

void RTRunComputePipelineVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) {
    vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->get_pipeline());
    descriptor_bindings->push(p_command_buffer);
    vkCmdDispatch(p_command_buffer, dispatch_group_count.width, dispatch_group_count.height, dispatch_group_count.depth);
}

} // namespace WhiteBirdEngine
