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
#include "entity_draw_data.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_rend_object.hh"
#include "function/rend_vk/mesh_gpu_data_vk.hh"
#include "function/rend_vk/ro_buffer_vk.hh"
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/matrix.hpp"
#include <cmath>
#include <cstdint>
#include <glm/gtc/matrix_inverse.hpp>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

EntityDrawData::EntityDrawData(const RendContextVK* p_rend_data, Ref<MeshResourcesVK> p_mesh) : mesh(std::move(p_mesh)) {
    if (p_rend_data == nullptr || mesh == MEM_NULL) {
        throw std::runtime_error("Failed to construct entity draw data: render data or mesh is missing.");
    }
    transform_buffers = make_ref<ROCMultislots>(p_rend_data->alloc,
        p_rend_data->alloc,
        p_rend_data->frames_in_flight,
        [p_rend_data](uint32_t) -> Ref<IRendObject> {
            return p_rend_data->create_buffer(ROBufferVK::Builder().set_properties(sizeof(GeometryNodeGPU), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
        });
    set_transform(glm::mat4(1.0F));
}

void EntityDrawData::set_transform(const glm::mat4& p_transform) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(p_transform[column][row])) {
                throw std::runtime_error("Failed to set entity transform: transform contains a non-finite value.");
            }
        }
    }
    const float determinant = glm::determinant(glm::mat3(p_transform));
    if (!std::isfinite(determinant) || std::abs(determinant) < 0.000001F) {
        throw std::runtime_error("Failed to set entity transform: transform has no valid inverse normal matrix.");
    }
    transform = p_transform;
    const GeometryNodeGPU node{
        .transform = p_transform,
        .normal_transform = glm::mat4(glm::inverseTranspose(glm::mat3(p_transform))),
    };
    transform_buffers->publish_buffer_update(&node, 0, sizeof(node));
}

void EntityDrawData::bind(RendGraphDescVK& p_graph, std::string_view p_resource_id) const {
    bind_container(p_graph, make_member_resource_id(p_resource_id, TRANSFORM), transform_buffers);
}

} // namespace WhiteBirdEngine
