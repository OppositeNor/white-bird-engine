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
#include "core/memory/unique.hh"
#include "function/function_utils.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/descriptor_heap_allocator_vk.hh"
#include "function/render_vk/render_plugin_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/render_vk_utils_private.hh"
#include "function/render_vk/ro_acceleration_structure_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_heap_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

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

struct RayTracingPluginVK::StaticOpaqueTopLevelBuild {
    Ref<ROBufferVK> instance_buffer = MEM_NULL;
    VkAccelerationStructureGeometryKHR geometry{};
    Ref<ROAccelerationStructureVK> acceleration_structure = MEM_NULL;
    Ref<ROBufferVK> scratch_buffer = MEM_NULL;
    VkDeviceAddress aligned_scratch_buffer_address = 0;
    std::array<VkAccelerationStructureBuildRangeInfoKHR, 1> build_ranges{};
};

VkTransformMatrixKHR build_vk_transform(const glm::mat4& p_transform);

namespace {

constexpr uint32_t ACCELERATION_STRUCTURE_DESCRIPTOR_CAPACITY = 16;

VkAccelerationStructureBuildGeometryInfoKHR make_bottom_level_build_info(std::span<const VkAccelerationStructureGeometryKHR> p_geometries) {
    VkAccelerationStructureBuildGeometryInfoKHR result{};
    result.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    result.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    result.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    result.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    result.geometryCount = static_cast<uint32_t>(p_geometries.size());
    result.pGeometries = p_geometries.data();
    return result;
}

VkAccelerationStructureBuildGeometryInfoKHR make_top_level_build_info(const VkAccelerationStructureGeometryKHR* p_geometry) {
    VkAccelerationStructureBuildGeometryInfoKHR result{};
    result.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    result.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    result.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    result.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    result.geometryCount = 1;
    result.pGeometries = p_geometry;
    return result;
}

VkAccelerationStructureBuildSizesInfoKHR get_build_sizes(const BaseRenderDataVK* p_render_data,
    const BaseRayTracingDataVK* p_base_ray_tracing_data,
    const VkAccelerationStructureBuildGeometryInfoKHR& p_build_info,
    std::span<const uint32_t> p_primitive_counts) {
    VkAccelerationStructureBuildSizesInfoKHR result{};
    result.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    p_base_ray_tracing_data->get_acceleration_structure_build_sizes(
        p_render_data->logic_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &p_build_info, p_primitive_counts.data(), &result);
    return result;
}

} // namespace

RayTracingPluginVK::RayTracingPluginVK(const BaseRenderDataVK* p_render_data)
    : render_data(p_render_data), base_ray_tracing_data(make_unique<BaseRayTracingDataVK>(p_render_data->allocator)) {
    singleton = this;
}

RayTracingPluginVK::~RayTracingPluginVK() {
    singleton = nullptr;
}

bool RayTracingPluginVK::supports_base_features(
    VkPhysicalDevice p_physical_device, const char* const* p_enabled_layers, size_t p_enabled_layer_count) const {
    static constexpr std::array<const char*, 2> REQUIRED_BASE_EXTENSIONS{
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    };
    // Both paths need acceleration structures and deferred host operations.
    if (!check_extension_support(REQUIRED_BASE_EXTENSIONS,
            std::span<const char* const>(p_enabled_layers, p_enabled_layer_count),
            [p_physical_device](const char* p_layer_name, uint32_t* p_extension_count, VkExtensionProperties* p_extension_properties) {
                return vkEnumerateDeviceExtensionProperties(p_physical_device, p_layer_name, p_extension_count, p_extension_properties);
            })) {
        return false;
    }

    // Static mesh BLAS builds read three-component float positions from vertex buffers.
    VkFormatProperties position_format_properties{};
    vkGetPhysicalDeviceFormatProperties(p_physical_device, VK_FORMAT_R32G32B32_SFLOAT, &position_format_properties);
    if ((position_format_properties.bufferFeatures & VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR) == 0) {
        return false;
    }

    // Acceleration-structure builds require device addresses and the base AS feature.
    if (!check_device_feature_support<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES>(
            p_physical_device, &VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress) ||
        !check_device_feature_support<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>(
            p_physical_device, &VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure)) {
        return false;
    }

    // Descriptor-heap acceleration-structure loads generated by Slang use 64-bit integers.
    VkPhysicalDeviceFeatures supported_device_features{};
    vkGetPhysicalDeviceFeatures(p_physical_device, &supported_device_features);
    return supported_device_features.shaderInt64 == VK_TRUE;
}

