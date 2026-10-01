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
#include "function/render_vk/mesh_draw_batch_planner_vk.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

struct MeshDrawGroupVK {
    MeshDrawGroupVK() = delete;
    explicit MeshDrawGroupVK(RenderAllocator* p_allocator) : draw_indices(p_allocator) {
    }
    ~MeshDrawGroupVK() = default;
    MeshDrawGroupVK(const MeshDrawGroupVK&) = delete;
    MeshDrawGroupVK& operator=(const MeshDrawGroupVK&) = delete;
    MeshDrawGroupVK(MeshDrawGroupVK&&) noexcept = default;
    MeshDrawGroupVK& operator=(MeshDrawGroupVK&&) noexcept = default;

    VertexInputDescriptionVK vertex_input;
    size_t vertex_stride = 0;
    VkDeviceSize minimum_offset = 0;
    VkDeviceSize maximum_offset = 0;
    Vector<size_t, RenderAllocator> draw_indices;
};

static bool mesh_vertex_inputs_equal(const VertexInputDescriptionVK& p_left, const VertexInputDescriptionVK& p_right) {
    if (p_left.bindings.size() != p_right.bindings.size() || p_left.attributes.size() != p_right.attributes.size()) {
        return false;
    }
    for (size_t i = 0; i < p_left.bindings.size(); ++i) {
        const auto& left = p_left.bindings[i];
        const auto& right = p_right.bindings[i];
        if (left.binding != right.binding || left.stride != right.stride || left.inputRate != right.inputRate ||
            left.divisor != right.divisor) {
            return false;
        }
    }
    for (size_t i = 0; i < p_left.attributes.size(); ++i) {
        const auto& left = p_left.attributes[i];
        const auto& right = p_right.attributes[i];
        if (left.location != right.location || left.binding != right.binding || left.format != right.format ||
            left.offset != right.offset) {
            return false;
        }
    }
    return true;
}

static bool mesh_group_accepts(
    const MeshDrawGroupVK& p_group, const VertexInputDescriptionVK& p_vertex_input, size_t p_vertex_stride, VkDeviceSize p_vertex_offset) {
    if (p_group.vertex_stride != p_vertex_stride || p_group.minimum_offset % p_vertex_stride != p_vertex_offset % p_vertex_stride ||
        !mesh_vertex_inputs_equal(p_group.vertex_input, p_vertex_input)) {
        return false;
    }
    const VkDeviceSize minimum_offset = std::min(p_group.minimum_offset, p_vertex_offset);
    const VkDeviceSize maximum_offset = std::max(p_group.maximum_offset, p_vertex_offset);
    return (maximum_offset - minimum_offset) / p_vertex_stride <= static_cast<VkDeviceSize>(std::numeric_limits<int32_t>::max());
}

static void validate_mesh_draw(const MeshDrawBatchPlannerVK::Draw& p_draw) {
    if (p_draw.indices.binary.start % sizeof(uint32_t) != 0) {
        throw std::runtime_error("Failed to plan mesh draws: index buffer offset is not uint32 aligned.");
    }
    if (p_draw.indices.index_count > std::numeric_limits<uint32_t>::max() ||
        p_draw.indices.binary.start / sizeof(uint32_t) > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to plan mesh draws: draw command exceeds Vulkan uint32 limits.");
    }
    if (p_draw.vertices.vertex_stride == 0 || p_draw.vertices.vertex_stride > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to plan mesh draws: vertex stride must be nonzero and fit Vulkan uint32 limits.");
    }
}

MeshDrawBatchPlannerVK::BuildResult::BuildResult(RenderAllocator* p_allocator)
    : commands(p_allocator), batches(p_allocator) {
}

