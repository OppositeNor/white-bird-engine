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
#include "../render_test.hh"
#include "application_vulkan_deferred_pbr_test.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_targets_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/rt_cpu_in_flight_acquire_vk.hh"
#include "function/render_vk/rt_generate_mipmaps_vk.hh"
#include "function/render_vk/rt_ping_pong_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipelines_sequential_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_triggered_vk.hh"
#include "glm/gtc/packing.hpp"
#include "glm/matrix.hpp"
#include "i_octree_render_feature.hh"
#include "resource/resource_utils.hh"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

constexpr size_t BLOOM_BYTES_PER_PIXEL = sizeof(uint16_t) * 4;

class VoxelDebugIndirectLightingTask final : public IRenderTaskVK {
public:
    VoxelDebugIndirectLightingTask(
        Unique<IRenderTaskVK> p_task, Unique<IRenderTaskVK> p_debug_view_task, const std::atomic<VoxelDebugViewMode>* p_debug_view_mode)
        : task(std::move(p_task)), debug_view_task(std::move(p_debug_view_task)), debug_view_mode(p_debug_view_mode) {
        if (task == MEM_NULL || debug_view_task == MEM_NULL || debug_view_mode == nullptr) {
            throw std::runtime_error("Failed to create the voxel debug indirect-lighting task: task or mode state is null.");
        }
        if (task->queue_family() != debug_view_task->queue_family()) {
            throw std::runtime_error("Failed to create the voxel debug indirect-lighting task: queue families do not match.");
        }
    }
    ~VoxelDebugIndirectLightingTask() override = default;
    VoxelDebugIndirectLightingTask(const VoxelDebugIndirectLightingTask&) = delete;
    VoxelDebugIndirectLightingTask(VoxelDebugIndirectLightingTask&&) noexcept = delete;
    VoxelDebugIndirectLightingTask& operator=(const VoxelDebugIndirectLightingTask&) = delete;
    VoxelDebugIndirectLightingTask& operator=(VoxelDebugIndirectLightingTask&&) noexcept = delete;

    QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }

    void bind_resources(const RendResViewVK& p_resources) override {
        task->bind_resources(p_resources);
        debug_view_task->bind_resources(p_resources);
    }

    void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        task->append_accesses(p_reads, p_writes);
        debug_view_task->append_accesses(p_reads, p_writes);
    }

    void record(VkCommandBuffer p_command_buffer, double p_delta) override {
        IRenderTaskVK* selected_task =
            debug_view_mode->load(std::memory_order_relaxed) == VoxelDebugViewMode::DISABLED ? task.get() : debug_view_task.get();
        selected_task->record(p_command_buffer, p_delta);
    }

private:
    Unique<IRenderTaskVK> task;
    Unique<IRenderTaskVK> debug_view_task;
    const std::atomic<VoxelDebugViewMode>* debug_view_mode;
};

std::vector<uint8_t> convert_bloom_to_rgba8(std::span<const uint8_t> p_bloom_data) {
    if (p_bloom_data.size_bytes() % BLOOM_BYTES_PER_PIXEL != 0) {
        throw std::runtime_error("Failed to convert deferred bloom acquisition: byte count is not pixel aligned.");
    }
    const size_t pixel_count = p_bloom_data.size_bytes() / BLOOM_BYTES_PER_PIXEL;
    std::vector<uint8_t> rgba8(pixel_count * 4);
    for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
        for (size_t channel = 0; channel < 3; ++channel) {
            uint16_t packed_channel = 0;
            std::memcpy(&packed_channel,
                p_bloom_data.data() + pixel * BLOOM_BYTES_PER_PIXEL + channel * sizeof(packed_channel),
                sizeof(packed_channel));
            float value = glm::unpackHalf1x16(packed_channel);
            if (!std::isfinite(value)) {
                value = value > 0.0F ? 1.0F : 0.0F;
            }
            rgba8[pixel * 4 + channel] = static_cast<uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
        }
        rgba8[pixel * 4 + 3] = UINT8_MAX;
    }
    return rgba8;
}

} // namespace