bool RayTracingPluginVK::supports_ray_query(
    VkPhysicalDevice p_physical_device, const char* const* p_enabled_layers, size_t p_enabled_layer_count) const {
    static constexpr std::array<const char*, 1> REQUIRED_RAY_QUERY_EXTENSION{VK_KHR_RAY_QUERY_EXTENSION_NAME};
    return check_extension_support(REQUIRED_RAY_QUERY_EXTENSION,
               std::span<const char* const>(p_enabled_layers, p_enabled_layer_count),
               [p_physical_device](const char* p_layer_name, uint32_t* p_extension_count, VkExtensionProperties* p_extension_properties) {
                   return vkEnumerateDeviceExtensionProperties(p_physical_device, p_layer_name, p_extension_count, p_extension_properties);
               }) &&
           check_device_feature_support<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR>(
               p_physical_device, &VkPhysicalDeviceRayQueryFeaturesKHR::rayQuery);
}

bool RayTracingPluginVK::supports_ray_tracing_pipeline(
    VkPhysicalDevice p_physical_device, const char* const* p_enabled_layers, size_t p_enabled_layer_count) const {
    static constexpr std::array<const char*, 1> REQUIRED_RAY_TRACING_PIPELINE_EXTENSION{VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};
    return check_extension_support(REQUIRED_RAY_TRACING_PIPELINE_EXTENSION,
               std::span<const char* const>(p_enabled_layers, p_enabled_layer_count),
               [p_physical_device](const char* p_layer_name, uint32_t* p_extension_count, VkExtensionProperties* p_extension_properties) {
                   return vkEnumerateDeviceExtensionProperties(p_physical_device, p_layer_name, p_extension_count, p_extension_properties);
               }) &&
           check_device_feature_support<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR>(
               p_physical_device, &VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline);
}

void RayTracingPluginVK::prepare_device_features(const RendererCapabilities& p_capabilities) {
    // Link only supported ray features into the device-creation feature chain.
    buffer_device_address_features = {};
    buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    buffer_device_address_features.bufferDeviceAddress = VK_TRUE;
    acceleration_structure_features = {};
    acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    acceleration_structure_features.accelerationStructure = VK_TRUE;
    ray_query_features = {};
    ray_query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    ray_query_features.rayQuery = p_capabilities.ray_query ? VK_TRUE : VK_FALSE;
    ray_tracing_pipeline_features = {};
    ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    ray_tracing_pipeline_features.rayTracingPipeline = p_capabilities.ray_tracing ? VK_TRUE : VK_FALSE;
    buffer_device_address_features.pNext = &acceleration_structure_features;
    acceleration_structure_features.pNext = &ray_query_features;
    ray_query_features.pNext = &ray_tracing_pipeline_features;
}

bool RayTracingPluginVK::try_enable(
    RendererCapabilities& p_capabilities, VkPhysicalDevice p_physical_device, const char* const* p_enabled_layers, size_t p_enabled_layer_count) {
    enabled = false;
    ray_query_enabled = false;
    ray_tracing_pipeline_enabled = false;
    // Skip the plugin when neither ray execution path was requested.
    if (!p_capabilities.ray_tracing && !p_capabilities.ray_query) {
        return false;
    }
    if (!supports_base_features(p_physical_device, p_enabled_layers, p_enabled_layer_count)) {
        // Shared acceleration-structure requirements disable both ray execution paths.
        p_capabilities.ray_tracing = false;
        p_capabilities.ray_query = false;
        return false;
    }

    if (p_capabilities.ray_query) {
        // Enable inline ray traversal only when its extension and feature are available.
        ray_query_enabled = supports_ray_query(p_physical_device, p_enabled_layers, p_enabled_layer_count);
        p_capabilities.ray_query = ray_query_enabled;
    }
    if (p_capabilities.ray_tracing) {
        // Check ray-tracing pipelines independently from inline ray queries.
        ray_tracing_pipeline_enabled = supports_ray_tracing_pipeline(p_physical_device, p_enabled_layers, p_enabled_layer_count);
        p_capabilities.ray_tracing = ray_tracing_pipeline_enabled;
    }
    if (!p_capabilities.ray_tracing && !p_capabilities.ray_query) {
        return false;
    }

    prepare_device_features(p_capabilities);
    enabled = true;
    return true;
}

std::span<const char* const> RayTracingPluginVK::get_required_instance_extensions() const {
    return {};
}

std::span<const char* const> RayTracingPluginVK::get_required_device_extensions() const {
    WBE_DEBUG_ASSERT(enabled);
    static thread_local std::vector<const char*> required_extensions;
    required_extensions = {
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    };
    if (ray_query_enabled) {
        // Request the ray-query device extension only for inline ray traversal.
        required_extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    }
    if (ray_tracing_pipeline_enabled) {
        // Request the pipeline extension only when that separate path is enabled.
        required_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    }
    return required_extensions;
}

void RayTracingPluginVK::on_before_instance_creation(VkInstanceCreateInfo* /*p_create_info*/) {
}

void RayTracingPluginVK::on_after_instance_created(VkInstance p_instance) {
    vk_instance = p_instance;
}