MeshDrawBatchPlannerVK::BuildResult MeshDrawBatchPlannerVK::build(RenderAllocator* p_allocator,
    std::span<const Draw> p_draws,
    const ShaderMetadata& p_vertex_metadata,
    std::string_view p_vertex_entry_name,
    uint32_t p_max_draw_count) const {
    if (p_max_draw_count == 0) {
        throw std::runtime_error("Failed to plan mesh draws: maximum indirect draw count must be nonzero.");
    }
    BuildResult result(p_allocator);
    Vector<MeshDrawGroupVK, RenderAllocator> groups(p_allocator);
    size_t drawable_count = 0;
    for (size_t i = 0; i < p_draws.size(); ++i) {
        const Draw& draw = p_draws[i];
        if (draw.indices.index_count == 0 || draw.instance_count == 0) {
            continue;
        }
        validate_mesh_draw(draw);
        if (drawable_count == std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to plan mesh draws: command count exceeds Vulkan uint32 limits.");
        }
        ++drawable_count;
        VertexInputDescriptionVK vertex_input =
            build_vertex_input_description_vk(p_vertex_metadata, p_vertex_entry_name, draw.vertices);
        if (vertex_input.bindings.size() > 1 ||
            (!vertex_input.bindings.empty() && (vertex_input.bindings.front().binding != 0 ||
                                                   vertex_input.bindings.front().stride != draw.vertices.vertex_stride))) {
            throw std::runtime_error("Failed to plan mesh draws: vertex input must use the shared interleaved binding zero.");
        }
        auto group = std::find_if(groups.begin(), groups.end(), [&](const MeshDrawGroupVK& p_group) {
            return mesh_group_accepts(p_group, vertex_input, draw.vertices.vertex_stride, draw.vertices.binary.start);
        });
        if (group == groups.end()) {
            groups.emplace_back(p_allocator);
            group = groups.end() - 1;
            group->vertex_input = std::move(vertex_input);
            group->vertex_stride = draw.vertices.vertex_stride;
            group->minimum_offset = draw.vertices.binary.start;
            group->maximum_offset = draw.vertices.binary.start;
        } else {
            group->minimum_offset = std::min(group->minimum_offset, static_cast<VkDeviceSize>(draw.vertices.binary.start));
            group->maximum_offset = std::max(group->maximum_offset, static_cast<VkDeviceSize>(draw.vertices.binary.start));
        }
        group->draw_indices.push_back(i);
    }
    result.commands.reserve(drawable_count);
    for (const MeshDrawGroupVK& group : groups) {
        size_t draws_in_batch = 0;
        for (size_t draw_index : group.draw_indices) {
            if (draws_in_batch == 0 || draws_in_batch == p_max_draw_count) {
                Batch batch;
                batch.first_draw = static_cast<uint32_t>(result.commands.size());
                batch.vertex_input = group.vertex_input;
                batch.vertex_buffer_offset = group.minimum_offset;
                result.batches.push_back(std::move(batch));
                draws_in_batch = 0;
            }
            const Draw& draw = p_draws[draw_index];
            result.commands.push_back(VkDrawIndexedIndirectCommand{
                .indexCount = static_cast<uint32_t>(draw.indices.index_count),
                .instanceCount = draw.instance_count,
                .firstIndex = static_cast<uint32_t>(draw.indices.binary.start / sizeof(uint32_t)),
                .vertexOffset = static_cast<int32_t>((draw.vertices.binary.start - group.minimum_offset) / group.vertex_stride),
                .firstInstance = draw.first_instance,
            });
            ++result.batches.back().draw_count;
            ++draws_in_batch;
        }
    }
    return result;
}

MeshDrawBatchPlannerVK::BuildResult MeshDrawBatchPlannerVK::build(RenderAllocator* p_allocator,
    std::span<const RUSubmesh3D> p_submeshes,
    std::span<const uint32_t> p_first_instances,
    const ShaderMetadata& p_vertex_metadata,
    std::string_view p_vertex_entry_name,
    uint32_t p_max_draw_count) const {
    if (p_submeshes.size() != p_first_instances.size()) {
        throw std::runtime_error("Failed to plan mesh draws: first-instance and submesh counts differ.");
    }
    Vector<Draw, RenderAllocator> draws(p_allocator);
    draws.reserve(p_submeshes.size());
    for (size_t i = 0; i < p_submeshes.size(); ++i) {
        const RUSubmesh3D& submesh = p_submeshes[i];
        if (submesh.instance_count > std::numeric_limits<uint32_t>::max() ||
            submesh.first_instance > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to plan mesh draws: draw command exceeds Vulkan uint32 limits.");
        }
        Draw draw;
        draw.vertices = submesh.vertices;
        draw.indices = submesh.indices;
        draw.instance_count = static_cast<uint32_t>(submesh.instance_count);
        draw.first_instance = p_first_instances[i];
        draws.push_back(std::move(draw));
    }
    return build(p_allocator, std::span<const Draw>(draws.data(), draws.size()), p_vertex_metadata, p_vertex_entry_name, p_max_draw_count);
}

} // namespace WhiteBirdEngine
