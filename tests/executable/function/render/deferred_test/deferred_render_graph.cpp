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
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/rt_cpu_in_flight_acquire_vk.hh"
#include "function/render_vk/rt_generate_mipmaps_vk.hh"
#include "function/render_vk/rt_ping_pong_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipelines_sequential_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "glm/gtc/packing.hpp"
#include "glm/matrix.hpp"
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

    Vector<ResAccessVK, RenderAllocator> read_resources() const override {
        return task->read_resources();
    }

    Vector<ResAccessVK, RenderAllocator> write_resources() const override {
        return task->write_resources();
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
    auto shadow_double_sided_task = create_shadow_task(true, true);
    shadow_double_sided_task.after(RT_RUN_DEFERRED_SHADOW_OPAQUE);
    pipeline_desc.add_task(std::move(shadow_double_sided_task));
    if (p_use_summed_area_table) {
        pipeline_desc.add_task(create_shadow_moments_summed_area_table_task_desc());
    } else {
        pipeline_desc.add_task(create_shadow_mipmap_task_desc());
    }
    p_gbuffer_opaque_task.after(RT_RUN_DEFERRED_SHADOW_DOUBLE_SIDED);
    pipeline_desc.add_task(std::move(p_gbuffer_opaque_task));
    auto gbuffer_double_sided_task = create_gbuffer_task(true, true);
    gbuffer_double_sided_task.after(RT_RUN_DEFERRED_GBUFFER);
    pipeline_desc.add_task(std::move(gbuffer_double_sided_task));
    pipeline_desc.add_task(create_direct_lighting_task_desc(p_use_summed_area_table));
    oct_tree->add_construction_render_tasks(pipeline_desc, RT_RUN_DIRECT_LIGHTING);
    pipeline_desc.add_task(oct_tree->get_deferred_construction_render_task_desc());
    pipeline_desc.add_task(oct_tree->get_debug_draw_setup_render_task_desc())
        .add_task(create_indirect_lighting_task_desc(p_use_summed_area_table))
        .add_task(create_combine_lighting_task_desc())
        .add_task(create_bloom_extract_task_desc())
        .add_task(create_bloom_blur_task_desc())
        .add_task(create_bloom_acquire_task_desc())
        .add_task(create_present_result_image_task_desc())
        .add_task(oct_tree->get_debug_draw_render_task_desc(RT_RUN_PRESENT_RESULT_IMAGE))
        .add_task(oct_tree->get_pre_deferred_debug_draw_render_task_desc())
        .add_task(create_light_billboard_task_desc());
    Ref<RenderGraphVK> graph = renderer_vk->register_render_graph(p_graph_id, std::move(pipeline_desc));
    write_descriptor_debug_output(std::format("{}_graph_descriptors", p_graph_id), graph->list_descriptors());
}

