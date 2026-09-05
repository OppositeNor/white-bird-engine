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
#include "core/memory/reference_strong.hh"
#include "function/function_utils.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_plugin_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_acceleration_structure_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/ext/matrix_float4x4.hpp>
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

struct RayTracingPluginVK::StaticOpaqueBottomLevelBuild {
    VkAccelerationStructureGeometryKHR geometry{};
    VkAccelerationStructureBuildRangeInfoKHR build_range{};
    uint32_t primitive_count = 0;
    std::vector<VkTransformMatrixKHR> instance_transforms;
    Ref<ROAccelerationStructureVK> acceleration_structure = MEM_NULL;
};

struct RayTracingPluginVK::StaticOpaqueBuildData {
    std::vector<StaticOpaqueBottomLevelBuild> bottom_level_builds;
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

StaticGeometryBufferUsageVK RayTracingPluginVK::get_static_geometry_buffer_usage() const {
    WBE_DEBUG_ASSERT(enabled);
    return StaticGeometryBufferUsageVK{
        .vertex_buffer_usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .index_buffer_usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };
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

static VkTransformMatrixKHR build_vk_transform(const glm::mat4& p_transform) {
    VkTransformMatrixKHR result{};
    result.matrix[0][0] = p_transform[0][0];
    result.matrix[0][1] = p_transform[1][0];
    result.matrix[0][2] = p_transform[2][0];
    result.matrix[0][3] = p_transform[3][0];
    result.matrix[1][0] = p_transform[0][1];
    result.matrix[1][1] = p_transform[1][1];
    result.matrix[1][2] = p_transform[2][1];
    result.matrix[1][3] = p_transform[3][1];
    result.matrix[2][0] = p_transform[0][2];
    result.matrix[2][1] = p_transform[1][2];
    result.matrix[2][2] = p_transform[2][2];
    result.matrix[2][3] = p_transform[3][2];
    return result;
}

RayTracingPluginVK::StaticOpaqueBuildData RayTracingPluginVK::build_static_opaque_data(
    RefRes<RUStaticOpaqueSet> p_set, VkDeviceAddress p_vertex_buffer_address, VkDeviceAddress p_index_buffer_address) const {
    if (p_set == nullptr) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: static set is missing.");
    }
    if (p_set->submesh_count > 0 && p_set->submeshes == nullptr) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: submesh storage is missing.");
    }
    if (p_set->instance_count > 0 && p_set->instances == nullptr) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: instance storage is missing.");
    }

    StaticOpaqueBuildData result;
    result.bottom_level_builds.reserve(p_set->submesh_count);
    for (size_t submesh_index = 0; submesh_index < p_set->submesh_count; ++submesh_index) {
        const RUStaticGeometrySubmesh& submesh = p_set->submeshes.get()[submesh_index];
        if (submesh.indices.index_count == 0 || submesh.instance_count == 0) {
            continue;
        }
        if (submesh.indices.index_count % 3 != 0) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: index count must form complete triangles.");
        }
        if (submesh.vertices.attribute_count == 0 || submesh.vertices.attributes == nullptr) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: vertex attributes are missing.");
        }
        if (submesh.vertices.vertex_stride == 0 ||
            submesh.vertices.binary.size != submesh.vertices.vertex_count * submesh.vertices.vertex_stride) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid vertex range.");
        }
        if (submesh.indices.binary.start % sizeof(uint32_t) != 0 ||
            submesh.indices.binary.size != submesh.indices.index_count * sizeof(uint32_t)) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid index range.");
        }
        if (submesh.first_instance > p_set->instance_count || submesh.instance_count > p_set->instance_count - submesh.first_instance) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: instance range is out of bounds.");
        }

        std::span<const RUVertexView::Attribute> attributes(submesh.vertices.attributes.get(), submesh.vertices.attribute_count);
        auto position_attribute = std::ranges::find_if(attributes, [](const RUVertexView::Attribute& p_attribute) {
            return p_attribute.role.get_name() == "position";
        });
        if (position_attribute == attributes.end()) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: a submesh has no position attribute.");
        }
        if (position_attribute->format != RUVertexAttributeFormat::FLOAT3) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: position attributes must use FLOAT3 format.");
        }
        if (submesh.vertices.vertex_count == 0 || submesh.vertices.vertex_count - 1 > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: vertex count exceeds uint32.");
        }

        StaticOpaqueBottomLevelBuild build;
        build.geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        build.geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        build.geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        build.geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        build.geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        build.geometry.geometry.triangles.vertexData.deviceAddress =
            p_vertex_buffer_address + submesh.vertices.binary.start + position_attribute->offset;
        build.geometry.geometry.triangles.vertexStride = submesh.vertices.vertex_stride;
        build.geometry.geometry.triangles.maxVertex = static_cast<uint32_t>(submesh.vertices.vertex_count - 1);
        build.geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        build.geometry.geometry.triangles.indexData.deviceAddress = p_index_buffer_address + submesh.indices.binary.start;
        build.primitive_count = static_cast<uint32_t>(submesh.indices.index_count / 3);
        build.build_range = VkAccelerationStructureBuildRangeInfoKHR{.primitiveCount = build.primitive_count};
        build.instance_transforms.reserve(submesh.instance_count);
        for (size_t instance_index = 0; instance_index < submesh.instance_count; ++instance_index) {
            build.instance_transforms.push_back(build_vk_transform(p_set->instances.get()[submesh.first_instance + instance_index].global_transform));
        }
        result.bottom_level_builds.push_back(std::move(build));
    }
    return result;
}