void RayTracingPluginVK::on_before_device_creation(
    VkPhysicalDevice p_physical_device, VkDeviceCreateInfo* p_create_info, VkPhysicalDeviceFeatures* p_device_features) {
    WBE_DEBUG_ASSERT(enabled);
    WBE_DEBUG_ASSERT(p_device_features != nullptr);
    // Slang's acceleration-structure heap access requires 64-bit integer operations.
    p_device_features->shaderInt64 = VK_TRUE;
    // Add the shared acceleration-structure dependencies to the device feature chain.
    enable_device_feature<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES>(
        p_physical_device, p_create_info, &buffer_device_address_features, &VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress);
    enable_device_feature<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>(
        p_physical_device, p_create_info, &acceleration_structure_features, &VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure);
    if (ray_query_enabled) {
        // Request inline ray traversal for compute and graphics shaders.
        enable_device_feature<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR>(
            p_physical_device, p_create_info, &ray_query_features, &VkPhysicalDeviceRayQueryFeaturesKHR::rayQuery);
    }
    if (ray_tracing_pipeline_enabled) {
        // Request ray-generation and hit shader stages for ray-tracing pipelines.
        enable_device_feature<VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR>(
            p_physical_device, p_create_info, &ray_tracing_pipeline_features, &VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline);
    }
}

void RayTracingPluginVK::on_after_device_created(
    BaseRenderDataVK* /*p_render_data*/, VkPhysicalDevice /*p_physical_device*/, VkDevice p_logical_device) {
    WBE_DEBUG_ASSERT(enabled);

    // Resolve the Vulkan acceleration-structure entry points after logical-device creation.
    base_ray_tracing_data->create_acceleration_structure = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(p_logical_device, "vkCreateAccelerationStructureKHR"));
    base_ray_tracing_data->destroy_acceleration_structure = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(p_logical_device, "vkDestroyAccelerationStructureKHR"));
    base_ray_tracing_data->get_acceleration_structure_build_sizes = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(p_logical_device, "vkGetAccelerationStructureBuildSizesKHR"));
    base_ray_tracing_data->command_build_acceleration_structures = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(p_logical_device, "vkCmdBuildAccelerationStructuresKHR"));
    base_ray_tracing_data->get_acceleration_structure_device_address = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(p_logical_device, "vkGetAccelerationStructureDeviceAddressKHR"));
    if (base_ray_tracing_data->create_acceleration_structure == nullptr || base_ray_tracing_data->destroy_acceleration_structure == nullptr ||
        base_ray_tracing_data->get_acceleration_structure_build_sizes == nullptr ||
        base_ray_tracing_data->command_build_acceleration_structures == nullptr ||
        base_ray_tracing_data->get_acceleration_structure_device_address == nullptr) {
        throw std::runtime_error("Failed to load Vulkan acceleration-structure commands.");
    }
}