void ApplicationVulkanDeferredPBRTest::add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const {
    add_static_set_graph_resources(p_pipeline_desc, static_set_resources);
    p_pipeline_desc.add_resource(REND_RES_SWAP_CHAIN, renderer_vk->acquire_swap_chain_image_view_container())
        .add_resource(SHADOWMAP_COLOR_IMAGE_VIEW, shadow_color_view)
        .add_resource(RSM_NORMAL_IMAGE_VIEW, shadow_normal_view)
        .add_resource(SHADOW_MOMENTS_IMAGE_VIEW, shadow_moments_attachment_view)
        .add_resource(SHADOW_MOMENTS_SAMPLED_IMAGE_VIEW, shadow_moments_sampled_view)
        .add_resource(SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE_VIEW, shadow_moments_summed_area_table_horizontal_view)
        .add_resource(SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW, shadow_moments_summed_area_table_view)
        .add_resource(SHADOWMAP_DEPTH_IMAGE_VIEW, shadow_depth_view)
        .add_resource(GBUFFER_ALBEDO_IMAGE_VIEW, gbuffer_albedo_view)
        .add_resource(GBUFFER_NORMAL_IMAGE_VIEW, gbuffer_normal_view)
        .add_resource(GBUFFER_RMA_IMAGE_VIEW, gbuffer_rma_view)
        .add_resource(GBUFFER_DEPTH_IMAGE_VIEW, gbuffer_depth_view)
        .add_resource(DIRECT_LIGHTING_IMAGE_VIEW, direct_lighting_view)
        .add_resource(INDIRECT_LIGHTING_IMAGE_VIEW, indirect_lighting_view)
        .add_resource(RESULT_IMAGE_VIEW, result_image_view)
        .add_resource(BLOOM_PING_IMAGE_VIEW, bloom_ping_image_view)
        .add_resource(BLOOM_PONG_IMAGE_VIEW, bloom_pong_image_view)
        .add_resource(GBUFFER_CAMERA_INFO_BUFFER, gbuffer_camera_info_ubos)
        .add_resource(COMPOSITION_CAMERA_INFO_BUFFER, composition_camera_info_ubos)
        .add_resource(SHADOW_LIGHT_INFO_BUFFER, spot_light_info_buffers)
        .add_resource(SPOT_LIGHT_INFO_BUFFER, spot_light_info_buffers)
        .add_resource(SPOT_LIGHT_COUNT_BUFFER, spot_light_count_buffer)
        .add_resource(SHADOW_SAMPLER, shadow_sampler)
        .add_resource(GBUFFER_SAMPLER, gbuffer_sampler)
        .add_resource(BLOOM_SAMPLER, bloom_sampler)
        .add_resource(LIGHT_BILLBOARD_INFO_BUFFER, light_billboard_ubo)
        .add_resource(LIGHT_BILLBOARD_TEXTURE_IMAGE_VIEW, light_billboard_texture_view)
        .add_resource(LIGHT_BILLBOARD_SAMPLER, light_billboard_sampler);
    oct_tree->register_render_graph_data(p_pipeline_desc);
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_shadow_moments_summed_area_table_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE;
    task_desc.after(RT_RUN_DEFERRED_SHADOW_DOUBLE_SIDED)
        .read(SHADOW_MOMENTS_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE)
        .write(SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
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
    task_desc.after(RT_RUN_DEFERRED_SHADOW_DOUBLE_SIDED);
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
    task_desc.after(RT_RUN_DEFERRED_GBUFFER_DOUBLE_SIDED)
        .after(p_use_summed_area_table ? RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE : RT_GENERATE_SHADOW_MIPMAPS)
        .read(SHADOWMAP_COLOR_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(RSM_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_ALBEDO_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_RMA_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_DEPTH_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(DIRECT_LIGHTING_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    if (p_use_summed_area_table) {
        task_desc.read(SHADOW_MOMENTS_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE).read(SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE);
    } else {
        task_desc.read(SHADOW_MOMENTS_SAMPLED_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE);
    }
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

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_indirect_lighting_task_desc(bool p_use_summed_area_table) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_INDIRECT_LIGHTING;
    task_desc.after(RT_RUN_DEFERRED_GBUFFER_DOUBLE_SIDED)
        .after(OctTreeRenderFeature::RT_RUN_BUILD_OCT_TREE_DEFERRED)
        .read(SHADOW_MOMENTS_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(SHADOWMAP_COLOR_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(RSM_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_ALBEDO_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_RMA_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_DEPTH_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(INDIRECT_LIGHTING_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    if (!p_use_summed_area_table) {
        task_desc.after(RT_GENERATE_SHADOW_MIPMAPS);
    } else {
        task_desc.after(RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE);
    }
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
    task_desc.after(RT_RUN_DIRECT_LIGHTING)
        .after(RT_RUN_INDIRECT_LIGHTING)
        .read(GBUFFER_RMA_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(DIRECT_LIGHTING_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(INDIRECT_LIGHTING_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(RESULT_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
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
    task_desc.after(RT_COMBINE_LIGHTING)
        .read(RESULT_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(BLOOM_PING_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
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
    task_desc.after(RT_EXTRACT_BLOOM)
        .read(BLOOM_PING_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(BLOOM_PONG_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(BLOOM_PING_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE)
        .write(BLOOM_PONG_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_bloom_extent = bloom_extent,
                           ping_write_pipeline = bloom_ping_write_compute_pipeline,
                           pong_write_pipeline = bloom_pong_write_compute_pipeline]() -> Unique<IRenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_bloom_extent.width + 7U) / 8U,
            .height = (task_bloom_extent.height + 7U) / 8U,
            .depth = 1,
        };
        RTRunComputePipelinesSequentialVK::Dispatch dispatch(task_render_allocator);
        for (uint32_t pass_index = 0; pass_index < BLOOM_BLUR_PASS_COUNT; ++pass_index) {
            dispatch.add_entry(pass_index % 2U == 0U ? pong_write_pipeline : ping_write_pipeline, dispatch_group_count);
        }
        return make_unique<RTRunComputePipelinesSequentialVK>(task_render_allocator, task_base_render_data, dispatch);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_bloom_acquire_task_desc() {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_ACQUIRE_BLOOM;
    task_desc.after(RT_BLUR_BLOOM);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_destination = std::span<uint8_t>(bloom_acquire_buffer),
                           task_acquire_requested = &bloom_acquire_requested,
                           task_acquire_completed = &bloom_acquire_completed]() -> Unique<IRenderTaskVK> {
        return make_unique<RTCPUInFlightAcquireVK>(task_render_allocator,
            task_base_render_data,
            BLOOM_PING_IMAGE_VIEW,
            task_destination,
            task_acquire_requested,
            task_acquire_completed);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_present_result_image_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_PRESENT_RESULT_IMAGE;
    task_desc.after(RT_ACQUIRE_BLOOM)
        .read(RESULT_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(BLOOM_PING_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = present_post_processing_graphics_pipeline]() -> Unique<IRenderTaskVK> {
        return make_unique<RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
            task_base_render_data->swap_chain_extent,
            REND_RES_SWAP_CHAIN,
            task_pipeline,
            VK_ATTACHMENT_LOAD_OP_CLEAR);
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
    task_desc.after(OctTreeRenderFeature::RT_RUN_OCT_TREE_PRE_DEFERRED_DEBUG_DRAW);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = light_billboard_pipeline]() -> Unique<IRenderTaskVK> {
        std::array<VkClearValue, 2> clear_values{};
        std::array<std::string_view, 1> color_attachment_ids{REND_RES_SWAP_CHAIN};
        return make_unique<RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            clear_values,
            task_base_render_data->swap_chain_extent,
            color_attachment_ids,
            GBUFFER_DEPTH_IMAGE_VIEW,
            task_pipeline,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VertexInputDescriptionVK{},
            0,
            6);
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
