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
#include "function/render_vk/ray_tracing_plugin_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/function_utils.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_acceleration_structure_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "utils/defs.hh"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

// Query one Vulkan feature flag through the features2 pNext chain.
template <VkStructureType StructureType, typename FeatureType>
static bool check_device_feature_support(VkPhysicalDevice p_device, VkBool32 FeatureType::* p_feature) {
    FeatureType feature{};
    feature.sType = StructureType;

    VkPhysicalDeviceFeatures2 device_features{};
    device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features.pNext = &feature;
    vkGetPhysicalDeviceFeatures2(p_device, &device_features);

    return feature.*p_feature == VK_TRUE;
}

// Verify that every requested extension is exposed either globally or by an enabled layer.
template <typename EnumerateFunction>
static bool check_extension_support(std::span<const char* const> p_extensions,
    std::span<const char* const> p_enabled_layers,
    const EnumerateFunction& p_enumerate_extensions) {
    std::set<std::string> unsupported_extensions(p_extensions.begin(), p_extensions.end());

    auto remove_supported_extensions = [&](const char* p_layer_name) {
        VkResult result = VK_SUCCESS;
        do {
            uint32_t extension_count = 0;
            result = p_enumerate_extensions(p_layer_name, &extension_count, nullptr);
            if (result != VK_SUCCESS) {
                return false;
            }

            std::vector<VkExtensionProperties> extension_properties(extension_count);
            result = p_enumerate_extensions(p_layer_name, &extension_count, extension_properties.data());
            if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
                return false;
            }
            if (result == VK_SUCCESS) {
                for (const auto& extension : extension_properties) {
                    unsupported_extensions.erase(extension.extensionName);
                }
            }
        } while (result == VK_INCOMPLETE);
        return true;
    };

    if (!remove_supported_extensions(nullptr)) {
        return false;
    }
    for (const char* layer_name : p_enabled_layers) {
        if (!remove_supported_extensions(layer_name)) {
            return false;
        }
    }
    return unsupported_extensions.empty();
}

struct RayTracingPluginVK::StaticOpaqueBuildData {
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

struct RayTracingPluginVK::StaticOpaqueTopLevelBuildData {
    StaticOpaqueTopLevelBuildData() = delete;
    ~StaticOpaqueTopLevelBuildData() = default;
    StaticOpaqueTopLevelBuildData(const StaticOpaqueTopLevelBuildData&) = delete;
    StaticOpaqueTopLevelBuildData(StaticOpaqueTopLevelBuildData&&) noexcept = default;
    StaticOpaqueTopLevelBuildData& operator=(const StaticOpaqueTopLevelBuildData&) = delete;
    StaticOpaqueTopLevelBuildData& operator=(StaticOpaqueTopLevelBuildData&&) noexcept = default;

    explicit StaticOpaqueTopLevelBuildData(RenderAllocator* p_allocator) : instances(p_allocator) {
    }

