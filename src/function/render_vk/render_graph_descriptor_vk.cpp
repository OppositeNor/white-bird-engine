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
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_task.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task_resource_map.hh"
#include "function/render_vk/i_bindable_render_resource_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RendTaskDescVK::RendTaskDescVK(RenderAllocator* p_allocator) : next(p_allocator), resource_map(p_allocator) {
}

RendTaskDescVK::RendTaskDescVK(RenderAllocator* p_allocator, std::string_view p_task_id, RendTaskFactoryVK p_factory)
    : task_id(p_task_id), next(p_allocator), resource_map(p_allocator), factory(std::move(p_factory)) {
    if (factory == nullptr) {
        throw std::runtime_error("Failed to construct render task descriptor: task factory is null.");
    }
}

RendTaskDescVK& RendTaskDescVK::connect(std::string_view p_next_task_id, std::string_view p_resource_id) {
    next.push_back({
        .next_task_id = std::string(p_next_task_id),
        .res_id = std::string(p_resource_id),
    });
    return *this;
}

RendTaskDescVK& RendTaskDescVK::alias(std::string_view p_task_resource_id, std::string_view p_graph_resource_id) {
    resource_map.bind(p_task_resource_id, p_graph_resource_id);
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_task(RendTaskDescVK p_task_desc) {
    tasks.emplace_back(std::move(p_task_desc));
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_task(std::string_view p_task_id, RendTaskFactoryVK p_factory) {
    tasks.emplace_back(allocator, p_task_id, std::move(p_factory));
    return *this;
}

RendTaskDescVK& RendGraphDescVK::get_task(std::string_view p_task_id) {
    for (RendTaskDescVK& task : tasks) {
        if (task.task_id == p_task_id) {
            return task;
        }
    }
    throw std::runtime_error(std::format("Failed to get render graph task: no task has id '{}'.", p_task_id));
}

RendGraphDescVK& RendGraphDescVK::add_resource(RendResVK p_resource) {
    if (p_resource.container != MEM_NULL) {
        RendResTID concrete_type_id = p_resource.container->get_resource_type_id();
        if (concrete_type_id == 0) {
            throw std::runtime_error("Failed to add render graph resource: container has no concrete resource type.");
        }
        if (p_resource.res_type_id != 0 && p_resource.res_type_id != concrete_type_id) {
            throw std::runtime_error("Failed to add render graph resource: container type does not match the declared type.");
        }
        p_resource.res_type_id = concrete_type_id;
    }
    resources.emplace_back(std::move(p_resource));
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, const IBindableRenderResourceVK& p_resource) {
    p_resource.bind(*this, p_resource_id);
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_buffer(std::string_view p_resource_id, RendBufferFactoryVK p_factory) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph buffer: resource id is empty.");
    }
    if (p_factory == nullptr) {
        throw std::runtime_error("Failed to add render graph buffer: factory is null.");
    }
    buffers.push_back({.res_id = std::string(p_resource_id), .factory = std::move(p_factory)});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_image(std::string_view p_resource_id, RendImageFactoryVK p_factory) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph image: resource id is empty.");
    }
    if (p_factory == nullptr) {
        throw std::runtime_error("Failed to add render graph image: factory is null.");
    }
    images.push_back({.res_id = std::string(p_resource_id), .factory = std::move(p_factory)});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_image_view(
    std::string_view p_resource_id, std::string_view p_image_id, RendImageViewFactoryVK p_factory) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph image view: resource id is empty.");
    }
    if (p_image_id.empty()) {
        throw std::runtime_error("Failed to add render graph image view: image id is empty.");
    }
    if (p_factory == nullptr) {
        p_factory = [](const RendGraphContextVK& /*p_context*/) {
            return RendImageViewDescVK{};
        };
    }
    image_views.push_back({.res_id = std::string(p_resource_id), .image_id = std::string(p_image_id), .factory = std::move(p_factory)});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_sampler(std::string_view p_resource_id, RendSamplerFactoryVK p_factory) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph sampler: resource id is empty.");
    }
    if (p_factory == nullptr) {
        p_factory = [](const RendGraphContextVK& /*p_context*/) {
            return RendSamplerDescVK{};
        };
    }
    samplers.push_back({.res_id = std::string(p_resource_id), .factory = std::move(p_factory)});
    return *this;
}

bool RendBufferDescVK::operator==(const RendBufferDescVK& p_other) const {
    return usage == p_other.usage && additional_usage == p_other.additional_usage && size == p_other.size &&
           domain == p_other.domain && per_frame == p_other.per_frame && initial_data == p_other.initial_data;
}

bool RendImageDescVK::operator==(const RendImageDescVK& p_other) const {
    return usage == p_other.usage && additional_usage == p_other.additional_usage && format == p_other.format &&
           extent.width == p_other.extent.width && extent.height == p_other.extent.height &&
           extent.depth == p_other.extent.depth && image_type == p_other.image_type && mip_levels == p_other.mip_levels &&
           layers == p_other.layers && per_frame == p_other.per_frame;
}

bool RendImageViewDescVK::operator==(const RendImageViewDescVK& p_other) const {
    return view_type == p_other.view_type && format == p_other.format && aspect_mask == p_other.aspect_mask &&
           base_mip_level == p_other.base_mip_level && mip_level_count == p_other.mip_level_count &&
           base_array_layer == p_other.base_array_layer && layer_count == p_other.layer_count;
}

bool RendSamplerDescVK::operator==(const RendSamplerDescVK& p_other) const {
    return min_filter == p_other.min_filter && mag_filter == p_other.mag_filter && mipmap_mode == p_other.mipmap_mode &&
           address_mode_u == p_other.address_mode_u && address_mode_v == p_other.address_mode_v &&
           address_mode_w == p_other.address_mode_w && mip_lod_bias == p_other.mip_lod_bias && max_anisotropy == p_other.max_anisotropy &&
           compare_op == p_other.compare_op && min_lod == p_other.min_lod && max_lod == p_other.max_lod &&
           border_color == p_other.border_color && unnormalized_coordinates == p_other.unnormalized_coordinates;
}

} // namespace WhiteBirdEngine
