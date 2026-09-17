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
#ifndef WBE_FILE_STATIC_SET_DRAW_BATCH_PLANNER_VK_HH
#define WBE_FILE_STATIC_SET_DRAW_BATCH_PLANNER_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstdint>
#include <span>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class StaticSetDrawBatchPlannerVK
 * @brief Groups one pipeline's static draws by compatible reflected vertex input and buffer offsets.
 *
 * Stateless and copyable. Planning preserves supplied instance indices when reordering draws.
 */
class StaticSetDrawBatchPlannerVK final {
public:
    /**
     * @class Batch
     * @brief Copyable owned vertex-input state for a contiguous range of indirect commands.
     */
    struct Batch {
        /**
         * @brief Construct an empty command range.
         */
        Batch() = default;
        /**
         * @brief Release owned vertex-input descriptions.
         */
        ~Batch() = default;
        /**
         * @brief Copy the command range and vertex-input descriptions.
         */
        Batch(const Batch&) = default;
        /**
         * @brief Move the command range and vertex-input descriptions.
         */
        Batch(Batch&&) noexcept = default;
        /**
         * @brief Copy the command range and vertex-input descriptions.
         */
        Batch& operator=(const Batch&) = default;
        /**
         * @brief Move the command range and vertex-input descriptions.
         */
        Batch& operator=(Batch&&) noexcept = default;
        /**
         * @brief Index of the first command in the result's command array.
         */
        uint32_t first_draw = 0;
        /**
         * @brief Number of commands submitted together.
         */
        uint32_t draw_count = 0;
        /**
         * @brief Effective reflected vertex-input state shared by all draws in this batch.
         */
        VertexInputDescriptionVK vertex_input;
        /**
         * @brief Shared vertex-buffer base offset; command vertex offsets are relative to this base.
         */
        VkDeviceSize vertex_buffer_offset = 0;
    };

    /**
     * @class BuildResult
     * @brief Move-only command and batch storage; its allocator must outlive the result.
     */
    struct BuildResult {
        BuildResult() = delete;
        /**
         * @brief Construct empty storage owned by the supplied allocator.
         */
        explicit BuildResult(RenderAllocator* p_allocator);
        /**
         * @brief Release owned command and batch storage.
         */
        ~BuildResult() = default;
        BuildResult(const BuildResult&) = delete;
        BuildResult& operator=(const BuildResult&) = delete;
        /**
         * @brief Transfer storage and allocator ownership associations.
         */
        BuildResult(BuildResult&&) noexcept = default;
        /**
         * @brief Transfer storage and allocator ownership associations.
         */
        BuildResult& operator=(BuildResult&&) noexcept = default;
        /**
         * @brief Tightly packed indirect commands in batch order.
         */
        Vector<VkDrawIndexedIndirectCommand, RenderAllocator> commands;
        /**
         * @brief Batches in first-seen compatible-group order.
         */
        Vector<Batch, RenderAllocator> batches;
    };

    /**
     * @brief Construct a stateless planner.
     */
    StaticSetDrawBatchPlannerVK() = default;
    /**
     * @brief Destroy the stateless planner.
     */
    ~StaticSetDrawBatchPlannerVK() = default;
    /**
     * @brief Copy the stateless planner.
     */
    StaticSetDrawBatchPlannerVK(const StaticSetDrawBatchPlannerVK&) = default;
    /**
     * @brief Move the stateless planner.
     */
    StaticSetDrawBatchPlannerVK(StaticSetDrawBatchPlannerVK&&) noexcept = default;
    /**
     * @brief Copy the stateless planner.
     */
    StaticSetDrawBatchPlannerVK& operator=(const StaticSetDrawBatchPlannerVK&) = default;
    /**
     * @brief Move the stateless planner.
     */
    StaticSetDrawBatchPlannerVK& operator=(StaticSetDrawBatchPlannerVK&&) noexcept = default;

    /**
     * @brief Plan drawable submeshes sharing prevalidated vertex and index binaries for one pipeline.
     *
     * First-instance entries correspond to input submeshes. Empty draws are skipped. Compatible groups
     * preserve source order internally and split at the device's maximum indirect draw count.
     * @throws std::runtime_error When spans differ, the draw limit is zero, or Vulkan draw/layout limits are exceeded.
     */
    BuildResult build(RenderAllocator* p_allocator,
        std::span<const RUStaticGeometrySubmesh> p_submeshes,
        std::span<const uint32_t> p_first_instances,
        const ShaderMetadata& p_vertex_metadata,
        std::string_view p_vertex_entry_name,
        uint32_t p_max_draw_count) const;
};

} // namespace WhiteBirdEngine

#endif