void ApplicationVulkanDeferredPBRTest::register_deferred_render_graph(
    std::string_view p_graph_id, bool p_use_summed_area_table, RendTaskDescVK p_shadow_opaque_task, RendTaskDescVK p_gbuffer_opaque_task) {
    RendGraphDescVK pipeline_desc(render_allocator);
    add_render_graph_resources(pipeline_desc);
    pipeline_desc.add_task(std::move(p_shadow_opaque_task));
    pipeline_desc.add_task(create_shadow_task(true, true));
    const std::array<std::string_view, 3> shadow_colors{SHADOWMAP_COLOR_IMAGE_VIEW, RSM_NORMAL_IMAGE_VIEW, SHADOW_MOMENTS_IMAGE_VIEW};
    auto dynamic_shadow = dynamic_mesh->make_task(
        RT_RUN_DYNAMIC_SHADOW, shadow_opaque_graphics_pipeline, shadow_extent, shadow_colors, SHADOWMAP_DEPTH_IMAGE_VIEW);
    pipeline_desc.add_task(std::move(dynamic_shadow));
    if (p_use_summed_area_table) {
        pipeline_desc.add_task(create_shadow_moments_summed_area_table_task_desc());
    } else {
        pipeline_desc.add_task(create_shadow_mipmap_task_desc());
    }
    pipeline_desc.add_task(std::move(p_gbuffer_opaque_task));
    pipeline_desc.add_task(create_gbuffer_task(true, true));
    const std::array<std::string_view, 3> gbuffer_colors{GBUFFER_ALBEDO_IMAGE_VIEW, GBUFFER_NORMAL_IMAGE_VIEW, GBUFFER_RMA_IMAGE_VIEW};
    auto dynamic_gbuffer = dynamic_mesh->make_task(
        RT_RUN_DYNAMIC_GBUFFER, gbuffer_opaque_graphics_pipeline, base_render_data->swap_chain_extent, gbuffer_colors, GBUFFER_DEPTH_IMAGE_VIEW);
    pipeline_desc.add_task(std::move(dynamic_gbuffer));
    pipeline_desc.add_task(create_direct_lighting_task_desc(p_use_summed_area_table));
    octree->add_construction_render_tasks(pipeline_desc, *dynamic_mesh);
    pipeline_desc.add_task(octree->get_deferred_construction_render_task_desc());
    pipeline_desc.add_task(octree->get_debug_draw_setup_render_task_desc())
        .add_task(create_indirect_lighting_task_desc())
        .add_task(create_combine_lighting_task_desc())
        .add_task(create_bloom_extract_task_desc())
        .add_task(create_bloom_blur_task_desc())
        .add_task(create_bloom_acquire_task_desc(p_use_summed_area_table))
        .add_task(create_present_result_image_task_desc())
        .add_task(octree->get_debug_draw_render_task_desc())
        .add_task(octree->get_pre_deferred_debug_draw_render_task_desc())
        .add_task(create_light_billboard_task_desc());
    connect_deferred_render_tasks(pipeline_desc, p_use_summed_area_table);
    octree->connect_render_tasks(pipeline_desc);
    Ref<RenderGraphVK> graph = renderer_vk->register_render_graph(p_graph_id, std::move(pipeline_desc));
    write_descriptor_debug_output(std::format("{}_graph_descriptors", p_graph_id), graph->list_descriptors());
}