void RayTracingPluginVK::update_base_render_data(BaseRenderDataVK* p_render_data, VkPhysicalDevice p_physical_device) {
    WBE_DEBUG_ASSERT(enabled);

    // Use the descriptor-heap type-specific size for the acceleration-structure arena.
    auto get_descriptor_size = reinterpret_cast<PFN_vkGetPhysicalDeviceDescriptorSizeEXT>(
        vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceDescriptorSizeEXT"));
    if (get_descriptor_size == nullptr) {
        throw std::runtime_error("Failed to load vkGetPhysicalDeviceDescriptorSizeEXT.");
    }
    base_ray_tracing_data->acceleration_structure_descriptor_size =
        get_descriptor_size(p_physical_device, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    if (base_ray_tracing_data->acceleration_structure_descriptor_size == 0 ||
        base_ray_tracing_data->acceleration_structure_descriptor_size > p_render_data->descriptor_heap_properties.bufferDescriptorSize) {
        throw std::runtime_error("Invalid acceleration-structure descriptor size reported by the device.");
    }

    // Cache limits needed to size and align BLAS and TLAS builds.
    VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties{};
    acceleration_structure_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 physical_device_properties{};
    physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physical_device_properties.pNext = &acceleration_structure_properties;
    vkGetPhysicalDeviceProperties2(p_physical_device, &physical_device_properties);
    base_ray_tracing_data->min_acceleration_structure_scratch_offset_alignment =
        acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment;
    base_ray_tracing_data->max_acceleration_structure_geometry_count = acceleration_structure_properties.maxGeometryCount;
    base_ray_tracing_data->max_acceleration_structure_primitive_count = acceleration_structure_properties.maxPrimitiveCount;
}

void RayTracingPluginVK::configure_descriptor_heaps(
    const BaseRenderDataVK* p_render_data, std::span<DescriptorHeapConfigurationVK> p_configurations) {
    WBE_DEBUG_ASSERT(enabled);
    auto resource_configuration = std::ranges::find_if(p_configurations, [](const DescriptorHeapConfigurationVK& p_configuration) {
        return p_configuration.type == DescriptorHeapTypeVK::RESOURCE;
    });
    if (resource_configuration == p_configurations.end()) {
        throw std::runtime_error("Failed to reserve acceleration-structure descriptors: resource heap is missing.");
    }
    // Extend the shared resource heap with a tightly packed acceleration-structure arena.
    acceleration_structure_descriptor_byte_offset = resource_configuration->reserve_arena_bytes(ACCELERATION_STRUCTURE_DESCRIPTOR_CAPACITY,
        base_ray_tracing_data->acceleration_structure_descriptor_size,
        p_render_data->descriptor_heap_properties.bufferDescriptorAlignment);
}

void RayTracingPluginVK::create_descriptor_arenas(BaseRenderDataVK* p_render_data) {
    WBE_DEBUG_ASSERT(enabled);
    if (p_render_data->descriptor_heap_allocator == nullptr) {
        throw std::runtime_error("Failed to create acceleration-structure arena: descriptor heap allocator is missing.");
    }
    // Keep the plugin-owned arena handle with the rest of the ray-tracing state.
    const DescriptorHeapIDVK heap_id = p_render_data->resource_descriptor_heap_id;
    const DescriptorArenaIDVK arena_id = p_render_data->descriptor_heap_allocator->allocate_arena(heap_id,
        DescriptorArenaConfigurationVK{
            .byte_offset = acceleration_structure_descriptor_byte_offset,
            .descriptor_stride = base_ray_tracing_data->acceleration_structure_descriptor_size,
            .descriptor_count = ACCELERATION_STRUCTURE_DESCRIPTOR_CAPACITY,
            .descriptor_alignment = p_render_data->descriptor_heap_properties.bufferDescriptorAlignment,
        });
    base_ray_tracing_data->acceleration_structure_descriptor_arena = {.heap_id = heap_id, .arena_id = arena_id};
}

StaticGeometryBufferUsageVK RayTracingPluginVK::get_static_geometry_buffer_usage() const {
    WBE_DEBUG_ASSERT(enabled);
    return StaticGeometryBufferUsageVK{
        .vertex_buffer_usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .index_buffer_usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };
}

void RayTracingPluginVK::reset_static_opaque_acceleration_structures() {
    // Drop the previous static scene while retaining its container for existing graph bindings.
    static_opaque_bottom_level_acceleration_structures.clear();
    static_opaque_top_level_acceleration_structure = MEM_NULL;
    static_opaque_top_level_instance_buffer = MEM_NULL;
    if (static_opaque_top_level_acceleration_structure_container != MEM_NULL) {
        static_opaque_top_level_acceleration_structure_container->clear();
    }
}

void RayTracingPluginVK::validate_static_opaque_set(RefRes<RUStaticGeometrySet> p_set) const {
    if (p_set == nullptr) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: static set is missing.");
    }
    if (p_set->submesh_count > 0 && p_set->submeshes == nullptr) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: submesh storage is missing.");
    }
    if (p_set->instance_count > 0 && p_set->instances == nullptr) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: instance storage is missing.");
    }
}

void RayTracingPluginVK::validate_static_opaque_submesh(const RUStaticGeometrySubmesh& p_submesh, size_t p_instance_count) const {
    if (p_submesh.indices.index_count % 3 != 0) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: index count must form complete triangles.");
    }
    if (p_submesh.vertices.attribute_count == 0 || p_submesh.vertices.attributes == nullptr) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: vertex attributes are missing.");
    }
    if (p_submesh.vertices.vertex_stride == 0 ||
        p_submesh.vertices.binary.size != p_submesh.vertices.vertex_count * p_submesh.vertices.vertex_stride) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid vertex range.");
    }
    if (p_submesh.indices.binary.start % sizeof(uint32_t) != 0 ||
        p_submesh.indices.binary.size != p_submesh.indices.index_count * sizeof(uint32_t)) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid index range.");
    }
    if (p_submesh.first_instance > p_instance_count || p_submesh.instance_count > p_instance_count - p_submesh.first_instance) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: instance range is out of bounds.");
    }
    if (p_submesh.vertices.vertex_count == 0 || p_submesh.vertices.vertex_count - 1 > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: vertex count exceeds uint32.");
    }
}

const RUVertexView::Attribute& RayTracingPluginVK::get_static_opaque_position_attribute(const RUStaticGeometrySubmesh& p_submesh) const {
    std::span<const RUVertexView::Attribute> attributes(p_submesh.vertices.attributes.get(), p_submesh.vertices.attribute_count);
    auto position_attribute = std::ranges::find_if(attributes, [](const RUVertexView::Attribute& p_attribute) {
        return p_attribute.role.get_name() == "position";
    });
    if (position_attribute == attributes.end()) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: a submesh has no position attribute.");
    }
    if (position_attribute->format != RUVertexAttributeFormat::FLOAT3) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: position attributes must use FLOAT3 format.");
    }
    return *position_attribute;
}