    Vector<VkAccelerationStructureInstanceKHR, RenderAllocator> instances;
};

static VkAccelerationStructureBuildGeometryInfoKHR make_bottom_level_build_info(
    std::span<const VkAccelerationStructureGeometryKHR> p_geometries) {
    VkAccelerationStructureBuildGeometryInfoKHR result{};
    result.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    result.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    result.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    result.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    result.geometryCount = static_cast<uint32_t>(p_geometries.size());
    result.pGeometries = p_geometries.data();
    return result;
}

static VkAccelerationStructureBuildGeometryInfoKHR make_top_level_build_info(const VkAccelerationStructureGeometryKHR* p_geometry) {
    VkAccelerationStructureBuildGeometryInfoKHR result{};
    result.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    result.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    result.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    result.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    result.geometryCount = 1;
    result.pGeometries = p_geometry;
    return result;
}

static VkAccelerationStructureBuildSizesInfoKHR get_build_sizes(const BaseRenderDataVK* p_render_data,
    const VkAccelerationStructureBuildGeometryInfoKHR& p_build_info,
    std::span<const uint32_t> p_primitive_counts) {
    VkAccelerationStructureBuildSizesInfoKHR result{};
    result.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    p_render_data->get_acceleration_structure_build_sizes(
        p_render_data->logic_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &p_build_info, p_primitive_counts.data(), &result);
    return result;
}

RayTracingPluginVK::RayTracingPluginVK(const BaseRenderDataVK* p_render_data) : render_data(p_render_data) {
    singleton = this;
}

RayTracingPluginVK::~RayTracingPluginVK() {
    singleton = nullptr;
}

bool RayTracingPluginVK::try_enable(
    RendererCapabilities& p_capabilities, VkPhysicalDevice p_physical_device, const char* const* p_enabled_layers, size_t p_enabled_layer_count) {
    static constexpr std::array<const char*, 3> REQUIRED_EXTENSIONS{
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    };
    enabled = false;
    if (!p_capabilities.ray_tracing) {
        return false;
    }
    // Reject ray tracing early unless the device supports the required extensions, buffer format, and feature set.
    if (!check_extension_support(REQUIRED_EXTENSIONS,
            std::span<const char* const>(p_enabled_layers, p_enabled_layer_count),
            [p_physical_device](const char* p_layer_name, uint32_t* p_extension_count, VkExtensionProperties* p_extension_properties) {
                return vkEnumerateDeviceExtensionProperties(p_physical_device, p_layer_name, p_extension_count, p_extension_properties);
            })) {
        p_capabilities.ray_tracing = false;
        return false;
    }

    VkFormatProperties position_format_properties{};
    vkGetPhysicalDeviceFormatProperties(p_physical_device, VK_FORMAT_R32G32B32_SFLOAT, &position_format_properties);
    if ((position_format_properties.bufferFeatures & VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR) == 0) {
        p_capabilities.ray_tracing = false;
        return false;
    }

    if (!check_device_feature_support<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES>(
            p_physical_device, &VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress) ||
        !check_device_feature_support<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>(
            p_physical_device, &VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure) ||
        !check_device_feature_support<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR>(
            p_physical_device, &VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline)) {
        p_capabilities.ray_tracing = false;
        return false;
    }

    buffer_device_address_features = {};
    buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    buffer_device_address_features.bufferDeviceAddress = VK_TRUE;
    acceleration_structure_features = {};
    acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    acceleration_structure_features.accelerationStructure = VK_TRUE;
    ray_tracing_pipeline_features = {};
    ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    ray_tracing_pipeline_features.rayTracingPipeline = VK_TRUE;
    buffer_device_address_features.pNext = &acceleration_structure_features;
    acceleration_structure_features.pNext = &ray_tracing_pipeline_features;
    enabled = true;
    return true;
}

std::span<const char* const> RayTracingPluginVK::get_required_instance_extensions() const {
    return {};
}

std::span<const char* const> RayTracingPluginVK::get_required_device_extensions() const {
    static constexpr std::array<const char*, 3> REQUIRED_EXTENSIONS{
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    };
    WBE_DEBUG_ASSERT(enabled);
    return REQUIRED_EXTENSIONS;
}

VkBaseOutStructure* RayTracingPluginVK::get_device_feature_chain() {
    WBE_DEBUG_ASSERT(enabled);
    return reinterpret_cast<VkBaseOutStructure*>(&buffer_device_address_features);
}

void RayTracingPluginVK::on_device_created(BaseRenderDataVK* p_render_data, VkPhysicalDevice /*p_physical_device*/, VkDevice p_logical_device) {
    WBE_DEBUG_ASSERT(enabled);

    // Resolve the Vulkan acceleration-structure entry points after logical-device creation.
    p_render_data->create_acceleration_structure = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(p_logical_device, "vkCreateAccelerationStructureKHR"));
    p_render_data->destroy_acceleration_structure = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(p_logical_device, "vkDestroyAccelerationStructureKHR"));
    p_render_data->get_acceleration_structure_build_sizes = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(p_logical_device, "vkGetAccelerationStructureBuildSizesKHR"));
    p_render_data->command_build_acceleration_structures = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(p_logical_device, "vkCmdBuildAccelerationStructuresKHR"));
    p_render_data->get_acceleration_structure_device_address = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(p_logical_device, "vkGetAccelerationStructureDeviceAddressKHR"));
    if (p_render_data->create_acceleration_structure == nullptr || p_render_data->destroy_acceleration_structure == nullptr ||
        p_render_data->get_acceleration_structure_build_sizes == nullptr || p_render_data->command_build_acceleration_structures == nullptr ||
        p_render_data->get_acceleration_structure_device_address == nullptr) {
        throw std::runtime_error("Failed to load Vulkan acceleration-structure commands.");
    }
}

