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
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include <stdexcept>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunComputePipelineVK::RTRunComputePipelineVK(
    const BaseRenderDataVK* p_render_data, std::string_view p_pipeline_id, VkExtent3D p_dispatch_group_count, QueueFamilyVK p_queue_family)
    : render_data(p_render_data), queue_family_id(p_queue_family), pipeline_id(p_pipeline_id),
      dispatch_group_count(p_dispatch_group_count) {
}

Vector<ResAccessVK, RenderAllocator> RTRunComputePipelineVK::read_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

void RTRunComputePipelineVK::validate_resources(const RendResViewVK& p_resources) const {
    Ref<ROComputePipelineVK> compute_pipeline = p_resources.get(pipeline_id).container->get().dynamic_cast_ref<ROComputePipelineVK>();
    if (compute_pipeline == MEM_NULL) {
        throw std::runtime_error("RTRunComputePipelineVK: compute pipeline resource has the wrong type.");
    }
}

Vector<ResAccessVK, RenderAllocator> RTRunComputePipelineVK::write_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

void RTRunComputePipelineVK::record(VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double /*p_delta*/) {
    auto pipeline_resource = p_resources.get(pipeline_id).container;
    auto compute_pipeline = pipeline_resource == MEM_NULL ? Ref<ROComputePipelineVK>(MEM_NULL)
                                                          : pipeline_resource->get().dynamic_cast_ref<ROComputePipelineVK>();
    if (compute_pipeline == MEM_NULL) {
        throw std::runtime_error("RTRunComputePipelineVK: compute pipeline resource is missing.");
    }

    vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline->get_pipeline());
    compute_pipeline->bind_descriptor_sets(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE);
    vkCmdDispatch(p_command_buffer, dispatch_group_count.width, dispatch_group_count.height, dispatch_group_count.depth);
}

} // namespace WhiteBirdEngine
