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
#include "function/render_vk/static_set_draw_batch_planner_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

struct StaticSetDrawGroupVK {
    StaticSetDrawGroupVK() = delete;
    explicit StaticSetDrawGroupVK(RenderAllocator* p_allocator)
        : submesh_indices(p_allocator) {
    }
    ~StaticSetDrawGroupVK() = default;
    StaticSetDrawGroupVK(const StaticSetDrawGroupVK&) = delete;
    StaticSetDrawGroupVK& operator=(const StaticSetDrawGroupVK&) = delete;
    StaticSetDrawGroupVK(StaticSetDrawGroupVK&&) noexcept = default;
    StaticSetDrawGroupVK& operator=(StaticSetDrawGroupVK&&) noexcept = default;

    VertexInputDescriptionVK vertex_input;
    size_t vertex_stride = 0;
    VkDeviceSize minimum_offset = 0;
    VkDeviceSize maximum_offset = 0;
    Vector<size_t, RenderAllocator> submesh_indices;
};

static bool static_set_vertex_inputs_equal(const VertexInputDescriptionVK& p_left, const VertexInputDescriptionVK& p_right) {
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

static bool static_set_group_accepts(const StaticSetDrawGroupVK& p_group,
    const VertexInputDescriptionVK& p_vertex_input,
    size_t p_vertex_stride,
    VkDeviceSize p_vertex_offset) {
    if (p_group.vertex_stride != p_vertex_stride ||
        p_group.minimum_offset % p_vertex_stride != p_vertex_offset % p_vertex_stride ||
        !static_set_vertex_inputs_equal(p_group.vertex_input, p_vertex_input)) {
        return false;
    }
    const VkDeviceSize minimum_offset = std::min(p_group.minimum_offset, p_vertex_offset);
    const VkDeviceSize maximum_offset = std::max(p_group.maximum_offset, p_vertex_offset);
    return (maximum_offset - minimum_offset) / p_vertex_stride <= static_cast<VkDeviceSize>(std::numeric_limits<int32_t>::max());
}

static void validate_static_set_draw(const RUStaticGeometrySubmesh& p_submesh) {
    if (p_submesh.indices.binary.start % sizeof(uint32_t) != 0) {
        throw std::runtime_error("Failed to plan static set draws: index buffer offset is not uint32 aligned.");
    }
    if (p_submesh.indices.index_count > std::numeric_limits<uint32_t>::max() ||
        p_submesh.indices.binary.start / sizeof(uint32_t) > std::numeric_limits<uint32_t>::max() ||
        p_submesh.instance_count > std::numeric_limits<uint32_t>::max() ||
        p_submesh.first_instance > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to plan static set draws: draw command exceeds Vulkan uint32 limits.");
    }
    if (p_submesh.vertices.vertex_stride == 0 || p_submesh.vertices.vertex_stride > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to plan static set draws: vertex stride must be nonzero and fit Vulkan uint32 limits.");
    }
}

StaticSetDrawBatchPlannerVK::BuildResult::BuildResult(RenderAllocator* p_allocator)
    : commands(p_allocator), batches(p_allocator) {
}

StaticSetDrawBatchPlannerVK::BuildResult StaticSetDrawBatchPlannerVK::build(RenderAllocator* p_allocator,
    std::span<const RUStaticGeometrySubmesh> p_submeshes,
    std::span<const uint32_t> p_first_instances,
    const ShaderMetadata& p_vertex_metadata,
    std::string_view p_vertex_entry_name,
    uint32_t p_max_draw_count) const {
    if (p_submeshes.size() != p_first_instances.size()) {
        throw std::runtime_error("Failed to plan static set draws: first-instance and submesh counts differ.");
    }
    if (p_max_draw_count == 0) {
        throw std::runtime_error("Failed to plan static set draws: maximum indirect draw count must be nonzero.");
    }
    BuildResult result(p_allocator);
    Vector<StaticSetDrawGroupVK, RenderAllocator> groups(p_allocator);
    size_t drawable_count = 0;
    for (size_t i = 0; i < p_submeshes.size(); ++i) {
        const RUStaticGeometrySubmesh& submesh = p_submeshes[i];
        if (submesh.indices.index_count == 0 || submesh.instance_count == 0) {
            continue;
        }
        validate_static_set_draw(submesh);
        if (drawable_count == std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to plan static set draws: command count exceeds Vulkan uint32 limits.");
        }
        ++drawable_count;
        VertexInputDescriptionVK vertex_input =
            build_vertex_input_description_vk(p_vertex_metadata, p_vertex_entry_name, submesh.vertices);
        if (vertex_input.bindings.size() > 1 ||
            (!vertex_input.bindings.empty() &&
                (vertex_input.bindings.front().binding != 0 || vertex_input.bindings.front().stride != submesh.vertices.vertex_stride))) {
            throw std::runtime_error("Failed to plan static set draws: vertex input must use the shared interleaved binding zero.");
        }
        auto group = std::find_if(groups.begin(), groups.end(), [&](const StaticSetDrawGroupVK& p_group) {
            return static_set_group_accepts(p_group, vertex_input, submesh.vertices.vertex_stride, submesh.vertices.binary.start);
        });
        if (group == groups.end()) {
            groups.emplace_back(p_allocator);
            group = groups.end() - 1;
            group->vertex_input = std::move(vertex_input);
            group->vertex_stride = submesh.vertices.vertex_stride;
            group->minimum_offset = submesh.vertices.binary.start;
            group->maximum_offset = submesh.vertices.binary.start;
        } else {
            group->minimum_offset = std::min(group->minimum_offset, static_cast<VkDeviceSize>(submesh.vertices.binary.start));
            group->maximum_offset = std::max(group->maximum_offset, static_cast<VkDeviceSize>(submesh.vertices.binary.start));
        }
        group->submesh_indices.push_back(i);
    }
    result.commands.reserve(drawable_count);
    for (const StaticSetDrawGroupVK& group : groups) {
        size_t draws_in_batch = 0;
        for (size_t submesh_index : group.submesh_indices) {
            if (draws_in_batch == 0 || draws_in_batch == p_max_draw_count) {
                Batch batch;
                batch.first_draw = static_cast<uint32_t>(result.commands.size());
                batch.vertex_input = group.vertex_input;
                batch.vertex_buffer_offset = group.minimum_offset;
                result.batches.push_back(std::move(batch));
                draws_in_batch = 0;
            }
            const RUStaticGeometrySubmesh& submesh = p_submeshes[submesh_index];
            result.commands.push_back(VkDrawIndexedIndirectCommand{
                .indexCount = static_cast<uint32_t>(submesh.indices.index_count),
                .instanceCount = static_cast<uint32_t>(submesh.instance_count),
                .firstIndex = static_cast<uint32_t>(submesh.indices.binary.start / sizeof(uint32_t)),
                .vertexOffset = static_cast<int32_t>((submesh.vertices.binary.start - group.minimum_offset) / group.vertex_stride),
                .firstInstance = p_first_instances[submesh_index],
            });
            ++result.batches.back().draw_count;
            ++draws_in_batch;
        }
    }
    return result;
}

} // namespace WhiteBirdEngine
