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
#include "function/render_vk/ray_tracing_vk.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_database.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_acceleration_structure_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

struct RayTracingVK::StaticOpaqueBuildData {
    StaticOpaqueBuildData() = delete;
    ~StaticOpaqueBuildData() = default;
    StaticOpaqueBuildData(const StaticOpaqueBuildData&) = delete;
    StaticOpaqueBuildData(StaticOpaqueBuildData&&) noexcept = default;
    StaticOpaqueBuildData& operator=(const StaticOpaqueBuildData&) = delete;
    StaticOpaqueBuildData& operator=(StaticOpaqueBuildData&&) noexcept = default;

    explicit StaticOpaqueBuildData(RenderAllocator* p_allocator)
        : geometries(p_allocator), build_ranges(p_allocator), primitive_counts(p_allocator) {
    }

    Vector<VkAccelerationStructureGeometryKHR, RenderAllocator> geometries;
    Vector<VkAccelerationStructureBuildRangeInfoKHR, RenderAllocator> build_ranges;
    Vector<uint32_t, RenderAllocator> primitive_counts;
    uint64_t total_primitive_count = 0;
};

static VkAccelerationStructureBuildGeometryInfoKHR make_bottom_level_build_info(
    const Vector<VkAccelerationStructureGeometryKHR, RenderAllocator>& p_geometries) {
    VkAccelerationStructureBuildGeometryInfoKHR result{};
    result.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    result.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    result.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    result.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    result.geometryCount = static_cast<uint32_t>(p_geometries.size());
    result.pGeometries = p_geometries.data();
    return result;
}

static VkAccelerationStructureBuildSizesInfoKHR get_build_sizes(const BaseRenderDataVK* p_render_data,
    const VkAccelerationStructureBuildGeometryInfoKHR& p_build_info,
    const Vector<uint32_t, RenderAllocator>& p_primitive_counts) {
    VkAccelerationStructureBuildSizesInfoKHR result{};
    result.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    p_render_data->get_acceleration_structure_build_sizes(
        p_render_data->logic_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &p_build_info, p_primitive_counts.data(), &result);
    return result;
}

RayTracingVK::RayTracingVK(const BaseRenderDataVK* p_render_data) : render_data(p_render_data) {
}

VkDeviceAddress RayTracingVK::get_buffer_device_address(Ref<ROBufferVK> p_buffer) const {
    VkBufferDeviceAddressInfo address_info{};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.buffer = p_buffer->get_buffer();
    return vkGetBufferDeviceAddress(render_data->logic_device, &address_info);
}

RayTracingVK::StaticOpaqueBuildData RayTracingVK::build_static_opaque_data(const RenderDatabase::OpaqueTrianglesInfo& p_info,
    VkDeviceAddress p_vertex_buffer_address,
    VkDeviceAddress p_index_buffer_address) const {
    StaticOpaqueBuildData result(render_data->allocator);
    result.geometries.reserve(p_info.draw_groups.size());
    result.build_ranges.reserve(p_info.draw_groups.size());
    result.primitive_counts.reserve(p_info.draw_groups.size());
    for (size_t i = 0; i < p_info.draw_groups.size(); ++i) {
        const auto& draw_group = p_info.draw_groups[i];
        if (draw_group.index_sub_region.size == 0) {
            continue;
        }

        const auto position_attribute = std::ranges::find_if(draw_group.vertex_layout.attributes, [](const auto& p_attribute) {
            return p_attribute.role.get_name() == "position";
        });
        if (position_attribute == draw_group.vertex_layout.attributes.end()) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: a draw group has no position attribute.");
        }
        if (position_attribute->format != RUVertexAttributeFormat::FLOAT3) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: position attributes must use FLOAT3 format.");
        }

        const size_t next_vertex_byte_offset =
            i + 1 < p_info.draw_groups.size() ? p_info.draw_groups[i + 1].vertex_buffer_byte_offset : p_info.vertices.size();
        if (next_vertex_byte_offset < draw_group.vertex_buffer_byte_offset) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid vertex-buffer offsets.");
        }
        const size_t vertex_byte_size = next_vertex_byte_offset - draw_group.vertex_buffer_byte_offset;
        if (draw_group.vertex_layout.vertex_stride == 0 || vertex_byte_size % draw_group.vertex_layout.vertex_stride != 0) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid draw-group vertex range.");
        }
        const size_t vertex_count = vertex_byte_size / draw_group.vertex_layout.vertex_stride;
        if (vertex_count == 0 || vertex_count - 1 > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: vertex count exceeds uint32.");
        }
        const size_t primitive_count = draw_group.index_sub_region.size / 3;
        if (primitive_count > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: primitive count exceeds uint32.");
        }
        if (primitive_count > std::numeric_limits<uint64_t>::max() - result.total_primitive_count) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: total primitive count overflowed.");
        }
        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.vertexData.deviceAddress =
            p_vertex_buffer_address + draw_group.vertex_buffer_byte_offset + position_attribute->offset;
        geometry.geometry.triangles.vertexStride = draw_group.vertex_layout.vertex_stride;
        geometry.geometry.triangles.maxVertex = static_cast<uint32_t>(vertex_count - 1);
        geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        geometry.geometry.triangles.indexData.deviceAddress =
            p_index_buffer_address + draw_group.index_sub_region.starting_index * sizeof(uint32_t);
        result.geometries.push_back(geometry);
        result.build_ranges.push_back(VkAccelerationStructureBuildRangeInfoKHR{.primitiveCount = static_cast<uint32_t>(primitive_count)});
        result.primitive_counts.push_back(static_cast<uint32_t>(primitive_count));
        result.total_primitive_count += primitive_count;
    }
    return result;
}

