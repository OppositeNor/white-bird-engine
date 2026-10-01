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
#include "function/render_vk/dynamic_mesh_resources_vk.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_resource_list_vk.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <span>
#include <string_view>
#include <stdexcept>

namespace WhiteBirdEngine {

DynamicMeshResourcesVK::DynamicMeshResourcesVK(RenderAllocator* p_allocator)
    : allocator(p_allocator), textures(make_ref<RenderResourceListVK>(p_allocator, p_allocator)), batches(p_allocator) {
    if (allocator == nullptr) {
        throw std::runtime_error("Failed to construct dynamic-mesh resources: allocator is null.");
    }
}

const MeshDrawBatchVK& DynamicMeshResourcesVK::get_draw_batch(size_t p_index) const {
    if (p_index >= batches.size()) {
        throw std::runtime_error("Failed to get dynamic-mesh draw batch: index is out of bounds.");
    }
    return batches[p_index];
}

void DynamicMeshResourcesVK::publish_visible_transforms(std::span<const glm::mat4> p_transforms) {
    WBE_DEBUG_ASSERT(draw_data != MEM_NULL && nodes != MEM_NULL && draw_batches != MEM_NULL);
    draw_data->update(p_transforms);
    nodes->publish_buffer_update(draw_data->get_nodes().data(), 0, draw_data->get_nodes().size_bytes());
    draw_batches->publish_buffer_update(draw_data->get_commands().data(), 0, draw_data->get_commands().size_bytes());
}

void DynamicMeshResourcesVK::bind(RendGraphDescVK& p_graph, std::string_view p_resource_id) const {
    if (vertices == MEM_NULL || indices == MEM_NULL || draw_batches == MEM_NULL || submeshes == MEM_NULL || nodes == MEM_NULL ||
        instances == MEM_NULL) {
        throw std::runtime_error("Failed to bind dynamic-mesh resources: geometry, command, or instance tables are missing.");
    }
    const auto member = [p_resource_id](std::string_view p_member) {
        return make_member_resource_id(p_resource_id, p_member);
    };
    vertices->bind(p_graph, member(VERTICES));
    indices->bind(p_graph, member(INDICES));
    draw_batches->bind(p_graph, member(DRAW_BATCHES));
    submeshes->bind(p_graph, member(SUBMESHES));
    nodes->bind(p_graph, member(NODES));
    instances->bind(p_graph, member(INSTANCES));
    textures->bind(p_graph, member(TEXTURES));
    if (material_params != MEM_NULL) {
        material_params->bind(p_graph, member(MATERIAL_PARAMS));
    }
    if (material_sampler != MEM_NULL) {
        material_sampler->bind(p_graph, member(MATERIAL_SAMPLER));
    }
}

} // namespace WhiteBirdEngine
