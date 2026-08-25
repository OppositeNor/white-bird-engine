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
#ifndef WBE_FILE_STATIC_OPAQUE_VERTEX_BATCH_PLANNER_VK_HH
#define WBE_FILE_STATIC_OPAQUE_VERTEX_BATCH_PLANNER_VK_HH

#include "function/render/render_database.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class StaticOpaqueVertexBatchPlannerVK
 * @brief Builds dynamic vertex-input batches from flattened static draw groups.
 */
class StaticOpaqueVertexBatchPlannerVK final {
public:
    /**
     * @brief One indirect command range sharing a vertex layout and bound buffer offset.
     */
    struct Batch {
        /**
         * @brief Construct an empty batch.
         */
        Batch() = default;
        /**
         * @brief Destroy the owned dynamic description.
         */
        ~Batch() = default;
        /**
         * @brief Copy a batch.
         */
        Batch(const Batch&) = default;
        /**
         * @brief Move a batch.
         */
        Batch(Batch&&) noexcept = default;
        /**
         * @brief Copy a batch.
         */
        Batch& operator=(const Batch&) = default;
        /**
         * @brief Move a batch.
         */
        Batch& operator=(Batch&&) noexcept = default;

        /** First indirect command in the shared command buffer. */
        uint32_t first_draw = 0;
        /** Number of indirect commands rendered by this batch. */
        uint32_t draw_count = 0;
        /** Byte offset bound for the shared vertex buffer. */
        VkDeviceSize vertex_buffer_offset = 0;
        /** Dynamic vertex-input state for the draw group's resource layout. */
        VertexInputDescriptionVK vertex_input;
    };

    /**
     * @brief Construct a stateless planner.
     */
    StaticOpaqueVertexBatchPlannerVK() = default;
    /**
     * @brief Destroy a planner.
     */
    ~StaticOpaqueVertexBatchPlannerVK() = default;
    /**
     * @brief Copy a planner.
     */
    StaticOpaqueVertexBatchPlannerVK(const StaticOpaqueVertexBatchPlannerVK&) = default;
    /**
     * @brief Move a planner.
     */
    StaticOpaqueVertexBatchPlannerVK(StaticOpaqueVertexBatchPlannerVK&&) noexcept = default;
    /**
     * @brief Copy a planner.
     */
    StaticOpaqueVertexBatchPlannerVK& operator=(const StaticOpaqueVertexBatchPlannerVK&) = default;
    /**
     * @brief Move a planner.
     */
    StaticOpaqueVertexBatchPlannerVK& operator=(StaticOpaqueVertexBatchPlannerVK&&) noexcept = default;

    /**
     * @brief Build one batch per contiguous vertex layout in a pipeline range.
     * @throws std::runtime_error When the group range, indirect range, vertex layout, or shader contract is invalid.
     */
    std::vector<Batch> build(const ShaderMetadata& p_vertex_metadata,
        std::string_view p_vertex_entry_name,
        const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
        size_t p_first_draw_group,
        size_t p_draw_group_count) const;
};

} // namespace WhiteBirdEngine

#endif
