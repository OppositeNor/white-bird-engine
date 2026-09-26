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
#include "function/render_vk/rt_cpu_in_flight_buffer_acquire_vk.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_staging_buffer_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include <atomic>
#include <cstring>
#include <semaphore>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTCPUInFlightBufferAcquireVK::RTCPUInFlightBufferAcquireVK(const BaseRenderDataVK* p_render_data,
    std::string_view p_buffer_id,
    std::span<uint8_t> p_destination,
    std::atomic_bool* p_acquire_requested,
    std::binary_semaphore* p_completion_semaphore)
    : render_data(p_render_data), buffer_id(p_buffer_id), destination(p_destination), acquire_requested(p_acquire_requested),
      completion_semaphore(p_completion_semaphore) {
    if (render_data == nullptr || render_data->allocator == nullptr) {
        throw std::runtime_error("RTCPUInFlightBufferAcquireVK: render data is unavailable.");
    }
    if (buffer_id.empty()) {
        throw std::runtime_error("RTCPUInFlightBufferAcquireVK: buffer resource id is empty.");
    }
    if (destination.empty()) {
        throw std::runtime_error("RTCPUInFlightBufferAcquireVK: CPU destination is empty.");
    }
    if (acquire_requested == nullptr || completion_semaphore == nullptr) {
        throw std::runtime_error("RTCPUInFlightBufferAcquireVK: acquisition flag and completion semaphore are required.");
    }
}

Vector<ResAccessVK, RenderAllocator> RTCPUInFlightBufferAcquireVK::read_resources() const {
    Vector<ResAccessVK, RenderAllocator> resources(render_data->allocator);
    resources.push_back({.res_id = buffer_id, .usage = RendResUsageVK::TRANSFER_SRC});
    return resources;
}

Vector<ResAccessVK, RenderAllocator> RTCPUInFlightBufferAcquireVK::write_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

void RTCPUInFlightBufferAcquireVK::bind_resources(const RendResViewVK& p_resources) {
    buffer_container = p_resources.get(buffer_id).container;
    if (buffer_container == MEM_NULL || buffer_container->get_resource_type_id() != TypeUUID<ROBufferVK>::UUID) {
        throw std::runtime_error("RTCPUInFlightBufferAcquireVK: buffer resource has the wrong type.");
    }
}

void RTCPUInFlightBufferAcquireVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) {
    if (pending_completion != MEM_NULL && pending_completion->load(std::memory_order_acquire)) {
        pending_completion = MEM_NULL;
    }

    // An unrequested empty command buffer still completes the task's normal render-graph semaphore edges.
    if (!acquire_requested->exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (pending_completion != MEM_NULL) {
        throw std::runtime_error("RTCPUInFlightBufferAcquireVK: a new acquisition was requested before the previous one completed.");
    }

    Ref<ROBufferVK> buffer = buffer_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROBufferVK>();
    if (buffer == MEM_NULL) {
        throw std::runtime_error("RTCPUInFlightBufferAcquireVK: buffer resource is unavailable.");
    }
    const size_t buffer_size = buffer->fields().buffer_size;
    if (destination.size_bytes() < buffer_size) {
        throw std::runtime_error("RTCPUInFlightBufferAcquireVK: CPU destination is smaller than the source buffer.");
    }

    const size_t staging_buffer_size = ROStagingBufferVK::required_buffer_size(buffer_size);
    Ref<ROStagingBufferVK> staging_buffer = ROStagingBufferVK::Builder()
                                                    .set_size(staging_buffer_size)
                                                    .set_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                                                    .create_ref(render_data);
    const ROStagingBufferVK::Allocation staging_allocation = staging_buffer->allocate_region(buffer_size);
    const VkBufferCopy copy_region{
        .srcOffset = 0,
        .dstOffset = staging_allocation.offset,
        .size = buffer_size,
    };
    vkCmdCopyBuffer(p_command_buffer, buffer->get_buffer(), staging_buffer->get_buffer(), 1, &copy_region);

    const VkBufferMemoryBarrier2 destination_barrier =
        ROBufferVK::MemoryBarrierBuilder()
            .set_buffer(staging_buffer->get_buffer(), staging_buffer->fields().buffer_size)
            .set_src_access(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT)
            .set_dst_access(VK_ACCESS_2_HOST_READ_BIT, VK_PIPELINE_STAGE_2_HOST_BIT)
            .set_data_region(staging_allocation.offset, buffer_size)
            .build();
    VkDependencyInfo destination_dependency{};
    destination_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    destination_dependency.bufferMemoryBarrierCount = 1;
    destination_dependency.pBufferMemoryBarriers = &destination_barrier;
    vkCmdPipelineBarrier2(p_command_buffer, &destination_dependency);

    pending_completion = make_ref<std::atomic_bool>(render_data->allocator, false);
    WBE_DEBUG_ASSERT(render_data->frames_in_flight > 0);
    // The copy belongs to the current frame slot. Reaching this callback after the other frame slots have advanced means the
    // renderer has wrapped to this slot and waited for its fence, so host reads cannot race the recorded transfer. This is one
    // frame earlier than the staging allocation's normal delayed release, keeping its mapped region reserved during the copy.
    render_data->register_delayed_call(
        [completion = pending_completion,
            staging_buffer,
            mapped_memory = staging_allocation.data,
            buffer_size,
            destination = destination,
            completion_semaphore = completion_semaphore]() mutable {
            std::memcpy(destination.data(), mapped_memory, buffer_size);
            completion_semaphore->release();
            completion->store(true, std::memory_order_release);
        },
        render_data->frames_in_flight - 1);
}

} // namespace WhiteBirdEngine
