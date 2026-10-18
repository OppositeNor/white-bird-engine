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
#include "function/render_vk/i_bindable_render_resource_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/mesh_acceleration_structure_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_targets_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/rt_build_acceleration_structures_vk.hh"
#include "function/render_vk/rt_cpu_in_flight_acquire_vk.hh"
#include "function/render_vk/rt_generate_mipmaps_vk.hh"
#include "function/render_vk/rt_ping_pong_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipelines_sequential_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_triggered_vk.hh"
#include "glm/gtc/packing.hpp"
#include "glm/matrix.hpp"
#include "i_voxel_render_feature.hh"
#include "resource/resource_utils.hh"
#include "rt_imgui_vk.hh"
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

// One workgroup per 8x8 tile of the swap chain image.
VkExtent3D make_screen_tile_group_count(const RendGraphContextVK& p_context) {
    return VkExtent3D{
        .width = (p_context.swap_chain_extent.width + 7U) / 8U,
        .height = (p_context.swap_chain_extent.height + 7U) / 8U,
        .depth = 1,
    };
}

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
    virtual ~VoxelDebugIndirectLightingTask() override = default;
    VoxelDebugIndirectLightingTask(const VoxelDebugIndirectLightingTask&) = delete;
    VoxelDebugIndirectLightingTask(VoxelDebugIndirectLightingTask&&) noexcept = delete;
    VoxelDebugIndirectLightingTask& operator=(const VoxelDebugIndirectLightingTask&) = delete;
    VoxelDebugIndirectLightingTask& operator=(VoxelDebugIndirectLightingTask&&) noexcept = delete;

    virtual QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }

    virtual void bind_resources(const RendResViewVK& p_resources) override {
        task->bind_resources(p_resources);
        debug_view_task->bind_resources(p_resources);
    }

    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        task->append_accesses(p_reads, p_writes);
        debug_view_task->append_accesses(p_reads, p_writes);
    }

    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override {
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

RendImageDescVK make_graph_image_desc(RendResPrimaryUsageVK p_usage, VkFormat p_format, VkExtent2D p_extent) {
    return RendImageDescVK{
        .usage = p_usage,
        .format = p_format,
        .extent = {.width = p_extent.width, .height = p_extent.height, .depth = 1},
    };
}

} // namespace

void ApplicationVulkanDeferredPBRTest::register_deferred_render_graph(
    std::string_view p_graph_id, bool p_use_summed_area_table, RendTaskDescVK p_gbuffer_opaque_task) {
    // The graph calls this for its first build and again whenever it rebuilds: after a swap chain resize, or when
    // draw_pixel_sort_options() rebuilds it for new post-processing pipelines.
    Ref<RenderGraphVK> graph = renderer_vk->register_render_graph(p_graph_id,
        [this, p_use_summed_area_table, gbuffer_opaque_task = std::move(p_gbuffer_opaque_task)](const RendGraphContextVK& /*p_graph_context*/) {
            RendGraphDescVK pipeline_desc(render_allocator);
            add_render_graph_resources(pipeline_desc);
            if (mesh_acceleration_structure != MEM_NULL) {
                // Build the acceleration structures once, on the graph's first recording.
                // A recreated task builds them again on its first recording.
                pipeline_desc.add_task(RendTaskDescVK(render_allocator,
                    RT_BUILD_MESH_ACCELERATION_STRUCTURE,
                    [this, acceleration_structure = mesh_acceleration_structure](const RendGraphContextVK& /*p_context*/) {
                        auto build = make_unique<RTBuildAccelerationStructuresVK>(render_allocator,
                            base_render_data,
                            IBindableRenderResourceVK::make_member_resource_id(MESH_ACCELERATION_STRUCTURE_KEY, MeshAccelerationStructureVK::TOP_LEVEL),
                            [acceleration_structure](VkCommandBuffer p_command_buffer) {
                                acceleration_structure->record_build(p_command_buffer);
                            });
                        return make_unique<RTTriggeredVK>(render_allocator, std::move(build), true);
                    }));
            }
            for (uint32_t light_index = 0; light_index < SPOT_LIGHT_COUNT; ++light_index) {
                add_spot_light_shadow_tasks(pipeline_desc, light_index, p_use_summed_area_table);
            }
            pipeline_desc.add_task(RendTaskDescVK(gbuffer_opaque_task));
            pipeline_desc.add_task(create_gbuffer_task(true, true));
            const std::array<std::string_view, 3> gbuffer_colors{GBUFFER_ALBEDO_IMAGE_VIEW, GBUFFER_NORMAL_IMAGE_VIEW, GBUFFER_RMA_IMAGE_VIEW};
            pipeline_desc.add_task(dynamic_mesh->make_task(
                RT_RUN_DYNAMIC_GBUFFER, 0, gbuffer_dynamic_graphics_pipeline, &RendGraphContextVK::swap_chain_extent, gbuffer_colors, GBUFFER_DEPTH_IMAGE_VIEW));
            pipeline_desc.add_task(dynamic_mesh->make_task(
                RT_RUN_DYNAMIC_GBUFFER_SECOND, 1, gbuffer_dynamic_graphics_pipeline, &RendGraphContextVK::swap_chain_extent, gbuffer_colors, GBUFFER_DEPTH_IMAGE_VIEW));
            pipeline_desc.add_task(create_direct_lighting_task_desc(p_use_summed_area_table));
            voxel_structure->add_build_render_tasks(pipeline_desc, *dynamic_mesh);
            pipeline_desc.add_task(create_indirect_lighting_task_desc()).add_task(create_combine_lighting_task_desc());
            const std::string_view post_processing_last_task_id =
                post_processing->add_render_tasks(pipeline_desc, RESULT_IMAGE_VIEW, RT_COMBINE_LIGHTING);
            pipeline_desc.add_task(create_bloom_extract_task_desc())
                .add_task(create_bloom_blur_task_desc())
                .add_task(create_bloom_acquire_task_desc(p_use_summed_area_table))
                .add_task(create_present_result_image_task_desc())
                .add_task(create_present_frame_sequence_task_desc(p_use_summed_area_table))
                .add_task(create_frame_sequence_acquire_task_desc(p_use_summed_area_table));
            voxel_structure->add_overlay_render_tasks(pipeline_desc);
            for (uint32_t light_index = 0; light_index < SPOT_LIGHT_COUNT; ++light_index) {
                pipeline_desc.add_task(create_light_billboard_task_desc(light_index));
            }
            pipeline_desc.add_task(create_imgui_task_desc());
            connect_deferred_render_tasks(pipeline_desc, p_use_summed_area_table, post_processing_last_task_id);
            voxel_structure->connect_render_tasks(pipeline_desc);
            return pipeline_desc;
        });
    write_descriptor_debug_output(std::format("{}_graph_descriptors", p_graph_id), graph->list_descriptors());
    deferred_render_graphs[p_use_summed_area_table ? 1U : 0U] = graph;
}