void RayTracingPluginVK::update_base_render_data(BaseRenderDataVK* p_render_data, VkPhysicalDevice p_physical_device) const {
    WBE_DEBUG_ASSERT(enabled);

    VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties{};
    acceleration_structure_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 physical_device_properties{};
    physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physical_device_properties.pNext = &acceleration_structure_properties;
    vkGetPhysicalDeviceProperties2(p_physical_device, &physical_device_properties);
    p_render_data->min_acceleration_structure_scratch_offset_alignment =
        acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment;
    p_render_data->max_acceleration_structure_geometry_count = acceleration_structure_properties.maxGeometryCount;
    p_render_data->max_acceleration_structure_primitive_count = acceleration_structure_properties.maxPrimitiveCount;
}

VkBufferUsageFlags RayTracingPluginVK::get_static_opaque_vertex_buffer_usage() const {
    WBE_DEBUG_ASSERT(enabled);
    return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
}

VkBufferUsageFlags RayTracingPluginVK::get_static_opaque_index_buffer_usage() const {
    WBE_DEBUG_ASSERT(enabled);
    return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
}

VkDeviceAddress RayTracingPluginVK::get_buffer_device_address(Ref<ROBufferVK> p_buffer) const {
    VkBufferDeviceAddressInfo address_info{};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.buffer = p_buffer->get_buffer();
    return vkGetBufferDeviceAddress(render_data->logic_device, &address_info);
}

VkDeviceAddress RayTracingPluginVK::get_acceleration_structure_device_address(Ref<ROAccelerationStructureVK> p_acceleration_structure) const {
    VkAccelerationStructureDeviceAddressInfoKHR address_info{};
    address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    address_info.accelerationStructure = p_acceleration_structure->get_acceleration_structure();
    return render_data->get_acceleration_structure_device_address(render_data->logic_device, &address_info);
}

