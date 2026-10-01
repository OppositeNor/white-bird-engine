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
#include "function/render_vk/mesh_gpu_data_vk.hh"
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/matrix.hpp"
#include "utils/defs.hh"
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_resource_list_vk.hh"
#include <cstddef>
#include <string_view>
#include <stdexcept>

namespace WhiteBirdEngine {

MeshResourcesVK::MeshResourcesVK(RenderAllocator* p_allocator)
    : allocator(p_allocator), textures(make_ref<RenderResourceListVK>(p_allocator, p_allocator)), opaque_batches(p_allocator),
      opaque_double_sided_batches(p_allocator), masked_batches(p_allocator) {
    if (allocator == nullptr) {
        throw std::runtime_error("Failed to construct static-mesh resources: allocator is null.");
    }
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
    if (vertices == MEM_NULL || indices == MEM_NULL || submeshes == MEM_NULL || nodes == MEM_NULL || instances == MEM_NULL) {
        throw std::runtime_error("Failed to bind static-mesh resources: geometry or instance tables are missing.");
    }
    const auto member = [p_resource_id](std::string_view p_member) {
        return make_member_resource_id(p_resource_id, p_member);
    };
    if (transform_buffers == MEM_NULL) {
        throw std::runtime_error("Failed to bind mesh resources: transform buffers are missing.");
    }
    vertices->bind(p_graph, member(VERTICES));
    transform_buffers->bind(p_graph, member(TRANSFORM));
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

void MeshResourcesVK::set_transform(const glm::mat4& p_transform) {
    WBE_DEBUG_ASSERT(transform_buffers != MEM_NULL);
    for (size_t column = 0; column < 4; ++column) {
        for (size_t row = 0; row < 4; ++row) {
            if (!std::isfinite(p_transform[column][row])) {
                throw std::runtime_error("Failed to set mesh transform: transform contains a non-finite value.");
            }
        }
    }
    const float determinant = glm::determinant(glm::mat3(p_transform));
    if (!std::isfinite(determinant) || std::abs(determinant) < 0.000001F) {
        throw std::runtime_error("Failed to set mesh transform: transform has no valid inverse normal matrix.");
    }
    transform = p_transform;
    const GeometryNodeGPU node{
        .transform = p_transform,
        .normal_transform = glm::mat4(glm::inverseTranspose(glm::mat3(p_transform))),
    };
    transform_buffers->publish_buffer_update(&node, 0, sizeof(node));
}

} // namespace WhiteBirdEngine