void ApplicationVulkanDeferredPBRTest::add_spot_light_shadow_tasks(
    RendGraphDescVK& p_pipeline_desc, uint32_t p_light_index, bool p_use_summed_area_table) const {
    const SpotLightGraphIds ids = make_spot_light_graph_ids(p_light_index);
    p_pipeline_desc.add_task(create_shadow_task(p_light_index, false));
    p_pipeline_desc.add_task(create_shadow_task(p_light_index, true, true));
    const std::array<std::string_view, 3> shadow_colors{ids.shadowmap_color, ids.rsm_normal, ids.shadow_moments};
    RendTaskDescVK dynamic_shadow = dynamic_mesh->make_task(
        ids.run_dynamic_shadow, 0, shadow_dynamic_graphics_pipeline, make_fixed_extent_factory(shadow_extent), shadow_colors, ids.shadowmap_depth);
    dynamic_shadow.alias(SHADOW_LIGHT_INFO_BUFFER, ids.shadow_light_info);
    p_pipeline_desc.add_task(std::move(dynamic_shadow));
    RendTaskDescVK dynamic_shadow_second = dynamic_mesh->make_task(
        ids.run_dynamic_shadow_second, 1, shadow_dynamic_graphics_pipeline, make_fixed_extent_factory(shadow_extent), shadow_colors, ids.shadowmap_depth);
    dynamic_shadow_second.alias(SHADOW_LIGHT_INFO_BUFFER, ids.shadow_light_info);
    p_pipeline_desc.add_task(std::move(dynamic_shadow_second));
    if (p_use_summed_area_table) {
        p_pipeline_desc.add_task(create_shadow_moments_summed_area_table_task_desc(p_light_index));
    } else {
        p_pipeline_desc.add_task(create_shadow_mipmap_task_desc(p_light_index));
    }
}

void ApplicationVulkanDeferredPBRTest::connect_spot_light_shadow_tasks(
    RendGraphDescVK& p_pipeline_desc, uint32_t p_light_index, bool p_use_summed_area_table) const {
    constexpr std::string_view DIRECT_LIGHTING = RT_RUN_DIRECT_LIGHTING;
    constexpr std::string_view INDIRECT_LIGHTING = RT_RUN_INDIRECT_LIGHTING;
    const SpotLightGraphIds ids = make_spot_light_graph_ids(p_light_index);
    const std::array<std::string_view, 4> shadow_passes{
        ids.run_shadow_opaque, ids.run_shadow_double_sided, ids.run_dynamic_shadow, ids.run_dynamic_shadow_second};

    // Shadow and reflective-shadow-map passes of this light.
    for (std::string_view shadow_target : {std::string_view(ids.shadowmap_color),
             std::string_view(ids.rsm_normal),
             std::string_view(ids.shadow_moments),
             std::string_view(ids.shadowmap_depth)}) {
        connect_resource_chain(p_pipeline_desc, shadow_target, shadow_passes);
    }
    for (std::string_view shadow_color : {std::string_view(ids.shadowmap_color), std::string_view(ids.rsm_normal)}) {
        connect_resource_chain(p_pipeline_desc, shadow_color, {ids.run_dynamic_shadow_second, DIRECT_LIGHTING});
        connect_resource_chain(p_pipeline_desc, shadow_color, {ids.run_dynamic_shadow_second, INDIRECT_LIGHTING});
    }
    if (p_use_summed_area_table) {
        connect_resource_chain(
            p_pipeline_desc, ids.shadow_moments, {ids.run_dynamic_shadow_second, ids.build_shadow_moments_summed_area_table});
        connect_resource_chain(p_pipeline_desc, ids.shadow_moments, {ids.run_dynamic_shadow_second, DIRECT_LIGHTING});
        connect_resource_chain(p_pipeline_desc, ids.shadow_moments, {ids.run_dynamic_shadow_second, INDIRECT_LIGHTING});
        connect_resource_chain(
            p_pipeline_desc, ids.shadow_moments_summed_area_table, {ids.build_shadow_moments_summed_area_table, DIRECT_LIGHTING});
    } else {
        connect_resource_chain(
            p_pipeline_desc, ids.shadow_moments, {ids.run_dynamic_shadow_second, ids.generate_shadow_mipmaps, INDIRECT_LIGHTING});
        // Direct lighting samples the complete mip chain through a second view of the moments image.
        connect_resource_chain(p_pipeline_desc, ids.shadow_moments_sampled, {ids.generate_shadow_mipmaps, DIRECT_LIGHTING});
    }
}

