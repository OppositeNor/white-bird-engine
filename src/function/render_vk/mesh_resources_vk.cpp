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
#include "function/render_vk/mesh_resources_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_resource_list_vk.hh"
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <stdexcept>

namespace WhiteBirdEngine {

MeshResourcesVK::MeshResourcesVK(RenderAllocator* p_allocator)
    : allocator(p_allocator), vertex_buffers(p_allocator), vertex_binaries(p_allocator),
      textures(make_ref<RenderResourceListVK>(p_allocator, p_allocator)), opaque_batches(p_allocator),
      opaque_double_sided_batches(p_allocator), masked_batches(p_allocator) {
    if (allocator == nullptr) {
        throw std::runtime_error("Failed to construct static-mesh resources: allocator is null.");
    }
}

std::string MeshResourcesVK::get_vertex_buffer_member(size_t p_index) {
    return std::format("{}.{}", VERTICES, p_index);
}

Ref<ROBufferVK> MeshResourcesVK::acquire_vertex_buffer(size_t p_index) const {
    if (p_index >= vertex_buffers.size()) {
        throw std::runtime_error("Failed to acquire static-mesh vertex buffer: index is out of bounds.");
    }
    return vertex_buffers[p_index];
}

size_t MeshResourcesVK::get_vertex_buffer_index(RefRes<RUBinary> p_binary) const {
    for (size_t i = 0; i < vertex_binaries.size(); ++i) {
        if (vertex_binaries[i] == p_binary) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find static-mesh vertex buffer: no vertex buffer was compiled from the binary.");
}

const MeshDrawBatchVK& MeshResourcesVK::get_opaque_draw_batch(size_t p_index) const {
    if (p_index >= opaque_batches.size()) {
        throw std::runtime_error("Failed to get static-mesh opaque draw batch: index is out of bounds.");
    }
    return opaque_batches[p_index];
}

const MeshDrawBatchVK& MeshResourcesVK::get_opaque_double_sided_draw_batch(size_t p_index) const {
    if (p_index >= opaque_double_sided_batches.size()) {
        throw std::runtime_error("Failed to get static-mesh opaque_double_sided draw batch: index is out of bounds.");
    }
    return opaque_double_sided_batches[p_index];
}

const MeshDrawBatchVK& MeshResourcesVK::get_masked_draw_batch(size_t p_index) const {
    if (p_index >= masked_batches.size()) {
        throw std::runtime_error("Failed to get static-mesh masked draw batch: index is out of bounds.");
    }
    return masked_batches[p_index];
}

void MeshResourcesVK::bind(RendGraphDescVK& p_graph, std::string_view p_resource_id) const {
    if (vertex_buffers.empty() || indices == MEM_NULL || submeshes == MEM_NULL || nodes == MEM_NULL || instances == MEM_NULL) {
        throw std::runtime_error("Failed to bind static-mesh resources: geometry or instance tables are missing.");
    }
    const auto member = [p_resource_id](std::string_view p_member) {
        return make_member_resource_id(p_resource_id, p_member);
    };
    for (size_t i = 0; i < vertex_buffers.size(); ++i) {
        vertex_buffers[i]->bind(p_graph, member(get_vertex_buffer_member(i)));
    }
    indices->bind(p_graph, member(INDICES));
    submeshes->bind(p_graph, member(SUBMESHES));
    nodes->bind(p_graph, member(NODES));
    instances->bind(p_graph, member(INSTANCES));
    textures->bind(p_graph, member(TEXTURES));
    if (opaque_draw_batches != MEM_NULL) {
        opaque_draw_batches->bind(p_graph, member(OPAQUE_DRAW_BATCHES));
    }
    if (opaque_double_sided_draw_batches != MEM_NULL) {
        opaque_double_sided_draw_batches->bind(p_graph, member(OPAQUE_DOUBLE_SIDED_DRAW_BATCHES));
    }
    if (masked_draw_batches != MEM_NULL) {
        masked_draw_batches->bind(p_graph, member(MASKED_DRAW_BATCHES));
    }
    if (material_params != MEM_NULL) {
        material_params->bind(p_graph, member(MATERIAL_PARAMS));
    }
    if (material_sampler != MEM_NULL) {
        material_sampler->bind(p_graph, member(MATERIAL_SAMPLER));
    }
}

} // namespace WhiteBirdEngine
