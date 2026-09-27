/* Copyright 2026 OppositeNor

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
#include "function/render_vk/rt_ping_pong_vk.hh"
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

RTPingPongVK::RTPingPongVK(const BaseRenderDataVK* p_render_data,
    Ref<ROComputePipelineVK> p_ping_pipeline,
    Ref<ROComputePipelineVK> p_pong_pipeline,
    bool p_initial_state,
    uint32_t p_iterations,
    VkExtent3D p_dispatch_group_count,
    QueueFamilyVK p_queue_family)
    : RTPingPongVK(p_render_data,
          std::move(p_ping_pipeline),
          std::move(p_pong_pipeline),
          p_initial_state,
          p_iterations,
          p_dispatch_group_count,
          p_queue_family,
          PipelineResourceMapVK(p_render_data != nullptr ? p_render_data->allocator : nullptr),
          PipelineResourceMapVK(p_render_data != nullptr ? p_render_data->allocator : nullptr)) {
}

RTPingPongVK::RTPingPongVK(const BaseRenderDataVK* p_render_data,
    Ref<ROComputePipelineVK> p_ping_pipeline,
    Ref<ROComputePipelineVK> p_pong_pipeline,
    bool p_initial_state,
    uint32_t p_iterations,
    VkExtent3D p_dispatch_group_count,
    QueueFamilyVK p_queue_family,
    const PipelineResourceMapVK& p_ping_resource_map,
    const PipelineResourceMapVK& p_pong_resource_map)
    : render_data(p_render_data), queue_family_id(p_queue_family), ping_pipeline(std::move(p_ping_pipeline)),
      pong_pipeline(std::move(p_pong_pipeline)), dispatch_group_count(p_dispatch_group_count),
      initial_state(p_initial_state), iterations(p_iterations) {
    if (render_data == nullptr || render_data->allocator == nullptr) {
        throw std::runtime_error("RTPingPongVK: render data and its allocator must not be null.");
    }
    if (ping_pipeline == MEM_NULL) {
        throw std::runtime_error("RTPingPongVK: ping compute pipeline is null.");
    }
    if (pong_pipeline == MEM_NULL) {
        throw std::runtime_error("RTPingPongVK: pong compute pipeline is null.");
    }
    ping_descriptor_bindings = make_unique<PipelineDescriptorBindingsVK>(
        render_data->allocator, render_data, ping_pipeline->get_descriptor_layout(), p_ping_resource_map);
    pong_descriptor_bindings = make_unique<PipelineDescriptorBindingsVK>(
        render_data->allocator, render_data, pong_pipeline->get_descriptor_layout(), p_pong_resource_map);
}

void RTPingPongVK::set_iteration_prefixes(
    Ref<ROComputePipelineVK> p_ping_prefix_pipeline, Ref<ROComputePipelineVK> p_pong_prefix_pipeline, VkExtent3D p_dispatch_group_count) {
    set_iteration_prefixes(std::move(p_ping_prefix_pipeline),
        std::move(p_pong_prefix_pipeline),
        p_dispatch_group_count,
        PipelineResourceMapVK(render_data->allocator),
        PipelineResourceMapVK(render_data->allocator));
}

void RTPingPongVK::set_iteration_prefixes(Ref<ROComputePipelineVK> p_ping_prefix_pipeline,
    Ref<ROComputePipelineVK> p_pong_prefix_pipeline,
    VkExtent3D p_dispatch_group_count,
    const PipelineResourceMapVK& p_ping_resource_map,
    const PipelineResourceMapVK& p_pong_resource_map) {
    if (resources_bound) {
        throw std::runtime_error("RTPingPongVK: prefixes cannot change after graph binding.");
    }
    if (p_ping_prefix_pipeline == MEM_NULL || p_pong_prefix_pipeline == MEM_NULL) {
        throw std::runtime_error("RTPingPongVK: ping and pong prefix pipelines must not be null.");
    }
    ping_prefix_pipeline = std::move(p_ping_prefix_pipeline);
    pong_prefix_pipeline = std::move(p_pong_prefix_pipeline);
    prefix_dispatch_group_count = p_dispatch_group_count;
    ping_prefix_descriptor_bindings = make_unique<PipelineDescriptorBindingsVK>(
        render_data->allocator, render_data, ping_prefix_pipeline->get_descriptor_layout(), p_ping_resource_map);
    pong_prefix_descriptor_bindings = make_unique<PipelineDescriptorBindingsVK>(
        render_data->allocator, render_data, pong_prefix_pipeline->get_descriptor_layout(), p_pong_resource_map);
}

void RTPingPongVK::bind_resources(const RendResViewVK& p_resources) {
    resources_bound = true;
    ping_descriptor_bindings->bind_resources(p_resources);
    pong_descriptor_bindings->bind_resources(p_resources);
    if (ping_prefix_descriptor_bindings != MEM_NULL) {
        ping_prefix_descriptor_bindings->bind_resources(p_resources);
        pong_prefix_descriptor_bindings->bind_resources(p_resources);
    }
}

Vector<ResAccessVK, RenderAllocator> RTPingPongVK::read_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

Vector<ResAccessVK, RenderAllocator> RTPingPongVK::write_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

void RTPingPongVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) {
    bool current_state = initial_state;
    for (uint32_t iteration = 0; iteration < iterations; ++iteration) {
        if (ping_prefix_descriptor_bindings != MEM_NULL) {
            if (current_state) {
                vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pong_prefix_pipeline->get_pipeline());
                pong_prefix_descriptor_bindings->push(p_command_buffer);
            } else {
                vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ping_prefix_pipeline->get_pipeline());
                ping_prefix_descriptor_bindings->push(p_command_buffer);
            }
            vkCmdDispatch(p_command_buffer,
                prefix_dispatch_group_count.width,
                prefix_dispatch_group_count.height,
                prefix_dispatch_group_count.depth);
        }

        if (ping_prefix_descriptor_bindings != MEM_NULL) {
            VkMemoryBarrier2 pass_barrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            };
            VkDependencyInfo dependency_info{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &pass_barrier,
            };
            vkCmdPipelineBarrier2(p_command_buffer, &dependency_info);
        }

        if (current_state) {
            vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pong_pipeline->get_pipeline());
            pong_descriptor_bindings->push(p_command_buffer);
        } else {
            vkCmdBindPipeline(p_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ping_pipeline->get_pipeline());
            ping_descriptor_bindings->push(p_command_buffer);
        }
        vkCmdDispatch(p_command_buffer, dispatch_group_count.width, dispatch_group_count.height, dispatch_group_count.depth);
        current_state = !current_state;

        if (iteration + 1 < iterations) {
            VkMemoryBarrier2 pass_barrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            };
            VkDependencyInfo dependency_info{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &pass_barrier,
            };
            vkCmdPipelineBarrier2(p_command_buffer, &dependency_info);
        }
    }
}

} // namespace WhiteBirdEngine