void ApplicationVulkanDeferredPBRTest::connect_deferred_render_tasks(
    RendGraphDescVK& p_pipeline_desc, bool p_use_summed_area_table) const { // NOLINT
    constexpr VkPipelineStageFlags2 COLOR = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    constexpr VkPipelineStageFlags2 DEPTH = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    constexpr VkPipelineStageFlags2 COMPUTE = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    constexpr VkPipelineStageFlags2 FRAGMENT = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    constexpr VkPipelineStageFlags2 TRANSFER = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    auto writer = [](std::string_view p_task_id, VkPipelineStageFlags2 p_stage) {
        return ResourceChainLink{.task_id = p_task_id, .stage = p_stage, .writes = true};
    };
    auto reader = [](std::string_view p_task_id, VkPipelineStageFlags2 p_stage) {
        return ResourceChainLink{.task_id = p_task_id, .stage = p_stage, .writes = false};
    };
    const ResourceChainLink direct_lighting = reader(RT_RUN_DIRECT_LIGHTING, COMPUTE);
    const ResourceChainLink indirect_lighting = reader(RT_RUN_INDIRECT_LIGHTING, COMPUTE);

    // Shadow and reflective-shadow-map passes.
    for (std::string_view shadow_color : {SHADOWMAP_COLOR_IMAGE_VIEW, RSM_NORMAL_IMAGE_VIEW, SHADOW_MOMENTS_IMAGE_VIEW}) {
        connect_resource_chain(p_pipeline_desc,
            shadow_color,
            {writer(RT_RUN_DEFERRED_SHADOW_OPAQUE, COLOR),
                writer(RT_RUN_DEFERRED_SHADOW_DOUBLE_SIDED, COLOR),
                writer(RT_RUN_DYNAMIC_SHADOW, COLOR)});
    }
    connect_resource_chain(p_pipeline_desc,
        SHADOWMAP_DEPTH_IMAGE_VIEW,
        {writer(RT_RUN_DEFERRED_SHADOW_OPAQUE, DEPTH), writer(RT_RUN_DEFERRED_SHADOW_DOUBLE_SIDED, DEPTH), writer(RT_RUN_DYNAMIC_SHADOW, DEPTH)});
    for (std::string_view shadow_color : {SHADOWMAP_COLOR_IMAGE_VIEW, RSM_NORMAL_IMAGE_VIEW}) {
        connect_resource_chain(p_pipeline_desc, shadow_color, {writer(RT_RUN_DYNAMIC_SHADOW, COLOR), direct_lighting});
        connect_resource_chain(p_pipeline_desc, shadow_color, {writer(RT_RUN_DYNAMIC_SHADOW, COLOR), indirect_lighting});
    }
    if (p_use_summed_area_table) {
        connect_resource_chain(p_pipeline_desc,
            SHADOW_MOMENTS_IMAGE_VIEW,
            {writer(RT_RUN_DYNAMIC_SHADOW, COLOR), reader(RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE, COMPUTE)});
        connect_resource_chain(p_pipeline_desc, SHADOW_MOMENTS_IMAGE_VIEW, {writer(RT_RUN_DYNAMIC_SHADOW, COLOR), direct_lighting});
        connect_resource_chain(p_pipeline_desc, SHADOW_MOMENTS_IMAGE_VIEW, {writer(RT_RUN_DYNAMIC_SHADOW, COLOR), indirect_lighting});
        connect_resource_chain(p_pipeline_desc,
            SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW,
            {writer(RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE, COMPUTE), direct_lighting});
    } else {
        connect_resource_chain(p_pipeline_desc,
            SHADOW_MOMENTS_IMAGE_VIEW,
            {writer(RT_RUN_DYNAMIC_SHADOW, COLOR), writer(RT_GENERATE_SHADOW_MIPMAPS, TRANSFER), indirect_lighting});
        // Direct lighting samples the complete mip chain through a second view of the moments image.
        connect_resource_chain(
            p_pipeline_desc, SHADOW_MOMENTS_SAMPLED_IMAGE_VIEW, {writer(RT_GENERATE_SHADOW_MIPMAPS, TRANSFER), direct_lighting});
    }

    // G-buffer passes.
    for (std::string_view gbuffer_color : {GBUFFER_ALBEDO_IMAGE_VIEW, GBUFFER_NORMAL_IMAGE_VIEW, GBUFFER_RMA_IMAGE_VIEW}) {
        connect_resource_chain(p_pipeline_desc,
            gbuffer_color,
            {writer(RT_RUN_DEFERRED_GBUFFER, COLOR),
                writer(RT_RUN_DEFERRED_GBUFFER_DOUBLE_SIDED, COLOR),
                writer(RT_RUN_DYNAMIC_GBUFFER, COLOR),
                direct_lighting});
        connect_resource_chain(p_pipeline_desc, gbuffer_color, {writer(RT_RUN_DYNAMIC_GBUFFER, COLOR), indirect_lighting});
    }
    connect_resource_chain(
        p_pipeline_desc, GBUFFER_RMA_IMAGE_VIEW, {writer(RT_RUN_DYNAMIC_GBUFFER, COLOR), reader(RT_COMBINE_LIGHTING, COMPUTE)});
    connect_resource_chain(p_pipeline_desc,
        GBUFFER_DEPTH_IMAGE_VIEW,
        {writer(RT_RUN_DEFERRED_GBUFFER, DEPTH), writer(RT_RUN_DEFERRED_GBUFFER_DOUBLE_SIDED, DEPTH), writer(RT_RUN_DYNAMIC_GBUFFER, DEPTH), direct_lighting});
    connect_resource_chain(p_pipeline_desc, GBUFFER_DEPTH_IMAGE_VIEW, {writer(RT_RUN_DYNAMIC_GBUFFER, DEPTH), indirect_lighting});
    // Overlays depth-test against the G-buffer depth after lighting has sampled it.
    const ResourceChainLink octree_debug_depth = writer(IOctreeRenderFeature::RT_RUN_OCTREE_DEBUG_DRAW, DEPTH);
    connect_resource_chain(p_pipeline_desc, GBUFFER_DEPTH_IMAGE_VIEW, {direct_lighting, octree_debug_depth});
    connect_resource_chain(p_pipeline_desc,
        GBUFFER_DEPTH_IMAGE_VIEW,
        {indirect_lighting,
            octree_debug_depth,
            writer(IOctreeRenderFeature::RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW, DEPTH),
            writer(RT_RUN_LIGHT_BILLBOARD, DEPTH)});

    // Lighting composition and post-processing.
    connect_resource_chain(
        p_pipeline_desc, DIRECT_LIGHTING_IMAGE_VIEW, {writer(RT_RUN_DIRECT_LIGHTING, COMPUTE), reader(RT_COMBINE_LIGHTING, COMPUTE)});
    connect_resource_chain(p_pipeline_desc,
        INDIRECT_LIGHTING_IMAGE_VIEW,
        {writer(RT_RUN_INDIRECT_LIGHTING, COMPUTE), reader(RT_COMBINE_LIGHTING, COMPUTE)});
    connect_resource_chain(
        p_pipeline_desc, RESULT_IMAGE_VIEW, {writer(RT_COMBINE_LIGHTING, COMPUTE), reader(RT_EXTRACT_BLOOM, COMPUTE)});
    connect_resource_chain(
        p_pipeline_desc, RESULT_IMAGE_VIEW, {writer(RT_COMBINE_LIGHTING, COMPUTE), reader(RT_RUN_PRESENT_RESULT_IMAGE, FRAGMENT)});
    // The blur task ping-pongs between the bloom images internally; only its final ping result leaves the task.
    connect_resource_chain(p_pipeline_desc,
        BLOOM_PING_IMAGE_VIEW,
        {writer(RT_EXTRACT_BLOOM, COMPUTE),
            writer(RT_BLUR_BLOOM, COMPUTE),
            reader(RT_ACQUIRE_BLOOM, TRANSFER),
            reader(RT_RUN_PRESENT_RESULT_IMAGE, FRAGMENT)});
    connect_resource_chain(p_pipeline_desc,
        REND_RES_SWAP_CHAIN,
        {writer(RT_RUN_PRESENT_RESULT_IMAGE, COLOR),
            writer(IOctreeRenderFeature::RT_RUN_OCTREE_DEBUG_DRAW, COLOR),
            writer(IOctreeRenderFeature::RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW, COLOR),
            writer(RT_RUN_LIGHT_BILLBOARD, COLOR)});
}