RayTracingPluginVK::StaticOpaqueBuildData RayTracingPluginVK::build_static_opaque_data(
    const RenderDatabase::OpaqueTrianglesInfo& p_info, VkDeviceAddress p_vertex_buffer_address, VkDeviceAddress p_index_buffer_address) const {
    StaticOpaqueBuildData result(render_data->allocator);
    result.geometries.reserve(p_info.draw_groups.size());
    result.build_ranges.reserve(p_info.draw_groups.size());
    result.primitive_counts.reserve(p_info.draw_groups.size());
    // Translate each valid draw group into BLAS geometry while validating the mesh layout and index ranges.
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

RayTracingPluginVK::StaticOpaqueTopLevelBuildData RayTracingPluginVK::build_static_opaque_top_level_data(
    Ref<ROAccelerationStructureVK> p_bottom_level_acceleration_structure) const {
    StaticOpaqueTopLevelBuildData result(render_data->allocator);

    // Build a single identity TLAS instance that references the generated bottom-level acceleration structure.
    VkAccelerationStructureInstanceKHR instance{};
    instance.transform.matrix[0][0] = 1.0F;
    instance.transform.matrix[1][1] = 1.0F;
    instance.transform.matrix[2][2] = 1.0F;
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = get_acceleration_structure_device_address(p_bottom_level_acceleration_structure);
    result.instances.push_back(instance);
    return result;
}

GPUJobVK RayTracingPluginVK::create_static_opaque_build_job(StaticOpaqueBuildData&& p_build_data) {
    if (p_build_data.geometries.size() > std::numeric_limits<uint32_t>::max() ||
        p_build_data.geometries.size() > render_data->max_acceleration_structure_geometry_count ||
        p_build_data.total_primitive_count > render_data->max_acceleration_structure_primitive_count) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: device geometry limits exceeded.");
    }
    VkAccelerationStructureBuildGeometryInfoKHR bottom_level_build_info = make_bottom_level_build_info(p_build_data.geometries);
    VkAccelerationStructureBuildSizesInfoKHR bottom_level_build_sizes =
        get_build_sizes(render_data, bottom_level_build_info, p_build_data.primitive_counts);

    Ref<ROAccelerationStructureVK> bottom_level_acceleration_structure = make_ref<ROAccelerationStructureVK>(
        render_data->allocator, render_data, bottom_level_build_sizes.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
    const VkDeviceSize scratch_alignment = render_data->min_acceleration_structure_scratch_offset_alignment;
    if (scratch_alignment == 0 ||
        bottom_level_build_sizes.buildScratchSize > std::numeric_limits<VkDeviceSize>::max() - scratch_alignment + 1) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid scratch-buffer size or alignment.");
    }
    Ref<ROBufferVK> bottom_level_scratch_buffer =
        ROBufferVK::Builder()
            .set_domain(DeviceDomain::DEVICE)
            .set_size(std::max<VkDeviceSize>(bottom_level_build_sizes.buildScratchSize, 1) + scratch_alignment - 1)
            .set_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            .create_ref(render_data);
    bottom_level_build_info.dstAccelerationStructure = bottom_level_acceleration_structure->get_acceleration_structure();
    const VkDeviceAddress bottom_level_scratch_buffer_address = get_buffer_device_address(bottom_level_scratch_buffer);
    bottom_level_build_info.scratchData.deviceAddress =
        ((bottom_level_scratch_buffer_address + scratch_alignment - 1) / scratch_alignment) * scratch_alignment;
    bottom_level_build_info.pGeometries = nullptr;

    StaticOpaqueTopLevelBuildData top_level_build_data = build_static_opaque_top_level_data(bottom_level_acceleration_structure);

    // Prepare the TLAS build inputs and scratch resources after sizing and allocating the BLAS.
    Ref<ROBufferVK> top_level_instance_buffer =
        ROBufferVK::Builder()
            .set_domain(DeviceDomain::HOST)
            .set_size(sizeof(VkAccelerationStructureInstanceKHR) * top_level_build_data.instances.size())
            .set_usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            .create_ref(render_data);
    top_level_instance_buffer->update_buffer(top_level_build_data.instances.data(),
        sizeof(VkAccelerationStructureInstanceKHR) * top_level_build_data.instances.size());

    VkAccelerationStructureGeometryKHR top_level_geometry{};
    top_level_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    top_level_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    top_level_geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    top_level_geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    top_level_geometry.geometry.instances.data.deviceAddress = get_buffer_device_address(top_level_instance_buffer);

    Vector<uint32_t, RenderAllocator> top_level_primitive_counts(render_data->allocator);
    top_level_primitive_counts.push_back(static_cast<uint32_t>(top_level_build_data.instances.size()));
    VkAccelerationStructureBuildGeometryInfoKHR top_level_build_info = make_top_level_build_info(&top_level_geometry);
    VkAccelerationStructureBuildSizesInfoKHR top_level_build_sizes =
        get_build_sizes(render_data, top_level_build_info, top_level_primitive_counts);
    if (top_level_build_info.geometryCount > render_data->max_acceleration_structure_geometry_count ||
        top_level_build_data.instances.size() > render_data->max_acceleration_structure_primitive_count) {
        throw std::runtime_error("Failed to build static-opaque top-level acceleration structure: device geometry limits exceeded.");
    }
    if (top_level_build_sizes.buildScratchSize > std::numeric_limits<VkDeviceSize>::max() - scratch_alignment + 1) {
        throw std::runtime_error("Failed to build static-opaque top-level acceleration structure: invalid scratch-buffer size or alignment.");
    }
    Ref<ROAccelerationStructureVK> top_level_acceleration_structure = make_ref<ROAccelerationStructureVK>(
        render_data->allocator, render_data, top_level_build_sizes.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);
    Ref<ROBufferVK> top_level_scratch_buffer =
        ROBufferVK::Builder()
            .set_domain(DeviceDomain::DEVICE)
            .set_size(std::max<VkDeviceSize>(top_level_build_sizes.buildScratchSize, 1) + scratch_alignment - 1)
            .set_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            .create_ref(render_data);
    top_level_build_info.dstAccelerationStructure = top_level_acceleration_structure->get_acceleration_structure();
    const VkDeviceAddress top_level_scratch_buffer_address = get_buffer_device_address(top_level_scratch_buffer);
    top_level_build_info.scratchData.deviceAddress =
        ((top_level_scratch_buffer_address + scratch_alignment - 1) / scratch_alignment) * scratch_alignment;
    top_level_build_info.pGeometries = &top_level_geometry;

    Vector<VkAccelerationStructureBuildRangeInfoKHR, RenderAllocator> top_level_build_ranges(render_data->allocator);
    top_level_build_ranges.push_back(VkAccelerationStructureBuildRangeInfoKHR{.primitiveCount = top_level_primitive_counts[0]});

    const auto command_build_acceleration_structures = render_data->command_build_acceleration_structures;
    GPUJobVK build_job = [bottom_level_acceleration_structure,
                             bottom_level_scratch_buffer,
                             bottom_level_build_info,
                             bottom_level_geometries = std::move(p_build_data.geometries),
                             bottom_level_build_ranges = std::move(p_build_data.build_ranges),
                             top_level_acceleration_structure,
                             top_level_instance_buffer,
                             top_level_scratch_buffer,
                             top_level_build_info,
                             top_level_geometry,
                             top_level_build_ranges = std::move(top_level_build_ranges),
                             command_build_acceleration_structures](VkCommandBuffer p_command_buffer) mutable {
        // Keep all build inputs, scratch storage, and acceleration structures alive until the queued build completes.
        (void)bottom_level_acceleration_structure;
        (void)bottom_level_scratch_buffer;
        (void)top_level_acceleration_structure;
        (void)top_level_instance_buffer;
        (void)top_level_scratch_buffer;
        // Record BLAS build, synchronize its writes, then build the TLAS and expose it to ray-tracing shaders.
        bottom_level_build_info.pGeometries = bottom_level_geometries.data();
        const VkAccelerationStructureBuildRangeInfoKHR* bottom_level_build_range_pointer = bottom_level_build_ranges.data();
        command_build_acceleration_structures(p_command_buffer, 1, &bottom_level_build_info, &bottom_level_build_range_pointer);

        VkMemoryBarrier2 build_barrier{};
        build_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        build_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        build_barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        build_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        build_barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        VkDependencyInfo build_dependency_info{};
        build_dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        build_dependency_info.memoryBarrierCount = 1;
        build_dependency_info.pMemoryBarriers = &build_barrier;
        vkCmdPipelineBarrier2(p_command_buffer, &build_dependency_info);

        top_level_build_info.pGeometries = &top_level_geometry;
        const VkAccelerationStructureBuildRangeInfoKHR* top_level_build_range_pointer = top_level_build_ranges.data();
        command_build_acceleration_structures(p_command_buffer, 1, &top_level_build_info, &top_level_build_range_pointer);

        VkMemoryBarrier2 shader_read_barrier{};
        shader_read_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        shader_read_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        shader_read_barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        shader_read_barrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        shader_read_barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        VkDependencyInfo shader_read_dependency_info{};
        shader_read_dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        shader_read_dependency_info.memoryBarrierCount = 1;
        shader_read_dependency_info.pMemoryBarriers = &shader_read_barrier;
        vkCmdPipelineBarrier2(p_command_buffer, &shader_read_dependency_info);
    };
    static_opaque_bottom_level_acceleration_structure = bottom_level_acceleration_structure;
    static_opaque_top_level_acceleration_structure = top_level_acceleration_structure;
    static_opaque_top_level_instance_buffer = top_level_instance_buffer;
    return build_job;
}

