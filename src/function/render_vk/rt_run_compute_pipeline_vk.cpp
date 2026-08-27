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
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunComputePipelineVK::RTRunComputePipelineVK(const BaseRenderDataVK* p_render_data,
    std::string_view p_pipeline_id,
    VkExtent3D p_dispatch_group_count,
    std::span<const Ref<RODescriptorSetVK>> p_descriptor_sets,
    std::span<const ResAccessVK> p_read_accesses,
        std::span<const ResAccessVK> p_write_accesses,
        QueueFamilyVK p_queue_family)
        : render_data(p_render_data), queue_family_id(p_queue_family), pipeline_id(p_pipeline_id), dispatch_group_count(p_dispatch_group_count),
      descriptor_sets(p_descriptor_sets.begin(), p_descriptor_sets.end(), p_render_data->allocator),
      descriptor_sets_instances(p_render_data->allocator),
      additional_read_accesses(p_read_accesses.begin(), p_read_accesses.end(), p_render_data->allocator),
      additional_write_accesses(p_write_accesses.begin(), p_write_accesses.end(), p_render_data->allocator) {
    descriptor_sets_instances.reserve(descriptor_sets.size());
}

Vector<ResAccessVK, RenderAllocator> RTRunComputePipelineVK::read_resources() const {
    Vector<ResAccessVK, RenderAllocator> read_accesses(render_data->allocator);
    read_accesses.reserve(additional_read_accesses.size() + 1);
    read_accesses.push_back(
        {.res_id = pipeline_id, .access = VK_ACCESS_2_SHADER_READ_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT});
    for (const auto& access : additional_read_accesses) {
        read_accesses.push_back(access);
    }
    return read_accesses;
}

Vector<ResAccessVK, RenderAllocator> RTRunComputePipelineVK::write_resources() const {
    Vector<ResAccessVK, RenderAllocator> write_accesses(render_data->allocator);
    write_accesses.reserve(additional_write_accesses.size());
    for (const auto& access : additional_write_accesses) {
        write_accesses.push_back(access);
    }
    return write_accesses;
}

void RTRunComputePipelineVK::record(VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double /*p_delta*/) {
    auto compute_pipeline = p_resources.get(pipeline_id).res.dynamic_cast_ref<ROComputePipelineVK>();
    if (compute_pipeline == MEM_NULL) {
        throw std::runtime_error("RTRunComputePipelineVK: compute pipeline resource is missing.");
    }

    vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline->get_compute_pipeline());
    descriptor_sets_instances.clear();
    for (const auto& descriptor_set : descriptor_sets) {
        if (descriptor_set == MEM_NULL) {
            throw std::runtime_error("RTRunComputePipelineVK: descriptor set is missing.");
        }
        VkDescriptorSet descriptor_set_instance = descriptor_set->get_descriptor_set();
        if (descriptor_set_instance == VK_NULL_HANDLE) {
            throw std::runtime_error("RTRunComputePipelineVK: descriptor set is missing.");
        }
        descriptor_sets_instances.push_back(descriptor_set_instance);
    }
    if (!descriptor_sets_instances.empty()) {
        vkCmdBindDescriptorSets(p_command_buffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            compute_pipeline->fields().pipeline_layout->get_pipeline_layout(),
            0,
            static_cast<uint32_t>(descriptor_sets_instances.size()),
            descriptor_sets_instances.data(),
            0,
            nullptr);
    }
    vkCmdDispatch(p_command_buffer, dispatch_group_count.width, dispatch_group_count.height, dispatch_group_count.depth);
}

} // namespace WhiteBirdEngine
