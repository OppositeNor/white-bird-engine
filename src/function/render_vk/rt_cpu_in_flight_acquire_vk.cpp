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
#include "function/render_vk/rt_cpu_in_flight_acquire_vk.hh"
#include "core/async/worker.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include <chrono>
#include <future>
#include <semaphore>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTCPUInFlightAcquireVK::RTCPUInFlightAcquireVK(
    const BaseRenderDataVK* p_render_data, std::string_view p_image_view_id, std::span<uint8_t> p_destination, std::binary_semaphore* p_completion_semaphore)
    : render_data(p_render_data), image_view_id(p_image_view_id), destination(p_destination),
      completion_semaphore(p_completion_semaphore) {
    if (render_data == nullptr || render_data->allocator == nullptr) {
        throw std::runtime_error("RTCPUInFlightAcquireVK: render data is unavailable.");
    }
    if (image_view_id.empty()) {
        throw std::runtime_error("RTCPUInFlightAcquireVK: image-view resource id is empty.");
    }
    if (destination.empty()) {
        throw std::runtime_error("RTCPUInFlightAcquireVK: CPU destination is empty.");
    }
    if (completion_semaphore == nullptr) {
        throw std::runtime_error("RTCPUInFlightAcquireVK: completion semaphore is required.");
    }
}

Vector<ResAccessVK, RenderAllocator> RTCPUInFlightAcquireVK::read_resources() const {
    Vector<ResAccessVK, RenderAllocator> resources(render_data->allocator);
    resources.push_back({.res_id = image_view_id, .usage = RendResUsageVK::TRANSFER_SRC});
    return resources;
}

Vector<ResAccessVK, RenderAllocator> RTCPUInFlightAcquireVK::write_resources() const {
    return Vector<ResAccessVK, RenderAllocator>(render_data->allocator);
}

void RTCPUInFlightAcquireVK::bind_resources(const RendResViewVK& p_resources) {
    image_view_container = p_resources.get(image_view_id).container;
    if (image_view_container == MEM_NULL || image_view_container->get_resource_type_id() != TypeUUID<ROImageViewVK>::UUID) {
        throw std::runtime_error("RTCPUInFlightAcquireVK: image-view resource has the wrong type.");
    }
}

void RTCPUInFlightAcquireVK::record(VkCommandBuffer /*p_command_buffer*/, double /*p_delta*/) {
    // Observe a completed continuation so asynchronous failures follow the engine's crash-on-failure convention.
    if (pending_completion.valid() && pending_completion.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        pending_completion.get();
    }

    if (pending_completion.valid()) {
        throw std::runtime_error("RTCPUInFlightAcquireVK: a new acquisition was requested before the previous one completed.");
    }

    if (image_view_container == MEM_NULL) {
        throw std::runtime_error("RTCPUInFlightAcquireVK: source resource has not been bound.");
    }

    Ref<ROImageViewVK> image_view = image_view_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROImageViewVK>();
    if (image_view == MEM_NULL || image_view->fields().image == MEM_NULL) {
        throw std::runtime_error("RTCPUInFlightAcquireVK: image-view resource is unavailable.");
    }

    // The image schedules the GPU copy and worker-side span population without blocking graph recording.
    std::future<void> acquire_future = image_view->fields().image->acquire_image_content(destination);
    Worker* worker = Worker::get_singleton();
    WBE_DEBUG_ASSERT(worker != nullptr);
    pending_completion =
        worker->submit_task([acquire_future = std::move(acquire_future), completion_semaphore = completion_semaphore]() mutable {
            acquire_future.get();
            completion_semaphore->release();
        });
}

} // namespace WhiteBirdEngine
