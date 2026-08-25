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
#include "function/render_vk/static_opaque_vertex_batch_planner_vk.hh"
#include "function/render/render_database.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

std::vector<StaticOpaqueVertexBatchPlannerVK::Batch> StaticOpaqueVertexBatchPlannerVK::build(const ShaderMetadata& p_vertex_metadata,
    std::string_view p_vertex_entry_name,
    const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    size_t p_first_draw_group,
    size_t p_draw_group_count) const {
    if (p_first_draw_group > p_triangles_info.draw_groups.size() ||
        p_draw_group_count > p_triangles_info.draw_groups.size() - p_first_draw_group) {
        throw std::runtime_error("Failed to plan static opaque vertex batches: draw-group range is invalid.");
    }
    std::vector<Batch> result;
    result.reserve(p_draw_group_count);
    const size_t draw_group_end = p_first_draw_group + p_draw_group_count;
    for (size_t draw_group_index = p_first_draw_group; draw_group_index < draw_group_end; ++draw_group_index) {
        const RenderDatabase::OpaqueTrianglesInfo::DrawGroup& draw_group = p_triangles_info.draw_groups[draw_group_index];
        if (draw_group.indirect_command_sub_region.starting_index > std::numeric_limits<uint32_t>::max() ||
            draw_group.indirect_command_sub_region.size > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to plan static opaque vertex batches: indirect command range exceeds uint32.");
        }
        Batch batch;
        batch.first_draw = static_cast<uint32_t>(draw_group.indirect_command_sub_region.starting_index);
        batch.draw_count = static_cast<uint32_t>(draw_group.indirect_command_sub_region.size);
        batch.vertex_buffer_offset = static_cast<VkDeviceSize>(draw_group.vertex_buffer_byte_offset);
        batch.vertex_input = build_vertex_input_description_vk(p_vertex_metadata, p_vertex_entry_name, draw_group.vertex_layout);
        result.push_back(std::move(batch));
    }
    return result;
}

} // namespace WhiteBirdEngine
