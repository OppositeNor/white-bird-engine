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
#ifndef WBE_FILE_GEOMETRY_DRAW_DATA_VK_HH
#define WBE_FILE_GEOMETRY_DRAW_DATA_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "function/render/render_defs.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <span>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @struct GeometrySubmeshGPU
 * @brief Material-table row addressed by an instance link.
 */
struct GeometrySubmeshGPU {
    /** Index into the reflected material parameter table. */
    uint32_t material_id = 0;
    /** GPU row field; padding must remain zero. */
    uint32_t padding_0 = 0;
    /** GPU row field; padding must remain zero. */
    uint32_t padding_1 = 0;
    /** GPU row field; padding must remain zero. */
    uint32_t padding_2 = 0;
};

/**
 * @struct GeometryNodeGPU
 * @brief Model and inverse-transpose normal matrices shared by mesh passes.
 */
struct GeometryNodeGPU {
    /** Object-to-world transform. */
    glm::mat4 transform{1.0F};
    /** Inverse transpose of the model matrix linear part. */
    glm::mat4 normal_transform{1.0F};
};

/**
 * @struct GeometryInstanceGPU
 * @brief Immutable link from firstInstance to a submesh and transform slot.
 */
struct GeometryInstanceGPU {
    /** Index of the material-bearing submesh row. */
    uint32_t submesh_index = 0;
    /** Index of the transform slot. */
    uint32_t node_index = 0;
    /** GPU row field; padding must remain zero. */
    uint32_t padding_0 = 0;
    /** GPU row field; padding must remain zero. */
    uint32_t padding_1 = 0;
};

/**
 * @class GeometryDrawDataVK
 * @brief Fixed-capacity CPU publication data for known draw ranges. Neither copyable nor movable.
 * Commands retain geometry and firstInstance addressing; every group uses the same dense visible transform list.
 * Each group's immutable instance links must reserve capacity entries pointing to nodes [0, capacity).
 */
class GeometryDrawDataVK {
public:
    GeometryDrawDataVK() = delete;
    /**
     * @brief Snapshot immutable command geometry and reserve transform slots. Invalid capacity or ranges throw.
     */
    GeometryDrawDataVK(RenderAllocator* p_allocator, uint32_t p_capacity, std::span<const VkDrawIndexedIndirectCommand> p_commands);
    ~GeometryDrawDataVK() = default;
    GeometryDrawDataVK(const GeometryDrawDataVK&) = delete;
    GeometryDrawDataVK(GeometryDrawDataVK&&) = delete;
    GeometryDrawDataVK& operator=(const GeometryDrawDataVK&) = delete;
    GeometryDrawDataVK& operator=(GeometryDrawDataVK&&) = delete;
    /**
     * @brief Replace all visible transforms and draw counts. Overflow or singular transforms throw before mutation.
     */
    void update(std::span<const glm::mat4> p_visible_transforms);
    /**
     * @brief Complete node array to publish once to the stable frame-slot container.
     */
    std::span<const GeometryNodeGPU> get_nodes() const {
        return nodes;
    }
    /**
     * @brief Complete command array to publish once to the stable frame-slot container.
     */
    std::span<const VkDrawIndexedIndirectCommand> get_commands() const {
        return commands;
    }

private:
    Vector<GeometryNodeGPU, RenderAllocator> nodes;
    Vector<VkDrawIndexedIndirectCommand, RenderAllocator> commands;
};

} // namespace WhiteBirdEngine
#endif
