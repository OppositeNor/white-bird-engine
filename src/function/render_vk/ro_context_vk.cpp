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
#include "function/render_vk/ro_context_vk.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_object_vk.hh"
#include <format>
#include <stdexcept>

namespace WhiteBirdEngine {

namespace {

RenderAllocator* context_allocator(const BaseRenderDataVK* p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("ROContextVK: render data is null.");
    }
    return p_render_data->allocator;
}

} // namespace

ROContextVK::ROContextVK(const BaseRenderDataVK* p_render_data, RendResTID p_resource_type_id, Factory p_factory)
    : RenderObjectVK(p_render_data), resource_type_id(p_resource_type_id), resources(context_allocator(p_render_data)) {
    WBE_DEBUG_ASSERT(render_data->allocator != nullptr);
    WBE_DEBUG_ASSERT(render_data->frames_in_flight != 0);
    if (!p_factory) {
        throw std::runtime_error("ROContextVK: resource factory is empty.");
    }

    resources.reserve(render_data->frames_in_flight);
    for (uint32_t context_id = 0; context_id < render_data->frames_in_flight; ++context_id) {
        Ref<RenderObjectVK> resource = p_factory(context_id);
        if (resource == MEM_NULL) {
            throw std::runtime_error(std::format("ROContextVK: resource factory returned null for frame slot {}.", context_id));
        }
        resources.emplace_back(std::move(resource));
    }
}

RendResTID ROContextVK::get_resource_type_id() const {
    return resource_type_id;
}

uint32_t ROContextVK::get_context_count() const {
    return static_cast<uint32_t>(resources.size());
}

Ref<RenderObjectVK> ROContextVK::get() const {
    return get(render_data->current_frame_slot);
}

Ref<RenderObjectVK> ROContextVK::get(uint32_t p_context_id) const {
    if (p_context_id >= resources.size()) {
        throw std::runtime_error(
            std::format("ROContextVK: frame slot {} is out of range for {} contexts.", p_context_id, resources.size()));
    }
    return resources[p_context_id];
}

} // namespace WhiteBirdEngine