GPUJobVK RayTracingVK::create_static_opaque_build_job(StaticOpaqueBuildData&& p_build_data) {
    if (p_build_data.geometries.size() > std::numeric_limits<uint32_t>::max() ||
        p_build_data.geometries.size() > render_data->max_acceleration_structure_geometry_count ||
        p_build_data.total_primitive_count > render_data->max_acceleration_structure_primitive_count) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: device geometry limits exceeded.");
    }

    VkAccelerationStructureBuildGeometryInfoKHR build_info = make_bottom_level_build_info(p_build_data.geometries);
    VkAccelerationStructureBuildSizesInfoKHR build_sizes = get_build_sizes(render_data, build_info, p_build_data.primitive_counts);

    Ref<ROAccelerationStructureVK> acceleration_structure = make_ref<ROAccelerationStructureVK>(
        render_data->allocator, render_data, build_sizes.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
    const VkDeviceSize scratch_alignment = render_data->min_acceleration_structure_scratch_offset_alignment;
    if (scratch_alignment == 0 || build_sizes.buildScratchSize > std::numeric_limits<VkDeviceSize>::max() - scratch_alignment + 1) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid scratch-buffer size or alignment.");
    }
    Ref<ROBufferVK> scratch_buffer = ROBufferVK::Builder()
                                         .set_domain(DeviceDomain::DEVICE)
                                         .set_size(std::max<VkDeviceSize>(build_sizes.buildScratchSize, 1) + scratch_alignment - 1)
                                         .set_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
                                         .create_ref(render_data);
    build_info.dstAccelerationStructure = acceleration_structure->get_acceleration_structure();
    const VkDeviceAddress scratch_buffer_address = get_buffer_device_address(scratch_buffer);
    build_info.scratchData.deviceAddress = ((scratch_buffer_address + scratch_alignment - 1) / scratch_alignment) * scratch_alignment;
    build_info.pGeometries = nullptr;

    const auto command_build_acceleration_structures = render_data->command_build_acceleration_structures;
    GPUJobVK build_job = [acceleration_structure,
                             scratch_buffer,
                             build_info,
                             geometries = std::move(p_build_data.geometries),
                             build_ranges = std::move(p_build_data.build_ranges),
                             command_build_acceleration_structures](VkCommandBuffer p_command_buffer) mutable {
        // Keep the destination and scratch storage alive until this queued build has been submitted and completed.
        (void)acceleration_structure;
        (void)scratch_buffer;
        build_info.pGeometries = geometries.data();
        const VkAccelerationStructureBuildRangeInfoKHR* build_range_pointer = build_ranges.data();
        command_build_acceleration_structures(p_command_buffer, 1, &build_info, &build_range_pointer);

        VkMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        VkDependencyInfo dependency_info{};
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.memoryBarrierCount = 1;
        dependency_info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(p_command_buffer, &dependency_info);
    };
    static_opaque_acceleration_structure = acceleration_structure;
    return build_job;
}

GPUJobVK RayTracingVK::update_static_opaque_acceleration_structure(
    const RenderDatabase::OpaqueTrianglesInfo& p_info, Ref<ROBufferVK> p_vertex_buffer, Ref<ROBufferVK> p_index_buffer) {
    if (p_info.draw_groups.empty() || p_vertex_buffer == MEM_NULL || p_index_buffer == MEM_NULL) {
        static_opaque_acceleration_structure = MEM_NULL;
        return nullptr;
    }

    StaticOpaqueBuildData build_data =
        build_static_opaque_data(p_info, get_buffer_device_address(p_vertex_buffer), get_buffer_device_address(p_index_buffer));
    if (build_data.geometries.empty()) {
        static_opaque_acceleration_structure = MEM_NULL;
        return nullptr;
    }
    return create_static_opaque_build_job(std::move(build_data));
}

} // namespace WhiteBirdEngine