RayTracingPluginVK::StaticOpaqueBottomLevelBuild RayTracingPluginVK::create_static_opaque_bottom_level_build(
    const RUStaticGeometrySubmesh& p_submesh,
    RefRes<RUStaticGeometrySet> p_set,
    VkDeviceAddress p_vertex_buffer_address,
    VkDeviceAddress p_index_buffer_address) const {
    validate_static_opaque_submesh(p_submesh, p_set->instance_count);
    const RUVertexView::Attribute& position_attribute = get_static_opaque_position_attribute(p_submesh);

    StaticOpaqueBottomLevelBuild build;
    build.geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    build.geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    build.geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    build.geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    build.geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    build.geometry.geometry.triangles.vertexData.deviceAddress =
        p_vertex_buffer_address + p_submesh.vertices.binary.start + position_attribute.offset;
    build.geometry.geometry.triangles.vertexStride = p_submesh.vertices.vertex_stride;
    build.geometry.geometry.triangles.maxVertex = static_cast<uint32_t>(p_submesh.vertices.vertex_count - 1);
    build.geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    build.geometry.geometry.triangles.indexData.deviceAddress = p_index_buffer_address + p_submesh.indices.binary.start;
    build.primitive_count = static_cast<uint32_t>(p_submesh.indices.index_count / 3);
    build.build_range = VkAccelerationStructureBuildRangeInfoKHR{.primitiveCount = build.primitive_count};
    // Keep each instance transform for the TLAS after this submesh's BLAS is built.
    build.instance_transforms.reserve(p_submesh.instance_count);
    for (size_t instance_index = 0; instance_index < p_submesh.instance_count; ++instance_index) {
        build.instance_transforms.push_back(
            build_vk_transform(p_set->instances.get()[p_submesh.first_instance + instance_index].global_transform));
    }
    return build;
}

Ref<ROBufferVK> RayTracingPluginVK::create_aligned_scratch_buffer(
    VkDeviceSize p_required_size, VkDeviceSize p_alignment, VkDeviceAddress& p_aligned_buffer_address) const {
    Ref<ROBufferVK> scratch_buffer = ROBufferVK::Builder()
                                         .set_domain(DeviceDomain::DEVICE)
                                         .set_size(p_required_size + p_alignment - 1)
                                         .set_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
                                         .create_ref(render_data);
    const VkDeviceAddress scratch_buffer_address = scratch_buffer->get_device_address();
    p_aligned_buffer_address = ((scratch_buffer_address + p_alignment - 1) / p_alignment) * p_alignment;
    return scratch_buffer;
}

void RayTracingPluginVK::initialize_bottom_level_builds(StaticOpaqueBuildData& p_build_data,
    std::vector<Ref<ROAccelerationStructureVK>>& p_bottom_level_acceleration_structures,
    VkDeviceSize& p_max_bottom_level_scratch_size,
    size_t& p_total_instance_count) const {
    p_bottom_level_acceleration_structures.reserve(p_build_data.bottom_level_builds.size());
    p_max_bottom_level_scratch_size = 1;
    p_total_instance_count = 0;

    // Size one BLAS per populated submesh and track the largest reusable scratch allocation.
    for (StaticOpaqueBottomLevelBuild& build : p_build_data.bottom_level_builds) {
        std::array<uint32_t, 1> primitive_counts{build.primitive_count};
        VkAccelerationStructureBuildGeometryInfoKHR bottom_level_build_info =
            make_bottom_level_build_info(std::span<const VkAccelerationStructureGeometryKHR>(&build.geometry, 1));
        VkAccelerationStructureBuildSizesInfoKHR bottom_level_build_sizes =
            get_build_sizes(render_data, base_ray_tracing_data.get(), bottom_level_build_info, primitive_counts);
        if (bottom_level_build_info.geometryCount > base_ray_tracing_data->max_acceleration_structure_geometry_count ||
            build.primitive_count > base_ray_tracing_data->max_acceleration_structure_primitive_count) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: device geometry limits exceeded.");
        }
        if (bottom_level_build_sizes.buildScratchSize >
            std::numeric_limits<VkDeviceSize>::max() - base_ray_tracing_data->min_acceleration_structure_scratch_offset_alignment + 1) {
            throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid scratch-buffer size or alignment.");
        }
        build.acceleration_structure = ROAccelerationStructureVK::Builder()
                                           .set_size(bottom_level_build_sizes.accelerationStructureSize)
                                           .set_type(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR)
                                           .create_ref(render_data, base_ray_tracing_data.get());
        p_bottom_level_acceleration_structures.push_back(build.acceleration_structure);
        const VkDeviceSize required_scratch_size = std::max(bottom_level_build_sizes.buildScratchSize, static_cast<VkDeviceSize>(1));
        p_max_bottom_level_scratch_size = std::max(p_max_bottom_level_scratch_size, required_scratch_size);
        p_total_instance_count += build.instance_transforms.size();
    }
}

