#include "function/render_vk/ro_ray_tracing_pipeline_vk.hh"
#include "function/function_utils.hh"
#include "function/render_vk/i_ro_ray_tracing_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "generated/type_uuid.gen.hh"
#include <limits>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RORayTracingPipelineVK::RORayTracingPipelineVK(const BaseRenderDataVK* p_render_data, const BaseRayTracingDataVK* p_ray_tracing_data, const Builder& p_builder)
    : IRORayTracingVK(p_render_data, p_ray_tracing_data), shaders(p_render_data->allocator) {
    VkRayTracingPipelineCreateInfoKHR pipeline_create_info{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
    };

    (void)pipeline_create_info; (void)p_builder; // TODO
}

RORayTracingPipelineVK::~RORayTracingPipelineVK() {
    vkDestroyPipeline(render_data->logic_device, pipeline, render_data->allocation_callbacks);
}

TypeID RORayTracingPipelineVK::get_type_uuid() const {
    return TypeUUID<RORayTracingPipelineVK>::UUID;
}

size_t RORayTracingPipelineVK::calculate_sbt_buffer_size(VkPipeline p_ray_pipeline,
    VkRayTracingPipelineCreateInfoKHR p_ray_pipeline_info,
    std::span<const VkRayTracingPipelineCreateInfoKHR> p_libraries_info) {
    WBE_DEBUG_ASSERT(p_ray_pipeline != VK_NULL_HANDLE);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_pipeline_properties{};
    ray_tracing_pipeline_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 physical_device_properties{};
    physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physical_device_properties.pNext = &ray_tracing_pipeline_properties;
    vkGetPhysicalDeviceProperties2(render_data->physical_device, &physical_device_properties);

    size_t total_group_count = p_ray_pipeline_info.groupCount;
    for (const VkRayTracingPipelineCreateInfoKHR& library_info : p_libraries_info) {
        if (total_group_count > std::numeric_limits<size_t>::max() - library_info.groupCount) {
            throw std::runtime_error("Shader binding table group count overflow.");
        }
        total_group_count += library_info.groupCount;
    }

    const size_t shader_group_handle_stride = get_align_size(
        ray_tracing_pipeline_properties.shaderGroupHandleSize,
        ray_tracing_pipeline_properties.shaderGroupHandleAlignment);
    if (total_group_count > std::numeric_limits<size_t>::max() / shader_group_handle_stride) {
        throw std::runtime_error("Shader binding table size overflow.");
    }

    const size_t shader_group_handles_size = total_group_count * shader_group_handle_stride;
    return get_align_size(shader_group_handles_size, ray_tracing_pipeline_properties.shaderGroupBaseAlignment);
}

void RORayTracingPipelineVK::create_shader_binding_table() {
    VkRayTracingPipelineCreateInfoKHR create_info {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
    };
    (void)create_info;
    // TODO
    auto builder = ROBufferVK::Builder();
    shader_binding_table_buffer = builder
        .set_usage(VK_BUFFER_USAGE_2_SHADER_BINDING_TABLE_BIT_KHR)
        .set_domain(DeviceDomain::DEVICE)
        .set_size(1024 /*TODO*/)
        .create_ref(render_data);
}

} // namespace WhiteBirdEngine