void ApplicationVulkanDeferredPBRTest::connect_deferred_render_tasks(
    RendGraphDescVK& p_pipeline_desc, bool p_use_summed_area_table, std::string_view p_post_processing_last_task_id) const { // NOLINT
    constexpr std::string_view DIRECT_LIGHTING = RT_RUN_DIRECT_LIGHTING;
    constexpr std::string_view INDIRECT_LIGHTING = RT_RUN_INDIRECT_LIGHTING;

    for (uint32_t light_index = 0; light_index < SPOT_LIGHT_COUNT; ++light_index) {
        connect_spot_light_shadow_tasks(p_pipeline_desc, light_index, p_use_summed_area_table);
    }

    // G-buffer passes.
    for (std::string_view gbuffer_color : {GBUFFER_ALBEDO_IMAGE_VIEW, GBUFFER_NORMAL_IMAGE_VIEW, GBUFFER_RMA_IMAGE_VIEW}) {
        connect_resource_chain(p_pipeline_desc,
            gbuffer_color,
            {RT_RUN_DEFERRED_GBUFFER, RT_RUN_DEFERRED_GBUFFER_DOUBLE_SIDED, RT_RUN_DYNAMIC_GBUFFER, RT_RUN_DYNAMIC_GBUFFER_SECOND, DIRECT_LIGHTING});
        connect_resource_chain(p_pipeline_desc, gbuffer_color, {RT_RUN_DYNAMIC_GBUFFER_SECOND, INDIRECT_LIGHTING});
    }
    connect_resource_chain(p_pipeline_desc, GBUFFER_RMA_IMAGE_VIEW, {RT_RUN_DYNAMIC_GBUFFER_SECOND, RT_COMBINE_LIGHTING});
    connect_resource_chain(p_pipeline_desc,
        GBUFFER_DEPTH_IMAGE_VIEW,
        {RT_RUN_DEFERRED_GBUFFER, RT_RUN_DEFERRED_GBUFFER_DOUBLE_SIDED, RT_RUN_DYNAMIC_GBUFFER, RT_RUN_DYNAMIC_GBUFFER_SECOND, DIRECT_LIGHTING});
    connect_resource_chain(p_pipeline_desc, GBUFFER_DEPTH_IMAGE_VIEW, {RT_RUN_DYNAMIC_GBUFFER_SECOND, INDIRECT_LIGHTING});
    // Overlays depth-test against the G-buffer depth after lighting has sampled it.
    const std::span<const std::string_view> voxel_overlay_task_ids = voxel_structure->get_overlay_render_task_ids();
    if (voxel_overlay_task_ids.empty()) {
        throw std::runtime_error("Failed to connect the deferred render graph: the voxel structure has no overlays.");
    }
    connect_resource_chain(p_pipeline_desc, GBUFFER_DEPTH_IMAGE_VIEW, {DIRECT_LIGHTING, voxel_overlay_task_ids.front()});
    // Overlays that draw into the swap chain after the lighting passes, in drawing order.
    std::vector<std::string> overlay_task_ids;
    for (uint32_t light_index = 0; light_index < SPOT_LIGHT_COUNT; ++light_index) {
        overlay_task_ids.push_back(make_spot_light_graph_ids(light_index).run_light_billboard);
    }
    std::vector<std::string_view> depth_overlay_chain{INDIRECT_LIGHTING};
    depth_overlay_chain.insert(depth_overlay_chain.end(), voxel_overlay_task_ids.begin(), voxel_overlay_task_ids.end());
    depth_overlay_chain.insert(depth_overlay_chain.end(), overlay_task_ids.begin(), overlay_task_ids.end());
    connect_resource_chain(p_pipeline_desc, GBUFFER_DEPTH_IMAGE_VIEW, depth_overlay_chain);

    // Lighting composition and post-processing.
    connect_resource_chain(p_pipeline_desc, DIRECT_LIGHTING_IMAGE_VIEW, {RT_RUN_DIRECT_LIGHTING, RT_COMBINE_LIGHTING});
    connect_resource_chain(p_pipeline_desc, INDIRECT_LIGHTING_IMAGE_VIEW, {RT_RUN_INDIRECT_LIGHTING, RT_COMBINE_LIGHTING});
    // The post-processing modifies the result image in place after composition, so bloom and present read its output.
    connect_resource_chain(p_pipeline_desc, RESULT_IMAGE_VIEW, {p_post_processing_last_task_id, RT_EXTRACT_BLOOM});
    connect_resource_chain(p_pipeline_desc, RESULT_IMAGE_VIEW, {p_post_processing_last_task_id, RT_RUN_PRESENT_RESULT_IMAGE});
    // The blur task ping-pongs between the bloom images internally; only its final ping result leaves the task.
    connect_resource_chain(
        p_pipeline_desc, BLOOM_PING_IMAGE_VIEW, {RT_EXTRACT_BLOOM, RT_BLUR_BLOOM, RT_ACQUIRE_BLOOM, RT_RUN_PRESENT_RESULT_IMAGE});
    // The frame sequence presents the same result and bloom into its own image, which it then acquires.
    connect_resource_chain(p_pipeline_desc, RESULT_IMAGE_VIEW, {p_post_processing_last_task_id, RT_RUN_PRESENT_FRAME_SEQUENCE});
    connect_resource_chain(p_pipeline_desc, BLOOM_PING_IMAGE_VIEW, {RT_BLUR_BLOOM, RT_RUN_PRESENT_FRAME_SEQUENCE});
    connect_resource_chain(p_pipeline_desc, FRAME_SEQUENCE_IMAGE_VIEW, {RT_RUN_PRESENT_FRAME_SEQUENCE, RT_ACQUIRE_FRAME_SEQUENCE});
    std::vector<std::string_view> swap_chain_chain{RT_RUN_PRESENT_RESULT_IMAGE};
    swap_chain_chain.insert(swap_chain_chain.end(), voxel_overlay_task_ids.begin(), voxel_overlay_task_ids.end());
    swap_chain_chain.insert(swap_chain_chain.end(), overlay_task_ids.begin(), overlay_task_ids.end());
    swap_chain_chain.push_back(RT_DRAW_IMGUI);
    connect_resource_chain(p_pipeline_desc, REND_RES_SWAP_CHAIN, swap_chain_chain);
}

