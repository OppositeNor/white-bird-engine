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
#include "function/render_vk/geometry_draw_data_vk.hh"
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
#include <limits>
#include <stdexcept>

namespace WhiteBirdEngine {

GeometryDrawDataVK::GeometryDrawDataVK(
    RenderAllocator* p_allocator, uint32_t p_capacity, std::span<const VkDrawIndexedIndirectCommand> p_commands)
    : nodes(p_allocator), commands(p_commands.begin(), p_commands.end(), p_allocator) {
    if (p_capacity == 0 || commands.empty()) {
        throw std::runtime_error("Dynamic mesh draw data requires capacity and commands.");
    }
    for (auto& command : commands) {
        if (command.firstInstance > std::numeric_limits<uint32_t>::max() - p_capacity || command.instanceCount != p_capacity) {
            throw std::runtime_error("Dynamic mesh draw range does not reserve the configured capacity.");
        }
        command.instanceCount = 0;
    }
    nodes.resize(p_capacity);
}

void GeometryDrawDataVK::update(std::span<const glm::mat4> p_visible_transforms) {
    if (p_visible_transforms.size() > nodes.size()) {
        throw std::runtime_error("Dynamic mesh visible instance count exceeds capacity.");
    }
    for (const auto& transform : p_visible_transforms) {
        for (size_t column = 0; column < 4; ++column) {
            for (size_t row = 0; row < 4; ++row) {
                if (!std::isfinite(transform[column][row])) {
                    throw std::runtime_error("Dynamic mesh transform contains a non-finite value.");
                }
            }
        }
        const float determinant = glm::determinant(glm::mat3(transform));
        if (!std::isfinite(determinant) || std::abs(determinant) < 0.000001F) {
            throw std::runtime_error("Dynamic mesh transform has no valid inverse normal matrix.");
        }
    }
    for (size_t i = 0; i < nodes.size(); ++i) {
        nodes[i] = {};
        if (i < p_visible_transforms.size()) {
            nodes[i].transform = p_visible_transforms[i];
            nodes[i].normal_transform = glm::mat4(glm::inverseTranspose(glm::mat3(p_visible_transforms[i])));
        }
    }
    for (auto& command : commands) {
        command.instanceCount = static_cast<uint32_t>(p_visible_transforms.size());
    }
}

} // namespace WhiteBirdEngine