GPUJobVK RayTracingPluginVK::create_static_opaque_build_job(StaticOpaqueBuildData&& p_build_data) {
    if (p_build_data.bottom_level_builds.empty()) {
        static_opaque_bottom_level_acceleration_structures.clear();
        static_opaque_top_level_acceleration_structure = MEM_NULL;
        static_opaque_top_level_instance_buffer = MEM_NULL;
        return nullptr;
    }
    const VkDeviceSize scratch_alignment = render_data->min_acceleration_structure_scratch_offset_alignment;
    if (scratch_alignment == 0) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid scratch-buffer size or alignment.");
    }

    std::vector<Ref<ROAccelerationStructureVK>> bottom_level_acceleration_structures;
    bottom_level_acceleration_structures.reserve(p_build_data.bottom_level_builds.size());
    VkDeviceSize max_bottom_level_scratch_size = 1;
    size_t total_instance_count = 0;
    for (StaticOpaqueBottomLevelBuild& build : p_build_data.bottom_level_builds) {
        std::array<uint32_t, 1> primitive_counts{build.primitive_count};
        VkAccelerationStructureBuildGeometryInfoKHR bottom_level_build_info =
            make_bottom_level_build_info(std::span<const VkAccelerationStructureGeometryKHR>(&build.geometry, 1));
        VkAccelerationStructureBuildSizesInfoKHR bottom_level_build_sizes = get_build_sizes(render_data, bottom_level_build_info, primitive_counts);
        if (bottom_level_build_info.geometryCount > render_data->max_acceleration_structure_geometry_count ||
            build.primitive_count > render_data->max_acceleration_structure_primitive_count) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: device geometry limits exceeded.");
        }
        if (bottom_level_build_sizes.buildScratchSize > std::numeric_limits<VkDeviceSize>::max() - scratch_alignment + 1) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid scratch-buffer size or alignment.");
        }
        build.acceleration_structure = make_ref<ROAccelerationStructureVK>(render_data->allocator,
            render_data,
            bottom_level_build_sizes.accelerationStructureSize,
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
        bottom_level_acceleration_structures.push_back(build.acceleration_structure);
        VkDeviceSize required_scratch_size = std::max(bottom_level_build_sizes.buildScratchSize, static_cast<VkDeviceSize>(1));
        max_bottom_level_scratch_size = std::max(max_bottom_level_scratch_size, required_scratch_size);
        total_instance_count += build.instance_transforms.size();
    }

    if (total_instance_count == 0 || total_instance_count > render_data->max_acceleration_structure_primitive_count) {
        throw std::runtime_error("Failed to build static-opaque top-level acceleration structure: device geometry limits exceeded.");
    }

    Ref<ROBufferVK> bottom_level_scratch_buffer =
        ROBufferVK::Builder()
            .set_domain(DeviceDomain::DEVICE)
            .set_size(max_bottom_level_scratch_size + scratch_alignment - 1)
            .set_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            .create_ref(render_data);
    const VkDeviceAddress bottom_level_scratch_buffer_address = get_buffer_device_address(bottom_level_scratch_buffer);
    const VkDeviceAddress aligned_bottom_level_scratch_buffer_address =
        ((bottom_level_scratch_buffer_address + scratch_alignment - 1) / scratch_alignment) * scratch_alignment;

    std::vector<VkAccelerationStructureInstanceKHR> top_level_instances;
    top_level_instances.reserve(total_instance_count);
    for (const StaticOpaqueBottomLevelBuild& build : p_build_data.bottom_level_builds) {
        const uint64_t acceleration_structure_address = get_acceleration_structure_device_address(build.acceleration_structure);
        for (const VkTransformMatrixKHR& transform : build.instance_transforms) {
            VkAccelerationStructureInstanceKHR instance{};
            instance.transform = transform;
            instance.instanceCustomIndex = 0;
            instance.mask = 0xFF;
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance.accelerationStructureReference = acceleration_structure_address;
            top_level_instances.push_back(instance);
        }
    }

    Ref<ROBufferVK> top_level_instance_buffer =
        ROBufferVK::Builder()
            .set_domain(DeviceDomain::HOST)
            .set_size(sizeof(VkAccelerationStructureInstanceKHR) * top_level_instances.size())
            .set_usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            .create_ref(render_data);
    top_level_instance_buffer->update_buffer(top_level_instances.data(), sizeof(VkAccelerationStructureInstanceKHR) * top_level_instances.size());

    VkAccelerationStructureGeometryKHR top_level_geometry{};
    top_level_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    top_level_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    top_level_geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    top_level_geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    top_level_geometry.geometry.instances.data.deviceAddress = get_buffer_device_address(top_level_instance_buffer);

    std::array<uint32_t, 1> top_level_primitive_counts{static_cast<uint32_t>(top_level_instances.size())};
    VkAccelerationStructureBuildGeometryInfoKHR top_level_sizing_info = make_top_level_build_info(&top_level_geometry);
    VkAccelerationStructureBuildSizesInfoKHR top_level_build_sizes =
        get_build_sizes(render_data, top_level_sizing_info, top_level_primitive_counts);
    if (top_level_sizing_info.geometryCount > render_data->max_acceleration_structure_geometry_count ||
        top_level_instances.size() > render_data->max_acceleration_structure_primitive_count) {
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
    const VkDeviceAddress top_level_scratch_buffer_address = get_buffer_device_address(top_level_scratch_buffer);
    const VkDeviceAddress aligned_top_level_scratch_buffer_address =
        ((top_level_scratch_buffer_address + scratch_alignment - 1) / scratch_alignment) * scratch_alignment;

    const std::array<VkAccelerationStructureBuildRangeInfoKHR, 1> top_level_build_ranges{
        VkAccelerationStructureBuildRangeInfoKHR{.primitiveCount = top_level_primitive_counts[0]}};

    const auto command_build_acceleration_structures = render_data->command_build_acceleration_structures;
    GPUJobVK build_job = [bottom_level_acceleration_structures,
                             bottom_level_scratch_buffer,
                             bottom_level_builds = std::move(p_build_data.bottom_level_builds),
                             aligned_bottom_level_scratch_buffer_address,
                             top_level_acceleration_structure,
                             top_level_instance_buffer,
                             top_level_scratch_buffer,
                             top_level_geometry,
                             aligned_top_level_scratch_buffer_address,
                             top_level_build_ranges,
                             command_build_acceleration_structures](VkCommandBuffer p_command_buffer) mutable {
        // Keep all build inputs, scratch storage, and acceleration structures alive until the queued build completes.
        (void)bottom_level_acceleration_structures;
        (void)bottom_level_scratch_buffer;
        (void)top_level_acceleration_structure;
        (void)top_level_instance_buffer;
        (void)top_level_scratch_buffer;
        for (size_t build_index = 0; build_index < bottom_level_builds.size(); ++build_index) {
            VkAccelerationStructureBuildGeometryInfoKHR bottom_level_build_info =
                make_bottom_level_build_info(std::span<const VkAccelerationStructureGeometryKHR>(&bottom_level_builds[build_index].geometry, 1));
            bottom_level_build_info.dstAccelerationStructure = bottom_level_builds[build_index].acceleration_structure->get_acceleration_structure();
            bottom_level_build_info.scratchData.deviceAddress = aligned_bottom_level_scratch_buffer_address;
            const VkAccelerationStructureBuildRangeInfoKHR* bottom_level_build_range_pointer = &bottom_level_builds[build_index].build_range;
            command_build_acceleration_structures(p_command_buffer, 1, &bottom_level_build_info, &bottom_level_build_range_pointer);
            if (build_index + 1 < bottom_level_builds.size()) {
                VkMemoryBarrier2 between_blas_barrier{};
                between_blas_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                between_blas_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
                between_blas_barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                between_blas_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
                between_blas_barrier.dstAccessMask =
                    VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                VkDependencyInfo between_blas_dependency_info{};
                between_blas_dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                between_blas_dependency_info.memoryBarrierCount = 1;
                between_blas_dependency_info.pMemoryBarriers = &between_blas_barrier;
                vkCmdPipelineBarrier2(p_command_buffer, &between_blas_dependency_info);
            }
        }

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

        VkAccelerationStructureBuildGeometryInfoKHR top_level_build_info = make_top_level_build_info(&top_level_geometry);
        top_level_build_info.dstAccelerationStructure = top_level_acceleration_structure->get_acceleration_structure();
        top_level_build_info.scratchData.deviceAddress = aligned_top_level_scratch_buffer_address;
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
    static_opaque_bottom_level_acceleration_structures = bottom_level_acceleration_structures;
    static_opaque_top_level_acceleration_structure = top_level_acceleration_structure;
    static_opaque_top_level_instance_buffer = top_level_instance_buffer;
    return build_job;
}

GPUJobVK RayTracingPluginVK::update_static_opaque_acceleration_structure(
    RefRes<RUStaticOpaqueSet> p_set, Ref<ROBufferVK> p_vertex_buffer, Ref<ROBufferVK> p_index_buffer) {
    if (p_set == nullptr || p_vertex_buffer == MEM_NULL || p_index_buffer == MEM_NULL) {
        static_opaque_bottom_level_acceleration_structures.clear();
        static_opaque_top_level_acceleration_structure = MEM_NULL;
        static_opaque_top_level_instance_buffer = MEM_NULL;
        return nullptr;
    }

    StaticOpaqueBuildData build_data =
        build_static_opaque_data(p_set, get_buffer_device_address(p_vertex_buffer), get_buffer_device_address(p_index_buffer));
    if (build_data.bottom_level_builds.empty()) {
        static_opaque_bottom_level_acceleration_structures.clear();
        static_opaque_top_level_acceleration_structure = MEM_NULL;
        static_opaque_top_level_instance_buffer = MEM_NULL;
        return nullptr;
    }
    return create_static_opaque_build_job(std::move(build_data));
}

GPUJobVK RayTracingPluginVK::on_static_opaque_set_updated(BaseRenderDataVK* p_render_data,
    RefRes<RUStaticOpaqueSet> p_set,
    Ref<ROBufferVK> p_vertex_buffer,
    Ref<ROBufferVK> p_index_buffer) {
    WBE_DEBUG_ASSERT(enabled);

    render_data = p_render_data;
    GPUJobVK build_job = update_static_opaque_acceleration_structure(p_set, p_vertex_buffer, p_index_buffer);
    p_render_data->static_opaque_acceleration_structure =
        static_opaque_top_level_acceleration_structure == MEM_NULL
            ? VK_NULL_HANDLE
            : static_opaque_top_level_acceleration_structure->get_acceleration_structure();
    return build_job;
}

} // namespace WhiteBirdEngine