void ApplicationVulkanDeferredPBRTest::add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const {
    p_pipeline_desc.add_resource(STATIC_MESH_KEY, *static_mesh_resources);
    if (mesh_acceleration_structure != MEM_NULL) {
        p_pipeline_desc.add_resource(MESH_ACCELERATION_STRUCTURE_KEY, *mesh_acceleration_structure);
    }
    dynamic_mesh->register_resources(p_pipeline_desc);
    p_pipeline_desc.add_resource(REND_RES_SWAP_CHAIN, *renderer_vk->acquire_swap_chain_image_view_container())
        .add_resource(GBUFFER_CAMERA_INFO_BUFFER, *gbuffer_camera_info_ubos)
        .add_resource(COMPOSITION_CAMERA_INFO_BUFFER, *composition_camera_info_ubos)
        .add_resource(SPOT_LIGHT_INFO_BUFFER, *spot_light_info_buffers)
        .add_resource(LIGHT_BILLBOARD_TEXTURE_IMAGE_VIEW, *light_billboard_texture_view);
    for (uint32_t light_index = 0; light_index < SPOT_LIGHT_COUNT; ++light_index) {
        const SpotLightGraphIds ids = make_spot_light_graph_ids(light_index);
        p_pipeline_desc.add_resource(ids.shadow_light_info, *shadow_light_info_buffers[light_index])
            .add_resource(ids.light_billboard_info, *light_billboard_ubos[light_index]);
    }
    add_graph_owned_resources(p_pipeline_desc);
    voxel_structure->register_render_graph_data(p_pipeline_desc);
}

void ApplicationVulkanDeferredPBRTest::add_graph_owned_resources(RendGraphDescVK& p_pipeline_desc) const {
    add_shadow_graph_resources(p_pipeline_desc);
    add_gbuffer_graph_resources(p_pipeline_desc);
    add_lighting_graph_resources(p_pipeline_desc);
}

void ApplicationVulkanDeferredPBRTest::add_shadow_graph_resources(RendGraphDescVK& p_pipeline_desc) const {
    for (uint32_t light_index = 0; light_index < SPOT_LIGHT_COUNT; ++light_index) {
        add_spot_light_shadow_graph_resources(p_pipeline_desc, light_index);
    }
    // Every light shares one shadow sampler.
    p_pipeline_desc.add_sampler(SHADOW_SAMPLER, [mip_level_count = shadow_mipmap_level_count](const RendGraphContextVK& /*p_context*/) {
        return RendSamplerDescVK{
            .address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .min_lod = 0.0F,
            .max_lod = static_cast<float>(mip_level_count - 1),
        };
    });
}