void ApplicationVulkanDeferredPBRTest::add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const {
    p_pipeline_desc.add_static_set(static_set_resources);
    dynamic_mesh->register_resources(p_pipeline_desc);
    p_pipeline_desc.add_resource(REND_RES_SWAP_CHAIN, renderer_vk->acquire_swap_chain_image_view_container())
        .add_resource(GBUFFER_CAMERA_INFO_BUFFER, gbuffer_camera_info_ubos)
        .add_resource(COMPOSITION_CAMERA_INFO_BUFFER, composition_camera_info_ubos)
        .add_resource(SHADOW_LIGHT_INFO_BUFFER, spot_light_info_buffers)
        .add_resource(SPOT_LIGHT_INFO_BUFFER, spot_light_info_buffers)
        .add_resource(SPOT_LIGHT_COUNT_BUFFER, spot_light_count_buffer)
        .add_resource(LIGHT_BILLBOARD_INFO_BUFFER, light_billboard_ubo)
        .add_resource(LIGHT_BILLBOARD_TEXTURE_IMAGE_VIEW, light_billboard_texture_view);
    add_graph_owned_resources(p_pipeline_desc);
    octree->register_render_graph_data(p_pipeline_desc);
}

void ApplicationVulkanDeferredPBRTest::add_graph_owned_resources(RendGraphDescVK& p_pipeline_desc) const {
    constexpr std::string_view SHADOWMAP_DEPTH_IMAGE = "shadowmap_depth_image";
    constexpr std::string_view SHADOWMAP_COLOR_IMAGE = "shadowmap_color_image";
    constexpr std::string_view RSM_NORMAL_IMAGE = "rsm_normal_image";
    constexpr std::string_view SHADOW_MOMENTS_IMAGE = "shadow_moments_image";
    constexpr std::string_view SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE = "shadow_moments_sat_horizontal_image";
    constexpr std::string_view SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE = "shadow_moments_sat_image";
    constexpr std::string_view GBUFFER_ALBEDO_IMAGE = "gbuffer_albedo_image";
    constexpr std::string_view GBUFFER_NORMAL_IMAGE = "gbuffer_normal_image";
    constexpr std::string_view GBUFFER_RMA_IMAGE = "gbuffer_rma_image";
    constexpr std::string_view GBUFFER_DEPTH_IMAGE = "gbuffer_depth_image";
    constexpr std::string_view DIRECT_LIGHTING_IMAGE = "direct_lighting_image";
    constexpr std::string_view INDIRECT_LIGHTING_IMAGE = "indirect_lighting_image";
    constexpr std::string_view RESULT_IMAGE = "result_color_image";
    constexpr std::string_view BLOOM_PING_IMAGE = "bloom_ping_image";
    constexpr std::string_view BLOOM_PONG_IMAGE = "bloom_pong_image";
    auto image = [](RendResPrimaryUsageVK p_usage, VkFormat p_format, VkExtent2D p_extent) {
        return RendImageDescVK{
            .usage = p_usage, .format = p_format, .extent = {.width = p_extent.width, .height = p_extent.height, .depth = 1}};
    };
    const VkExtent2D screen_extent = base_render_data->swap_chain_extent;
    RendImageDescVK shadow_moments = image(RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, SHADOW_MOMENTS_FORMAT, shadow_extent);
    shadow_moments.additional_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    shadow_moments.mip_levels = shadow_mipmap_level_count;
    RendImageDescVK bloom_ping = image(RendResPrimaryUsageVK::IMAGE_READ_WRITE, LIGHTING_OUTPUT_FORMAT, bloom_extent);
    bloom_ping.additional_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    const RendSamplerDescVK clamped_linear_sampler{.address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};

    // Shadow and reflective-shadow-map targets.
    p_pipeline_desc
        .add_image(SHADOWMAP_DEPTH_IMAGE, image(RendResPrimaryUsageVK::IMAGE_DEPTH_TARGET, SHADOWMAP_DEPTH_FORMAT, shadow_extent))
        .add_image(SHADOWMAP_COLOR_IMAGE, image(RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, RSM_FLUX_FORMAT, shadow_extent))
        .add_image(RSM_NORMAL_IMAGE, image(RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, RSM_NORMAL_FORMAT, shadow_extent))
        .add_image(SHADOW_MOMENTS_IMAGE, shadow_moments)
        .add_image(SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE,
            image(RendResPrimaryUsageVK::IMAGE_READ_WRITE, SHADOW_MOMENTS_FORMAT, shadow_extent))
        .add_image(SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE,
            image(RendResPrimaryUsageVK::IMAGE_READ_WRITE, SHADOW_MOMENTS_FORMAT, shadow_extent))
        .add_image_view(SHADOWMAP_DEPTH_IMAGE_VIEW, SHADOWMAP_DEPTH_IMAGE)
        .add_image_view(SHADOWMAP_COLOR_IMAGE_VIEW, SHADOWMAP_COLOR_IMAGE)
        .add_image_view(RSM_NORMAL_IMAGE_VIEW, RSM_NORMAL_IMAGE)
        .add_image_view(SHADOW_MOMENTS_IMAGE_VIEW, SHADOW_MOMENTS_IMAGE)
        .add_image_view(SHADOW_MOMENTS_SAMPLED_IMAGE_VIEW, SHADOW_MOMENTS_IMAGE, {.mip_level_count = VK_REMAINING_MIP_LEVELS})
        .add_image_view(SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE_VIEW, SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE)
        .add_image_view(SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW, SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE)
        .add_sampler(SHADOW_SAMPLER,
            {.address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .min_lod = 0.0F,
                .max_lod = static_cast<float>(shadow_mipmap_level_count - 1)});

    // G-buffer targets.
    p_pipeline_desc
        .add_image(GBUFFER_ALBEDO_IMAGE, image(RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, GBUFFER_ALBEDO_FORMAT, screen_extent))
        .add_image(GBUFFER_NORMAL_IMAGE, image(RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, GBUFFER_NORMAL_FORMAT, screen_extent))
        .add_image(GBUFFER_RMA_IMAGE, image(RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, GBUFFER_RMA_FORMAT, screen_extent))
        .add_image(GBUFFER_DEPTH_IMAGE, image(RendResPrimaryUsageVK::IMAGE_DEPTH_TARGET, GBUFFER_DEPTH_FORMAT, screen_extent))
        .add_image_view(GBUFFER_ALBEDO_IMAGE_VIEW, GBUFFER_ALBEDO_IMAGE)
        .add_image_view(GBUFFER_NORMAL_IMAGE_VIEW, GBUFFER_NORMAL_IMAGE)
        .add_image_view(GBUFFER_RMA_IMAGE_VIEW, GBUFFER_RMA_IMAGE)
        .add_image_view(GBUFFER_DEPTH_IMAGE_VIEW, GBUFFER_DEPTH_IMAGE)
        .add_sampler(GBUFFER_SAMPLER,
            {.min_filter = VK_FILTER_NEAREST,
                .mag_filter = VK_FILTER_NEAREST,
                .address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE});

    // Lighting and post-processing outputs.
    p_pipeline_desc
        .add_image(DIRECT_LIGHTING_IMAGE, image(RendResPrimaryUsageVK::IMAGE_READ_WRITE, LIGHTING_OUTPUT_FORMAT, screen_extent))
        .add_image(INDIRECT_LIGHTING_IMAGE, image(RendResPrimaryUsageVK::IMAGE_READ_WRITE, LIGHTING_OUTPUT_FORMAT, screen_extent))
        .add_image(RESULT_IMAGE, image(RendResPrimaryUsageVK::IMAGE_READ_WRITE, LIGHTING_OUTPUT_FORMAT, screen_extent))
        .add_image(BLOOM_PING_IMAGE, bloom_ping)
        .add_image(BLOOM_PONG_IMAGE, image(RendResPrimaryUsageVK::IMAGE_READ_WRITE, LIGHTING_OUTPUT_FORMAT, bloom_extent))
        .add_image_view(DIRECT_LIGHTING_IMAGE_VIEW, DIRECT_LIGHTING_IMAGE)
        .add_image_view(INDIRECT_LIGHTING_IMAGE_VIEW, INDIRECT_LIGHTING_IMAGE)
        .add_image_view(RESULT_IMAGE_VIEW, RESULT_IMAGE)
        .add_image_view(BLOOM_PING_IMAGE_VIEW, BLOOM_PING_IMAGE)
        .add_image_view(BLOOM_PONG_IMAGE_VIEW, BLOOM_PONG_IMAGE)
        .add_sampler(BLOOM_SAMPLER, clamped_linear_sampler)
        .add_sampler(LIGHT_BILLBOARD_SAMPLER, clamped_linear_sampler);
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_shadow_moments_summed_area_table_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE;
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_shadow_extent = shadow_extent,
                           horizontal_pipeline = shadow_moments_summed_area_table_horizontal_compute_pipeline,
                           vertical_pipeline = shadow_moments_summed_area_table_vertical_compute_pipeline]() -> Unique<IRenderTaskVK> {
        VkExtent3D horizontal_dispatch_group_count{
            .width = (task_shadow_extent.height + 63U) / 64U,
            .height = 1,
            .depth = 1,
        };
        VkExtent3D vertical_dispatch_group_count{
            .width = (task_shadow_extent.width + 63U) / 64U,
            .height = 1,
            .depth = 1,
        };
        RTRunComputePipelinesSequentialVK::Dispatch dispatch(task_render_allocator);
        dispatch.add_entry(horizontal_pipeline, horizontal_dispatch_group_count).add_entry(vertical_pipeline, vertical_dispatch_group_count);
        return make_unique<RTRunComputePipelinesSequentialVK>(task_render_allocator, task_base_render_data, dispatch);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_shadow_mipmap_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_GENERATE_SHADOW_MIPMAPS;
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_shadow_extent = shadow_extent,
                           task_shadow_mipmap_level_count = shadow_mipmap_level_count]() -> Unique<IRenderTaskVK> {
        return make_unique<RTGenerateMipmapsVK>(
            task_render_allocator, task_base_render_data, SHADOW_MOMENTS_IMAGE_VIEW, task_shadow_extent, task_shadow_mipmap_level_count, SHADOW_MOMENTS_FORMAT);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_direct_lighting_task_desc(bool p_use_summed_area_table) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_DIRECT_LIGHTING;
    Ref<ROComputePipelineVK> pipeline =
        p_use_summed_area_table ? direct_lighting_summed_area_table_compute_pipeline : direct_lighting_compute_pipeline;
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = std::move(pipeline)]() -> Unique<IRenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_indirect_lighting_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_INDIRECT_LIGHTING;
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = indirect_lighting_compute_pipeline,
                           task_debug_view_pipeline = indirect_lighting_debug_view_compute_pipeline,
                           task_debug_view_mode = &voxel_debug_view_mode]() -> Unique<IRenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        Unique<IRenderTaskVK> task =
            make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline, dispatch_group_count);
        Unique<IRenderTaskVK> debug_view_task = make_unique<RTRunComputePipelineVK>(
            task_render_allocator, task_base_render_data, task_debug_view_pipeline, dispatch_group_count);
        return make_unique<VoxelDebugIndirectLightingTask>(
            task_render_allocator, std::move(task), std::move(debug_view_task), task_debug_view_mode);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_combine_lighting_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_COMBINE_LIGHTING;
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = combine_lighting_compute_pipeline]() -> Unique<IRenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_bloom_extract_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_EXTRACT_BLOOM;
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = bloom_extract_compute_pipeline]() -> Unique<IRenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_bloom_blur_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_BLUR_BLOOM;
    task_desc.alias(KAWASE_BLUR_PING, BLOOM_PING_IMAGE_VIEW).alias(KAWASE_BLUR_PONG, BLOOM_PONG_IMAGE_VIEW).alias(KAWASE_BLUR_SAMPLER, BLOOM_SAMPLER);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_bloom_extent = bloom_extent,
                           pong_write_pipeline = kawase_blur_pong_write_compute_pipeline,
                           ping_write_pipeline = kawase_blur_ping_write_compute_pipeline]() -> Unique<IRenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_bloom_extent.width + 7U) / 8U,
            .height = (task_bloom_extent.height + 7U) / 8U,
            .depth = 1,
        };
        // Starting from ping, even iterations blur ping into pong and odd iterations blur pong back into ping, so an even
        // iteration count leaves the result in ping.
        return make_unique<RTPingPongVK>(
            task_render_allocator, task_base_render_data, pong_write_pipeline, ping_write_pipeline, false, BLOOM_BLUR_PASS_COUNT, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_bloom_acquire_task_desc(bool p_use_summed_area_table) {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_ACQUIRE_BLOOM;
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_destination = std::span<uint8_t>(bloom_acquire_buffer),
                           this,
                           task_index = p_use_summed_area_table ? 1U : 0U,
                           task_acquire_completed = &bloom_acquire_completed]() -> Unique<IRenderTaskVK> {
        auto acquire_task = make_unique<RTCPUInFlightAcquireVK>(
            task_render_allocator, task_base_render_data, BLOOM_PING_IMAGE_VIEW, task_destination, task_acquire_completed);
        auto task = make_unique<RTTriggeredVK>(task_render_allocator, std::move(acquire_task));
        bloom_acquire_tasks[task_index] = RefRaw<RTTriggeredVK>(task);
        return task;
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_present_result_image_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_PRESENT_RESULT_IMAGE;
    task_desc.alias(RenderTargetsVK::color_id(0), REND_RES_SWAP_CHAIN);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = present_post_processing_graphics_pipeline]() -> Unique<IRenderTaskVK> {
        const std::array<VkClearValue, 1> clear_values{VkClearValue{.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}}};
        return RTRunGraphicsPipelineVK::Builder(task_render_allocator)
            .set_pipeline(task_pipeline)
            .set_render_extent(task_base_render_data->swap_chain_extent)
            .set_clear_values(clear_values)
            .create_unique(task_base_render_data);
    };
    return task_desc;
}

