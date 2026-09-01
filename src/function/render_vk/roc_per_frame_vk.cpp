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
#include "function/render_vk/roc_per_frame_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

static Vector<Ref<RenderObjectVK>, RenderAllocator> create_frame_resources(const BaseRenderDataVK* p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("ROCPerFrameVK: render data is null.");
    }
    WBE_DEBUG_ASSERT(p_render_data->allocator != nullptr);
    WBE_DEBUG_ASSERT(p_render_data->frames_in_flight != 0);

    Vector<Ref<RenderObjectVK>, RenderAllocator> resources(p_render_data->allocator);
    resources.reserve(p_render_data->frames_in_flight);
    for (uint32_t frame_slot = 0; frame_slot < p_render_data->frames_in_flight; ++frame_slot) {
        resources.emplace_back(MEM_NULL);
    }
    return resources;
}

static Vector<uint64_t, RenderAllocator> create_applied_generations(const BaseRenderDataVK* p_render_data) {
    Vector<uint64_t, RenderAllocator> result(p_render_data->allocator);
    result.resize(p_render_data->frames_in_flight, 0);
    return result;
}

ROCPerFrameVK::ROCPerFrameVK(const BaseRenderDataVK* p_render_data)
    : render_data(p_render_data), resources(create_frame_resources(p_render_data)),
      applied_generations(create_applied_generations(p_render_data)), pending_buffer_data(p_render_data->allocator) {
}

ROCPerFrameVK::ROCPerFrameVK(const BaseRenderDataVK* p_render_data, Factory p_factory)
    : render_data(p_render_data), resources(create_frame_resources(p_render_data)),
      applied_generations(create_applied_generations(p_render_data)), pending_buffer_data(p_render_data->allocator) {
    if (!p_factory) {
        throw std::runtime_error("ROCPerFrameVK: resource factory is empty.");
    }

    for (uint32_t frame_slot = 0; frame_slot < render_data->frames_in_flight; ++frame_slot) {
        Ref<RenderObjectVK> resource_ref = p_factory(frame_slot);
        if (resource_ref == MEM_NULL) {
            throw std::runtime_error(std::format("ROCPerFrameVK: resource factory returned null for frame slot {}.", frame_slot));
        }
        retain_resource_type(resource_ref);
        resources[frame_slot] = std::move(resource_ref);
    }
}

Ref<RenderObjectVK> ROCPerFrameVK::get() {
    return get(render_data->current_frame_slot);
}

Ref<RenderObjectVK> ROCPerFrameVK::get() const {
    return get(render_data->current_frame_slot);
}

Ref<RenderObjectVK> ROCPerFrameVK::get(uint32_t p_frame_slot) {
    if (p_frame_slot >= resources.size()) {
        throw std::runtime_error(
            std::format("ROCPerFrameVK: frame slot {} is out of range for {} contexts.", p_frame_slot, resources.size()));
    }
    synchronize(p_frame_slot);
    return resources[p_frame_slot];
}

Ref<RenderObjectVK> ROCPerFrameVK::get(uint32_t p_frame_slot) const {
    if (p_frame_slot >= resources.size()) {
        throw std::runtime_error(
            std::format("ROCPerFrameVK: frame slot {} is out of range for {} contexts.", p_frame_slot, resources.size()));
    }
    synchronize(p_frame_slot);
    return resources[p_frame_slot];
}

void ROCPerFrameVK::set(uint32_t p_frame_slot, Ref<RenderObjectVK> p_resource) {
    if (p_frame_slot >= resources.size()) {
        throw std::runtime_error(
            std::format("ROCPerFrameVK: frame slot {} is out of range for {} contexts.", p_frame_slot, resources.size()));
    }
    retain_resource_type(p_resource);
    resources[p_frame_slot] = std::move(p_resource);
    applied_generations[p_frame_slot] = 0;
}

uint32_t ROCPerFrameVK::get_context_count() const {
    return static_cast<uint32_t>(resources.size());
}

void ROCPerFrameVK::publish_buffer_update(const void* p_data, size_t p_dst_offset, size_t p_size) {
    if (p_data == nullptr || p_size == 0) {
        throw std::runtime_error("ROCPerFrameVK: published buffer data is empty.");
    }
    const auto* data = static_cast<const uint8_t*>(p_data);
    pending_buffer_data.assign(data, data + p_size);
    pending_buffer_dst_offset = p_dst_offset;
    ++update_generation;
    if (update_generation == 0) {
        ++update_generation;
        std::fill(applied_generations.begin(), applied_generations.end(), 0);
    }
}

void ROCPerFrameVK::synchronize(uint32_t p_frame_slot) const {
    if (pending_buffer_data.empty() || applied_generations[p_frame_slot] == update_generation) {
        return;
    }
    Ref<RenderObjectVK> resource = resources[p_frame_slot];
    if (resource == MEM_NULL) {
        throw std::runtime_error(std::format("ROCPerFrameVK: cannot apply update to empty frame slot {}.", p_frame_slot));
    }
    Ref<ROBufferVK> buffer = resource.dynamic_cast_ref<ROBufferVK>();
    if (buffer == MEM_NULL) {
        throw std::runtime_error(
            std::format("ROCPerFrameVK: frame slot {} does not contain a buffer for its pending update.", p_frame_slot));
    }
    buffer->update_buffer(pending_buffer_data.data(), pending_buffer_dst_offset, pending_buffer_data.size());
    applied_generations[p_frame_slot] = update_generation;
}

} // namespace WhiteBirdEngine