void ApplicationVulkanDeferredPBRTest::add_spot_light_shadow_graph_resources(RendGraphDescVK& p_pipeline_desc, uint32_t p_light_index) const {
    const SpotLightGraphIds ids = make_spot_light_graph_ids(p_light_index);
    RendImageDescVK shadow_moments =
        make_graph_image_desc(RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, SHADOW_MOMENTS_FORMAT, shadow_extent);
    shadow_moments.additional_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    shadow_moments.mip_levels = shadow_mipmap_level_count;

    // Shadow and reflective-shadow-map targets of this light.
    p_pipeline_desc
        .add_image(ids.shadowmap_depth_image,
            [extent = shadow_extent](const RendGraphContextVK& /*p_context*/) {
                return make_graph_image_desc(RendResPrimaryUsageVK::IMAGE_DEPTH_TARGET, SHADOWMAP_DEPTH_FORMAT, extent);
            })
        .add_image(ids.shadowmap_color_image,
            [extent = shadow_extent](const RendGraphContextVK& /*p_context*/) {
                return make_graph_image_desc(RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, RSM_FLUX_FORMAT, extent);
            })
        .add_image(ids.rsm_normal_image,
            [extent = shadow_extent](const RendGraphContextVK& /*p_context*/) {
                return make_graph_image_desc(RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, RSM_NORMAL_FORMAT, extent);
            })
        .add_image(ids.shadow_moments_image,
            [shadow_moments](const RendGraphContextVK& /*p_context*/) {
                return shadow_moments;
            })
        .add_image(ids.shadow_moments_summed_area_table_horizontal_image,
            [extent = shadow_extent](const RendGraphContextVK& /*p_context*/) {
                return make_graph_image_desc(RendResPrimaryUsageVK::IMAGE_READ_WRITE, SHADOW_MOMENTS_FORMAT, extent);
            })
        .add_image(ids.shadow_moments_summed_area_table_image,
            [extent = shadow_extent](const RendGraphContextVK& /*p_context*/) {
                return make_graph_image_desc(RendResPrimaryUsageVK::IMAGE_READ_WRITE, SHADOW_MOMENTS_FORMAT, extent);
            })
        .add_image_view(ids.shadowmap_depth, ids.shadowmap_depth_image)
        .add_image_view(ids.shadowmap_color, ids.shadowmap_color_image)
        .add_image_view(ids.rsm_normal, ids.rsm_normal_image)
        .add_image_view(ids.shadow_moments, ids.shadow_moments_image)
        .add_image_view(ids.shadow_moments_sampled,
            ids.shadow_moments_image,
            [=](const RendGraphContextVK& /*p_context*/) {
                return RendImageViewDescVK{.mip_level_count = VK_REMAINING_MIP_LEVELS};
            })
        .add_image_view(ids.shadow_moments_summed_area_table_horizontal, ids.shadow_moments_summed_area_table_horizontal_image)
        .add_image_view(ids.shadow_moments_summed_area_table, ids.shadow_moments_summed_area_table_image);
}

void ApplicationVulkanDeferredPBRTest::add_gbuffer_graph_resources(RendGraphDescVK& p_pipeline_desc) {
    constexpr std::string_view GBUFFER_ALBEDO_IMAGE = "gbuffer_albedo_image";
    constexpr std::string_view GBUFFER_NORMAL_IMAGE = "gbuffer_normal_image";
    constexpr std::string_view GBUFFER_RMA_IMAGE = "gbuffer_rma_image";
    constexpr std::string_view GBUFFER_DEPTH_IMAGE = "gbuffer_depth_image";

    // G-buffer targets.
    p_pipeline_desc
        .add_image(GBUFFER_ALBEDO_IMAGE,
            [](const RendGraphContextVK& p_context) {
                return make_graph_image_desc(
                    RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, GBUFFER_ALBEDO_FORMAT, p_context.swap_chain_extent);
            })
        .add_image(GBUFFER_NORMAL_IMAGE,
            [](const RendGraphContextVK& p_context) {
                return make_graph_image_desc(
                    RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, GBUFFER_NORMAL_FORMAT, p_context.swap_chain_extent);
            })
        .add_image(GBUFFER_RMA_IMAGE,
            [](const RendGraphContextVK& p_context) {
                return make_graph_image_desc(
                    RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, GBUFFER_RMA_FORMAT, p_context.swap_chain_extent);
            })
        .add_image(GBUFFER_DEPTH_IMAGE,
            [](const RendGraphContextVK& p_context) {
                return make_graph_image_desc(
                    RendResPrimaryUsageVK::IMAGE_DEPTH_TARGET, GBUFFER_DEPTH_FORMAT, p_context.swap_chain_extent);
            })
        .add_image_view(GBUFFER_ALBEDO_IMAGE_VIEW, GBUFFER_ALBEDO_IMAGE)
        .add_image_view(GBUFFER_NORMAL_IMAGE_VIEW, GBUFFER_NORMAL_IMAGE)
        .add_image_view(GBUFFER_RMA_IMAGE_VIEW, GBUFFER_RMA_IMAGE)
        .add_image_view(GBUFFER_DEPTH_IMAGE_VIEW, GBUFFER_DEPTH_IMAGE)
        .add_sampler(GBUFFER_SAMPLER, [=](const RendGraphContextVK& /*p_context*/) {
            return RendSamplerDescVK{
                .min_filter = VK_FILTER_NEAREST,
                .mag_filter = VK_FILTER_NEAREST,
                .address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            };
        });
}