void ApplicationVulkanDeferredPBRTest::write_completed_bloom_acquire() {
    if (!bloom_acquire_completed.try_acquire()) {
        return;
    }

    std::filesystem::path output_directory = std::filesystem::path("test_env") / "descriptor_debug";
    std::filesystem::create_directories(output_directory);
    std::filesystem::path output_path = output_directory / "deferred_bloom.png";
    std::vector<uint8_t> rgba8 = convert_bloom_to_rgba8(bloom_acquire_buffer);
    write_image_data_png(output_path, rgba8, bloom_extent.width, bloom_extent.height, 4);
    bloom_acquire_in_flight = false;
    stdout_log(WBE_CHANNEL_DEBUG)->message(std::format("Stored deferred bloom acquisition in {}.", output_path.generic_string()));
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_light_billboard_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_LIGHT_BILLBOARD;
    const std::array<std::string_view, 1> color_targets{REND_RES_SWAP_CHAIN};
    alias_render_targets(task_desc, color_targets, GBUFFER_DEPTH_IMAGE_VIEW);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = light_billboard_pipeline]() -> Unique<IRenderTaskVK> {
        return RTRunGraphicsPipelineVK::Builder(task_render_allocator)
            .set_pipeline(task_pipeline)
            .set_render_extent(task_base_render_data->swap_chain_extent)
            .set_color_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .set_depth_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .set_draw_vertex_count(6)
            .create_unique(task_base_render_data);
    };
    return task_desc;
}

ApplicationVulkanDeferredPBRTest::CompositionCameraInfo ApplicationVulkanDeferredPBRTest::make_composition_camera_info() const {
    return CompositionCameraInfo{
        .inverse_view_projection = glm::inverse(camera_component.proj * camera_component.view),
        .camera_world_position = glm::vec4(camera_position, 1.0F),
    };
}

} // namespace WhiteBirdEngine