RayTracingPluginVK::StaticOpaqueTopLevelBuild RayTracingPluginVK::create_top_level_build(
    StaticOpaqueBuildData& p_build_data, VkDeviceSize p_scratch_alignment) const {
    // Expand each BLAS reference into one TLAS instance per static-set transform.
    std::vector<VkAccelerationStructureInstanceKHR> top_level_instances;
    size_t total_instance_count = 0;
    for (const StaticOpaqueBottomLevelBuild& build : p_build_data.bottom_level_builds) {
        total_instance_count += build.instance_transforms.size();
    }
    top_level_instances.reserve(total_instance_count);
    for (const StaticOpaqueBottomLevelBuild& build : p_build_data.bottom_level_builds) {
        const uint64_t acceleration_structure_address = build.acceleration_structure->get_device_address();
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

    // Upload the instance records before querying the TLAS build size.
    StaticOpaqueTopLevelBuild result;
    result.instance_buffer =
        ROBufferVK::Builder()
            .set_domain(DeviceDomain::HOST)
            .set_size(sizeof(VkAccelerationStructureInstanceKHR) * top_level_instances.size())
            .set_usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            .create_ref(render_data);
    result.instance_buffer->update_buffer(
        top_level_instances.data(), sizeof(VkAccelerationStructureInstanceKHR) * top_level_instances.size());

    result.geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    result.geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    result.geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    result.geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    result.geometry.geometry.instances.data.deviceAddress = result.instance_buffer->get_device_address();

    std::array<uint32_t, 1> top_level_primitive_counts{static_cast<uint32_t>(top_level_instances.size())};
    VkAccelerationStructureBuildGeometryInfoKHR top_level_sizing_info = make_top_level_build_info(&result.geometry);
    VkAccelerationStructureBuildSizesInfoKHR top_level_build_sizes =
        get_build_sizes(render_data, base_ray_tracing_data.get(), top_level_sizing_info, top_level_primitive_counts);
    if (top_level_sizing_info.geometryCount > base_ray_tracing_data->max_acceleration_structure_geometry_count ||
        top_level_instances.size() > base_ray_tracing_data->max_acceleration_structure_primitive_count) {
        throw std::runtime_error("Failed to build static-opaque top-level acceleration structure: device geometry limits exceeded.");
    }
    if (top_level_build_sizes.buildScratchSize > std::numeric_limits<VkDeviceSize>::max() - p_scratch_alignment + 1) {
        throw std::runtime_error("Failed to build static-opaque top-level acceleration structure: invalid scratch-buffer size or alignment.");
    }

    result.acceleration_structure = ROAccelerationStructureVK::Builder()
                                        .set_size(top_level_build_sizes.accelerationStructureSize)
                                        .set_type(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
                                        .create_ref(render_data, base_ray_tracing_data.get());
    result.scratch_buffer = create_aligned_scratch_buffer(
        std::max<VkDeviceSize>(top_level_build_sizes.buildScratchSize, 1), p_scratch_alignment, result.aligned_scratch_buffer_address);
    result.build_ranges = {VkAccelerationStructureBuildRangeInfoKHR{.primitiveCount = top_level_primitive_counts[0]}};
    return result;
}

VkTransformMatrixKHR build_vk_transform(const glm::mat4& p_transform) {
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
    RefRes<RUStaticGeometrySet> p_set, VkDeviceAddress p_vertex_buffer_address, VkDeviceAddress p_index_buffer_address) const {
    validate_static_opaque_set(p_set);

    StaticOpaqueBuildData result;
    result.bottom_level_builds.reserve(p_set->submesh_count);
    for (size_t submesh_index = 0; submesh_index < p_set->submesh_count; ++submesh_index) {
        const RUStaticGeometrySubmesh& submesh = p_set->submeshes.get()[submesh_index];
        if (submesh.indices.index_count == 0 || submesh.instance_count == 0) {
            // Empty geometry contributes no BLAS or TLAS instances.
            continue;
        }
        result.bottom_level_builds.push_back(
            create_static_opaque_bottom_level_build(submesh, p_set, p_vertex_buffer_address, p_index_buffer_address));
    }
    return result;
}

GPUJobVK RayTracingPluginVK::create_static_opaque_build_job(StaticOpaqueBuildData&& p_build_data) {
    if (p_build_data.bottom_level_builds.empty()) {
        // An empty static set clears the scene AS without submitting a build.
        reset_static_opaque_acceleration_structures();
        return nullptr;
    }
    const VkDeviceSize scratch_alignment = base_ray_tracing_data->min_acceleration_structure_scratch_offset_alignment;
    if (scratch_alignment == 0) {
        throw std::runtime_error("Failed to build static-opaque acceleration structure: invalid scratch-buffer size or alignment.");
    }

    std::vector<Ref<ROAccelerationStructureVK>> bottom_level_acceleration_structures;
    VkDeviceSize max_bottom_level_scratch_size = 1;
    size_t total_instance_count = 0;
    initialize_bottom_level_builds(p_build_data, bottom_level_acceleration_structures, max_bottom_level_scratch_size, total_instance_count);

    if (total_instance_count == 0 || total_instance_count > base_ray_tracing_data->max_acceleration_structure_primitive_count) {
        throw std::runtime_error("Failed to build static-opaque top-level acceleration structure: device geometry limits exceeded.");
    }

    // Allocate shared BLAS scratch storage and the separate TLAS build inputs.
    VkDeviceAddress aligned_bottom_level_scratch_buffer_address = 0;
    Ref<ROBufferVK> bottom_level_scratch_buffer = create_aligned_scratch_buffer(
        max_bottom_level_scratch_size, scratch_alignment, aligned_bottom_level_scratch_buffer_address);
    StaticOpaqueTopLevelBuild top_level_build = create_top_level_build(p_build_data, scratch_alignment);

    // Publish the new TLAS through the stable container already held by the render graph.
    std::vector<Ref<ROAccelerationStructureVK>> stored_bottom_level_acceleration_structures = bottom_level_acceleration_structures;
    Ref<ROAccelerationStructureVK> stored_top_level_acceleration_structure = top_level_build.acceleration_structure;
    Ref<ROBufferVK> stored_top_level_instance_buffer = top_level_build.instance_buffer;
    static_opaque_bottom_level_acceleration_structures = std::move(stored_bottom_level_acceleration_structures);
    static_opaque_top_level_acceleration_structure = stored_top_level_acceleration_structure;
    static_opaque_top_level_instance_buffer = stored_top_level_instance_buffer;
    if (static_opaque_top_level_acceleration_structure_container == MEM_NULL) {
        // Create the container on the first nonempty static set.
        static_opaque_top_level_acceleration_structure_container =
            make_ref<ROCSingleVK>(render_data->allocator, static_opaque_top_level_acceleration_structure);
    } else {
        // Replace its resource while keeping existing graph bindings valid.
        static_opaque_top_level_acceleration_structure_container->set(static_opaque_top_level_acceleration_structure);
    }
    return create_static_opaque_record_job(std::move(bottom_level_acceleration_structures),
        bottom_level_scratch_buffer,
        std::move(p_build_data),
        aligned_bottom_level_scratch_buffer_address,
        std::move(top_level_build));
}

GPUJobVK RayTracingPluginVK::create_static_opaque_record_job(std::vector<Ref<ROAccelerationStructureVK>>&& p_bottom_level_acceleration_structures,
    Ref<ROBufferVK> p_bottom_level_scratch_buffer,
    StaticOpaqueBuildData&& p_build_data,
    VkDeviceAddress p_aligned_bottom_level_scratch_buffer_address,
    StaticOpaqueTopLevelBuild&& p_top_level_build) const {
    const auto command_build_acceleration_structures = base_ray_tracing_data->command_build_acceleration_structures;
    return [bottom_level_acceleration_structures = std::move(p_bottom_level_acceleration_structures),
               captured_bottom_level_scratch_buffer = p_bottom_level_scratch_buffer,
               bottom_level_builds = std::move(p_build_data.bottom_level_builds),
               aligned_bottom_level_scratch_buffer_address = p_aligned_bottom_level_scratch_buffer_address,
               top_level_acceleration_structure = p_top_level_build.acceleration_structure,
               top_level_instance_buffer = p_top_level_build.instance_buffer,
               top_level_scratch_buffer = p_top_level_build.scratch_buffer,
               top_level_geometry = p_top_level_build.geometry,
               aligned_top_level_scratch_buffer_address = p_top_level_build.aligned_scratch_buffer_address,
               top_level_build_ranges = p_top_level_build.build_ranges,
               command_build_acceleration_structures](VkCommandBuffer p_command_buffer) mutable {
        // Keep all build inputs, scratch storage, and acceleration structures alive until the queued build completes.
        (void)bottom_level_acceleration_structures;
        (void)captured_bottom_level_scratch_buffer;
        (void)top_level_acceleration_structure;
        (void)top_level_instance_buffer;
        (void)top_level_scratch_buffer;
        // Build BLAS objects serially because they reuse the same scratch buffer.
        for (size_t build_index = 0; build_index < bottom_level_builds.size(); ++build_index) {
            VkAccelerationStructureBuildGeometryInfoKHR bottom_level_build_info = make_bottom_level_build_info(
                std::span<const VkAccelerationStructureGeometryKHR>(&bottom_level_builds[build_index].geometry, 1));
            bottom_level_build_info.dstAccelerationStructure =
                bottom_level_builds[build_index].acceleration_structure->get_acceleration_structure();
            bottom_level_build_info.scratchData.deviceAddress = aligned_bottom_level_scratch_buffer_address;
            const VkAccelerationStructureBuildRangeInfoKHR* bottom_level_build_range_pointer =
                &bottom_level_builds[build_index].build_range;
            command_build_acceleration_structures(p_command_buffer, 1, &bottom_level_build_info, &bottom_level_build_range_pointer);
            if (build_index + 1 < bottom_level_builds.size()) {
                // Make the scratch writes visible before the next BLAS reuses them.
                VkMemoryBarrier2 between_blas_barrier{
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                };
                VkDependencyInfo between_blas_dependency_info{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &between_blas_barrier};
                vkCmdPipelineBarrier2(p_command_buffer, &between_blas_dependency_info);
            }
        }

        // Ensure every BLAS is readable before the TLAS references it.
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

        // Build the TLAS after all referenced BLAS objects are complete.
        VkAccelerationStructureBuildGeometryInfoKHR top_level_build_info = make_top_level_build_info(&top_level_geometry);
        top_level_build_info.dstAccelerationStructure = top_level_acceleration_structure->get_acceleration_structure();
        top_level_build_info.scratchData.deviceAddress = aligned_top_level_scratch_buffer_address;
        const VkAccelerationStructureBuildRangeInfoKHR* top_level_build_range_pointer = top_level_build_ranges.data();
        command_build_acceleration_structures(p_command_buffer, 1, &top_level_build_info, &top_level_build_range_pointer);

        // Expose the completed TLAS to later ray-query or ray-tracing shaders.
        VkMemoryBarrier2 shader_read_barrier{};
        shader_read_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        shader_read_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        shader_read_barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        shader_read_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        shader_read_barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        VkDependencyInfo shader_read_dependency_info{};
        shader_read_dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        shader_read_dependency_info.memoryBarrierCount = 1;
        shader_read_dependency_info.pMemoryBarriers = &shader_read_barrier;
        vkCmdPipelineBarrier2(p_command_buffer, &shader_read_dependency_info);
    };
}

GPUJobVK RayTracingPluginVK::update_static_opaque_acceleration_structure(RefRes<RUStaticGeometrySet> p_set,
    Ref<ROBufferVK> p_vertex_buffer,
    Ref<ROBufferVK> p_index_buffer,
    RefRes<RUStaticOpaqueDoubleSidedSet> p_double_sided_set) {
    if ((p_set == MEM_NULL && p_double_sided_set == MEM_NULL) || p_vertex_buffer == MEM_NULL || p_index_buffer == MEM_NULL) {
        // Missing static geometry removes the old AS and requires no GPU work.
        reset_static_opaque_acceleration_structures();
        return nullptr;
    }

    StaticOpaqueBuildData build_data;
    if (p_set != MEM_NULL) {
        build_data = build_static_opaque_data(p_set, p_vertex_buffer->get_device_address(), p_index_buffer->get_device_address());
    }
    if (p_double_sided_set != MEM_NULL) {
        auto double_sided_data = build_static_opaque_data(
            p_double_sided_set, p_vertex_buffer->get_device_address(), p_index_buffer->get_device_address());
        for (auto& build : double_sided_data.bottom_level_builds) {
            build_data.bottom_level_builds.push_back(std::move(build));
        }
    }
    if (build_data.bottom_level_builds.empty()) {
        // All submeshes were empty, so the previous TLAS is no longer valid.
        reset_static_opaque_acceleration_structures();
        return nullptr;
    }
    return create_static_opaque_build_job(std::move(build_data));
}

GPUJobVK RayTracingPluginVK::on_static_opaque_set_updated(BaseRenderDataVK* p_render_data,
    RefRes<RUStaticOpaqueSet> p_set,
    Ref<ROBufferVK> p_vertex_buffer,
    Ref<ROBufferVK> p_index_buffer,
    RefRes<RUStaticOpaqueDoubleSidedSet> p_double_sided_set) {
    WBE_DEBUG_ASSERT(enabled);

    render_data = p_render_data;
    GPUJobVK build_job = update_static_opaque_acceleration_structure(p_set, p_vertex_buffer, p_index_buffer, p_double_sided_set);
    base_ray_tracing_data->static_opaque_acceleration_structure =
        static_opaque_top_level_acceleration_structure == MEM_NULL
            ? VK_NULL_HANDLE
            : static_opaque_top_level_acceleration_structure->get_acceleration_structure();
    return build_job;
}

} // namespace WhiteBirdEngine