void ApplicationVulkanDeferredPBRTest::add_lighting_graph_resources(RendGraphDescVK& p_pipeline_desc) const {
    constexpr std::string_view DIRECT_LIGHTING_IMAGE = "direct_lighting_image";
    constexpr std::string_view INDIRECT_LIGHTING_IMAGE = "indirect_lighting_image";
    constexpr std::string_view RESULT_IMAGE = "result_color_image";
    constexpr std::string_view BLOOM_PING_IMAGE = "bloom_ping_image";
    constexpr std::string_view BLOOM_PONG_IMAGE = "bloom_pong_image";
    constexpr std::string_view FRAME_SEQUENCE_IMAGE = "frame_sequence_image";
    RendImageDescVK bloom_ping =
        make_graph_image_desc(RendResPrimaryUsageVK::IMAGE_READ_WRITE, LIGHTING_OUTPUT_FORMAT, bloom_extent);
    bloom_ping.additional_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    const RendSamplerDescVK clamped_linear_sampler{
        .address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };

    // Lighting and post-processing outputs.
    p_pipeline_desc
        .add_image(DIRECT_LIGHTING_IMAGE,
            [](const RendGraphContextVK& p_context) {
                return make_graph_image_desc(
                    RendResPrimaryUsageVK::IMAGE_READ_WRITE, LIGHTING_OUTPUT_FORMAT, p_context.swap_chain_extent);
            })
        .add_image(INDIRECT_LIGHTING_IMAGE,
            [](const RendGraphContextVK& p_context) {
                return make_graph_image_desc(
                    RendResPrimaryUsageVK::IMAGE_READ_WRITE, LIGHTING_OUTPUT_FORMAT, p_context.swap_chain_extent);
            })
        .add_image(RESULT_IMAGE,
            [](const RendGraphContextVK& p_context) {
                return make_graph_image_desc(
                    RendResPrimaryUsageVK::IMAGE_READ_WRITE, LIGHTING_OUTPUT_FORMAT, p_context.swap_chain_extent);
            })
        .add_image(BLOOM_PING_IMAGE,
            [bloom_ping](const RendGraphContextVK& /*p_context*/) {
                return bloom_ping;
            })
        .add_image(BLOOM_PONG_IMAGE,
            [extent = bloom_extent](const RendGraphContextVK& /*p_context*/) {
                return make_graph_image_desc(RendResPrimaryUsageVK::IMAGE_READ_WRITE, LIGHTING_OUTPUT_FORMAT, extent);
            })
        .add_image_view(DIRECT_LIGHTING_IMAGE_VIEW, DIRECT_LIGHTING_IMAGE)
        .add_image_view(INDIRECT_LIGHTING_IMAGE_VIEW, INDIRECT_LIGHTING_IMAGE)
        .add_image_view(RESULT_IMAGE_VIEW, RESULT_IMAGE)
        .add_image_view(BLOOM_PING_IMAGE_VIEW, BLOOM_PING_IMAGE)
        .add_image(FRAME_SEQUENCE_IMAGE,
            [render_data = base_render_data](const RendGraphContextVK& p_context) {
                // The present pipeline renders in the swap-chain format, so the sequence matches the presented image.
                return make_graph_image_desc(
                    RendResPrimaryUsageVK::IMAGE_RENDER_TARGET, render_data->swap_chain_format, p_context.swap_chain_extent);
            })
        .add_image_view(BLOOM_PONG_IMAGE_VIEW, BLOOM_PONG_IMAGE)
        .add_image_view(FRAME_SEQUENCE_IMAGE_VIEW, FRAME_SEQUENCE_IMAGE)
        .add_sampler(BLOOM_SAMPLER,
            [=](const RendGraphContextVK& /*p_context*/) {
                return clamped_linear_sampler;
            })
        .add_sampler(LIGHT_BILLBOARD_SAMPLER, [=](const RendGraphContextVK& /*p_context*/) {
            return clamped_linear_sampler;
        });
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_shadow_moments_summed_area_table_task_desc(uint32_t p_light_index) const {
    const SpotLightGraphIds ids = make_spot_light_graph_ids(p_light_index);
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = ids.build_shadow_moments_summed_area_table;
    task_desc.alias(SHADOW_MOMENTS_IMAGE_VIEW, ids.shadow_moments)
        .alias(SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE_VIEW, ids.shadow_moments_summed_area_table_horizontal)
        .alias(SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW, ids.shadow_moments_summed_area_table);
    VkExtent3D horizontal_dispatch_group_count{
        .width = (shadow_extent.height + 63U) / 64U,
        .height = 1,
        .depth = 1,
    };
    VkExtent3D vertical_dispatch_group_count{
        .width = (shadow_extent.width + 63U) / 64U,
        .height = 1,
        .depth = 1,
    };
    RTRunComputePipelinesSequentialVK::Dispatch dispatch(render_allocator);
    dispatch.add_entry(shadow_moments_summed_area_table_horizontal_compute_pipeline, horizontal_dispatch_group_count)
        .add_entry(shadow_moments_summed_area_table_vertical_compute_pipeline, vertical_dispatch_group_count);
    task_desc.factory = [this, dispatch](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTRunComputePipelinesSequentialVK>(render_allocator, base_render_data, dispatch, QueueFamilyVK::COMPUTE_1);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_shadow_mipmap_task_desc(uint32_t p_light_index) const {
    const SpotLightGraphIds ids = make_spot_light_graph_ids(p_light_index);
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = ids.generate_shadow_mipmaps;
    task_desc.factory = [this, ids](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTGenerateMipmapsVK>(
            render_allocator, base_render_data, ids.shadow_moments, ids.shadow_moments_sampled, shadow_extent, shadow_mipmap_level_count, SHADOW_MOMENTS_FORMAT);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_direct_lighting_task_desc(bool p_use_summed_area_table) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_DIRECT_LIGHTING;
    Ref<ROComputePipelineVK> pipeline =
        p_use_summed_area_table ? direct_lighting_summed_area_table_compute_pipeline : direct_lighting_compute_pipeline;
    if (p_use_summed_area_table) {
        constexpr std::array<std::pair<std::string_view, std::string_view>, 4> LIGHT_RESOURCES{{
            {SHADOW_MOMENTS_IMAGE_VIEW, SHADOW_MOMENTS_IMAGE_VIEW},
            {SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW, SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW},
            {SHADOWMAP_COLOR_IMAGE_VIEW, SHADOWMAP_COLOR_IMAGE_VIEW},
            {RSM_NORMAL_IMAGE_VIEW, RSM_NORMAL_IMAGE_VIEW},
        }};
        alias_spot_light_resources(task_desc, LIGHT_RESOURCES);
    } else {
        constexpr std::array<std::pair<std::string_view, std::string_view>, 3> LIGHT_RESOURCES{{
            {SHADOW_MOMENTS_SAMPLED_IMAGE_VIEW, SHADOW_MOMENTS_SAMPLED_IMAGE_VIEW},
            {SHADOWMAP_COLOR_IMAGE_VIEW, SHADOWMAP_COLOR_IMAGE_VIEW},
            {RSM_NORMAL_IMAGE_VIEW, RSM_NORMAL_IMAGE_VIEW},
        }};
        alias_spot_light_resources(task_desc, LIGHT_RESOURCES);
    }
    task_desc.factory = [this, pipeline](const RendGraphContextVK& p_context) {
        return make_unique<RTRunComputePipelineVK>(
            render_allocator, base_render_data, pipeline, make_screen_tile_group_count(p_context), QueueFamilyVK::COMPUTE_1);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_indirect_lighting_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_INDIRECT_LIGHTING;
    // Indirect lighting only loads mip level zero of the moments, so its mip-chain slot binds the base moments view, which
    // the summed-area-table graph orders without a mip generation pass.
    constexpr std::array<std::pair<std::string_view, std::string_view>, 3> LIGHT_RESOURCES{{
        {SHADOW_MOMENTS_SAMPLED_IMAGE_VIEW, SHADOW_MOMENTS_IMAGE_VIEW},
        {SHADOWMAP_COLOR_IMAGE_VIEW, SHADOWMAP_COLOR_IMAGE_VIEW},
        {RSM_NORMAL_IMAGE_VIEW, RSM_NORMAL_IMAGE_VIEW},
    }};
    alias_spot_light_resources(task_desc, LIGHT_RESOURCES);
    task_desc.factory = [this](const RendGraphContextVK& p_context) {
        const VkExtent3D dispatch_group_count = make_screen_tile_group_count(p_context);
        Unique<IRenderTaskVK> task = make_unique<RTRunComputePipelineVK>(
            render_allocator, base_render_data, indirect_lighting_compute_pipeline, dispatch_group_count, QueueFamilyVK::COMPUTE_2);
        Unique<IRenderTaskVK> debug_view_task = make_unique<RTRunComputePipelineVK>(render_allocator,
            base_render_data,
            indirect_lighting_debug_view_compute_pipeline,
            dispatch_group_count,
            QueueFamilyVK::COMPUTE_2);
        return make_unique<VoxelDebugIndirectLightingTask>(
            render_allocator, std::move(task), std::move(debug_view_task), &voxel_debug_view_mode);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_combine_lighting_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_COMBINE_LIGHTING;
    task_desc.factory = [this](const RendGraphContextVK& p_context) {
        return make_unique<RTRunComputePipelineVK>(
            render_allocator, base_render_data, combine_lighting_compute_pipeline, make_screen_tile_group_count(p_context), QueueFamilyVK::COMPUTE_1);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_bloom_extract_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_EXTRACT_BLOOM;
    // The bloom images keep the extent of the initial swap chain, because the bloom readback buffer is sized by it.
    VkExtent3D dispatch_group_count{
        .width = (bloom_extent.width + 7U) / 8U,
        .height = (bloom_extent.height + 7U) / 8U,
        .depth = 1,
    };
    task_desc.factory = [this, dispatch_group_count](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTRunComputePipelineVK>(
            render_allocator, base_render_data, bloom_extract_compute_pipeline, dispatch_group_count, QueueFamilyVK::COMPUTE_1);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_bloom_blur_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_BLUR_BLOOM;
    task_desc.alias(KAWASE_BLUR_PING, BLOOM_PING_IMAGE_VIEW).alias(KAWASE_BLUR_PONG, BLOOM_PONG_IMAGE_VIEW).alias(KAWASE_BLUR_SAMPLER, BLOOM_SAMPLER);
    VkExtent3D dispatch_group_count{
        .width = (bloom_extent.width + 7U) / 8U,
        .height = (bloom_extent.height + 7U) / 8U,
        .depth = 1,
    };
    // Starting from ping, even iterations blur ping into pong and odd iterations blur pong back into ping, so an even
    // iteration count leaves the result in ping.
    task_desc.factory = [this, dispatch_group_count](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTPingPongVK>(render_allocator,
            base_render_data,
            kawase_blur_pong_write_compute_pipeline,
            kawase_blur_ping_write_compute_pipeline,
            false,
            BLOOM_BLUR_PASS_COUNT,
            dispatch_group_count,
            QueueFamilyVK::COMPUTE_1);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_bloom_acquire_task_desc(bool p_use_summed_area_table) {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_ACQUIRE_BLOOM;
    // The recreated task subscribes again, so bloom readback requests keep reaching it after the graph recreates tasks.
    task_desc.factory = RTTriggeredVK::wrap_factory(
        [this](const RendGraphContextVK& /*p_context*/) -> Unique<IRenderTaskVK> {
            // Completed acquisitions reach write_completed_bloom_acquire() through the event registered in setup_bloom_acquisition().
            return make_unique<RTCPUInFlightAcquireVK>(render_allocator, base_render_data, BLOOM_PING_IMAGE_VIEW);
        },
        std::string(BLOOM_ACQUIRE_EVENT_IDS[p_use_summed_area_table ? 1U : 0U]),
        base_render_data);
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_present_result_image_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_PRESENT_RESULT_IMAGE;
    task_desc.alias(RenderTargetsVK::color_id(0), REND_RES_SWAP_CHAIN);
    const std::array<VkClearValue, 1> clear_values{VkClearValue{.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}}};
    task_desc.factory = [this, clear_values](const RendGraphContextVK& p_context) {
        return RTRunGraphicsPipelineVK::Builder(render_allocator)
            .set_pipeline(present_post_processing_graphics_pipeline)
            .set_render_extent(p_context.swap_chain_extent)
            .set_clear_values(clear_values)
            .create_unique(base_render_data);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_present_frame_sequence_task_desc(bool p_use_summed_area_table) {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_PRESENT_FRAME_SEQUENCE;
    task_desc.alias(RenderTargetsVK::color_id(0), FRAME_SEQUENCE_IMAGE_VIEW);
    const std::array<VkClearValue, 1> clear_values{VkClearValue{.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}}};
    task_desc.factory = RTTriggeredVK::wrap_factory(
        [this, clear_values](const RendGraphContextVK& p_context) -> Unique<IRenderTaskVK> {
            return RTRunGraphicsPipelineVK::Builder(render_allocator)
                .set_pipeline(present_post_processing_graphics_pipeline)
                .set_render_extent(p_context.swap_chain_extent)
                .set_clear_values(clear_values)
                .create_unique(base_render_data);
        },
        std::string(FRAME_SEQUENCE_EVENT_IDS[p_use_summed_area_table ? 1U : 0U]),
        base_render_data);
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_frame_sequence_acquire_task_desc(bool p_use_summed_area_table) {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_ACQUIRE_FRAME_SEQUENCE;
    task_desc.factory = RTTriggeredVK::wrap_factory(
        [this](const RendGraphContextVK& /*p_context*/) -> Unique<IRenderTaskVK> {
            if (frame_sequence_recording) {
                // A recreated wrapper loses its remaining frames, so the sequence would end early.
                throw std::runtime_error("The deferred render graph was recreated during a frame sequence capture.");
            }
            // Completed acquisitions reach write_frame_sequence_frame() through the event registered in setup_bloom_acquisition().
            return make_unique<RTCPUInFlightAcquireVK>(render_allocator, base_render_data, FRAME_SEQUENCE_IMAGE_VIEW);
        },
        std::string(FRAME_SEQUENCE_EVENT_IDS[p_use_summed_area_table ? 1U : 0U]),
        base_render_data);
    return task_desc;
}

void ApplicationVulkanDeferredPBRTest::write_completed_bloom_acquire(const RTCPUInFlightAcquireVK::AcquiredImage& p_image) {
    std::filesystem::path output_directory = std::filesystem::path("test_env") / "descriptor_debug";
    std::filesystem::create_directories(output_directory);
    std::filesystem::path output_path = output_directory / "deferred_bloom.png";
    std::vector<uint8_t> rgba8 = convert_bloom_to_rgba8(p_image.content);
    write_image_data_png(output_path, rgba8, p_image.extent.width, p_image.extent.height, 4);
    bloom_acquire_in_flight.store(false, std::memory_order_release);
    stdout_log(WBE_CHANNEL_DEBUG)->message(std::format("Stored deferred bloom acquisition in {}.", output_path.generic_string()));
}

void ApplicationVulkanDeferredPBRTest::alias_spot_light_resources(
    RendTaskDescVK& p_task_desc, std::span<const std::pair<std::string_view, std::string_view>> p_resource_ids) {
    for (uint32_t slot = 0; slot < MAX_SPOT_LIGHT_INFO_COUNT; ++slot) {
        const uint32_t light_index = slot < SPOT_LIGHT_COUNT ? slot : 0U;
        for (const auto& [task_resource_id, graph_resource_id] : p_resource_ids) {
            p_task_desc.alias(std::format("{}[{}]", task_resource_id, slot), std::format("{}_{}", graph_resource_id, light_index));
        }
    }
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_light_billboard_task_desc(uint32_t p_light_index) const {
    const SpotLightGraphIds ids = make_spot_light_graph_ids(p_light_index);
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = ids.run_light_billboard;
    task_desc.alias(LIGHT_BILLBOARD_INFO_BUFFER, ids.light_billboard_info);
    const std::array<std::string_view, 1> color_targets{REND_RES_SWAP_CHAIN};
    alias_render_targets(task_desc, color_targets, GBUFFER_DEPTH_IMAGE_VIEW);
    task_desc.factory = [this](const RendGraphContextVK& p_context) {
        return RTRunGraphicsPipelineVK::Builder(render_allocator)
            .set_pipeline(light_billboard_pipeline)
            .set_render_extent(p_context.swap_chain_extent)
            .set_color_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .set_depth_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .set_draw_vertex_count(6)
            .create_unique(base_render_data);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_imgui_task_desc() {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_DRAW_IMGUI;
    task_desc.alias(RenderTargetsVK::color_id(0), REND_RES_SWAP_CHAIN);
    task_desc.factory = [this](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTImGuiVK>(render_allocator, base_render_data, imgui_overlay.get());
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