GPUJobVK RayTracingPluginVK::update_static_opaque_acceleration_structure(
    const RenderDatabase::OpaqueTrianglesInfo& p_info, Ref<ROBufferVK> p_vertex_buffer, Ref<ROBufferVK> p_index_buffer) {
    if (p_info.draw_groups.empty() || p_vertex_buffer == MEM_NULL || p_index_buffer == MEM_NULL) {
        static_opaque_bottom_level_acceleration_structure = MEM_NULL;
        static_opaque_top_level_acceleration_structure = MEM_NULL;
        static_opaque_top_level_instance_buffer = MEM_NULL;
        return nullptr;
    }

    StaticOpaqueBuildData build_data =
        build_static_opaque_data(p_info, get_buffer_device_address(p_vertex_buffer), get_buffer_device_address(p_index_buffer));
    if (build_data.geometries.empty()) {
        static_opaque_bottom_level_acceleration_structure = MEM_NULL;
        static_opaque_top_level_acceleration_structure = MEM_NULL;
        static_opaque_top_level_instance_buffer = MEM_NULL;
        return nullptr;
    }
    return create_static_opaque_build_job(std::move(build_data));
}

GPUJobVK RayTracingPluginVK::on_static_opaque_triangles_updated(BaseRenderDataVK* p_render_data,
    const RenderDatabase::OpaqueTrianglesInfo& p_info,
    Ref<ROBufferVK> p_vertex_buffer,
    Ref<ROBufferVK> p_index_buffer,
    Ref<ROBufferVK> /*p_object_id_buffer*/) {
    WBE_DEBUG_ASSERT(enabled);

    render_data = p_render_data;
    GPUJobVK build_job = update_static_opaque_acceleration_structure(p_info, p_vertex_buffer, p_index_buffer);
    p_render_data->static_opaque_acceleration_structure =
        static_opaque_top_level_acceleration_structure == MEM_NULL
            ? VK_NULL_HANDLE
            : static_opaque_top_level_acceleration_structure->get_acceleration_structure();
    return build_job;
}

} // namespace WhiteBirdEngine
