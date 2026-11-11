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
#include "../entity_draw_data.hh"
#include "../render_test.hh"
#include "application_vulkan_deferred_pbr_test.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/data/data_value.hh"
#include "core/engine_core.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "deferred_recipe_context.hh"
#include "function/event/event_manager.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_bindable_render_resource_vk.hh"
#include "function/render_vk/i_render_graph_bindings_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/mesh_acceleration_structure_vk.hh"
#include "function/render_vk/rd_params_mesh_vk.hh"
#include "function/render_vk/rend_graph_recipe_builder_vk.hh"
#include "function/render_vk/rend_graph_recipe_params_vk.hh"
#include "function/render_vk/rend_graph_script_vk.hh"
#include "function/render_vk/rend_recipe_context_vk.hh"
#include "function/render_vk/rend_recipe_extent_2d_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/render_graph_bindings_function_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_targets_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/rt_build_acceleration_structures_vk.hh"
#include "function/render_vk/rt_cpu_in_flight_acquire_vk.hh"
#include "function/render_vk/rt_generate_mipmaps_vk.hh"
#include "function/render_vk/rt_multiplexer_vk.hh"
#include "function/render_vk/rt_ping_pong_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipelines_sequential_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_toggle_vk.hh"
#include "function/render_vk/rt_triggered_vk.hh"
#include "function/render_vk/vulkan_enums_vk.hh"
#include "glm/gtc/packing.hpp"
#include "glm/matrix.hpp"
#include "i_voxel_render_feature.hh"
#include "resource/resource_utils.hh"
#include "rt_imgui_vk.hh"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <optional>
#include <ratio>
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

void ApplicationVulkanDeferredPBRTest::acquire_deferred_render_graph(bool p_use_summed_area_table) {
    // The definitions differ only in their parameters; each instance's bindings prepare its context and imports for the first
    // build and every rebuild: after a swap chain resize, or when draw_pixel_sort_options() rebuilds it for new
    // post-processing pipelines.
    const size_t index = p_use_summed_area_table ? 1U : 0U;
    const std::string_view definition = DEFERRED_RENDER_GRAPHS[index];
    deferred_render_graph_bindings[index] = make_ref<RenderGraphBindingsFunctionVK>(global_allocator(),
        definition,
        [this, p_use_summed_area_table](const RenderContextVK& /*p_render_context*/, const RendRecipeContextVK& p_recipe_context) {
            return prepare_deferred_bindings(p_use_summed_area_table, p_recipe_context);
        });
    const auto start = std::chrono::steady_clock::now();
    deferred_render_graphs[index] = acquire_test_render_graph(*renderer_vk, definition, deferred_render_graph_bindings[index], false);
    if (measure_render_graphs) {
        stdout_log(WBE_CHANNEL_DEBUG)
            ->message(std::format("Render graph measurement: created '{}' in {:.3f} ms.",
                definition,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()));
    }
}

void ApplicationVulkanDeferredPBRTest::measure_render_graph_operations() {
    constexpr int REPETITIONS = 10;
    for (size_t index = 0; index < deferred_render_graphs.size(); ++index) {
        const std::string_view definition = DEFERRED_RENDER_GRAPHS[index];
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < REPETITIONS; ++i) {
            if (renderer_vk->acquire_render_graph(definition, deferred_render_graph_bindings[index]) != deferred_render_graphs[index]) {
                throw std::runtime_error("Render graph measurement: a warm acquisition compiled another instance.");
            }
        }
        const double acquire_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / REPETITIONS;
        start = std::chrono::steady_clock::now();
        for (int i = 0; i < REPETITIONS; ++i) {
            deferred_render_graphs[index]->rebuild();
        }
        const double rebuild_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / REPETITIONS;
        stdout_log(WBE_CHANNEL_DEBUG)
            ->message(std::format("Render graph measurement: '{}' warm acquisition {:.4f} ms, explicit rebuild {:.3f} ms (mean of {}).",
                definition,
                acquire_ms,
                rebuild_ms,
                REPETITIONS));
    }
}

void ApplicationVulkanDeferredPBRTest::measure_frame_time() {
    constexpr uint64_t FIRST_FRAME = 120;
    constexpr uint64_t FRAME_COUNT = 1000;
    const auto now = std::chrono::steady_clock::now();
    const uint64_t frame = render_context->current_frame;
    if (frame > FIRST_FRAME && measured_frame_count < FRAME_COUNT) {
        measured_frame_seconds += std::chrono::duration<double>(now - last_render_update).count();
        if (++measured_frame_count == FRAME_COUNT) {
            stdout_log(WBE_CHANNEL_DEBUG)
                ->message(std::format("Render graph measurement: mean frame time {:.3f} ms over {} frames.",
                    measured_frame_seconds * 1000.0 / static_cast<double>(FRAME_COUNT),
                    FRAME_COUNT));
        }
    }
    last_render_update = now;
}

RendGraphPreparedBindingsVK ApplicationVulkanDeferredPBRTest::prepare_deferred_bindings(
    bool p_use_summed_area_table, const RendRecipeContextVK& p_recipe_context) {
    RendGraphPreparedBindingsVK result;
    result.imports = make_deferred_recipe_imports(p_use_summed_area_table);
    DeferredRecipeContext context = make_deferred_recipe_context(p_use_summed_area_table, p_recipe_context);
    context.post_processing = DataValue::make_map();
    post_processing->configure_recipe(context.post_processing, result.imports);
    context.voxel = DataValue::make_map();
    voxel_structure->configure_recipe(context.voxel,
        result.imports,
        VoxelRecipeInputs{
            .dynamic_mesh = dynamic_mesh.get(),
            .static_build_event = VOXEL_STATIC_BUILD_EVENT_ID,
            .static_build_pending = voxel_static_build_pending,
        });
    result.context = RendGraphScriptVK::serialize_context(context);
    return result;
}

DeferredRecipeContext ApplicationVulkanDeferredPBRTest::make_deferred_recipe_context(
    bool p_use_summed_area_table, const RendRecipeContextVK& p_recipe_context) const {
    const auto make_extent = [](VkExtent2D p_extent) {
        return RendRecipeExtent2DVK{.width = p_extent.width, .height = p_extent.height};
    };
    const auto format = [](VkFormat p_format) {
        return static_cast<ImageFormatVK>(p_format);
    };
    DeferredRecipeContext context;
    static_cast<RendRecipeContextVK&>(context) = p_recipe_context;
    context.spot_light_count = SPOT_LIGHT_COUNT;
    context.max_spot_light_info_count = MAX_SPOT_LIGHT_INFO_COUNT;
    context.shadow_extent = make_extent(shadow_extent);
    context.shadow_mipmap_level_count = shadow_mipmap_level_count;
    context.bloom_extent = make_extent(bloom_extent);
    context.formats = DeferredRecipeFormats{
        .gbuffer_albedo = format(GBUFFER_ALBEDO_FORMAT),
        .gbuffer_normal = format(GBUFFER_NORMAL_FORMAT),
        .gbuffer_rma = format(GBUFFER_RMA_FORMAT),
        .gbuffer_depth = format(GBUFFER_DEPTH_FORMAT),
        .shadowmap_depth = format(SHADOWMAP_DEPTH_FORMAT),
        .shadow_moments = format(SHADOW_MOMENTS_FORMAT),
        .rsm_flux = format(RSM_FLUX_FORMAT),
        .rsm_normal = format(RSM_NORMAL_FORMAT),
        .lighting_output = format(LIGHTING_OUTPUT_FORMAT),
    };
    const size_t graph_index = p_use_summed_area_table ? 1U : 0U;
    context.bloom_acquire_event = BLOOM_ACQUIRE_EVENT_IDS[graph_index];
    context.bloom_image_event = BLOOM_IMAGE_EVENT_ID;
    context.frame_sequence_event = FRAME_SEQUENCE_EVENT_IDS[graph_index];
    context.has_mesh_acceleration_structure = mesh_acceleration_structure != MEM_NULL;
    context.static_mesh = static_mesh_params;
    context.dynamic_mesh = RDParamsMeshVK{.mesh = std::string(dynamic_mesh->get_mesh_key())};
    // A rebuild creates the toggled and multiplexed tasks in the mode the rendering thread applied last; later changes reach
    // them through the toggle and selection events (see apply_voxel_debug_view_mode()).
    const auto make_toggle = [this](size_t p_group) {
        return RendRecipeToggleVK{.event = std::string(VOXEL_DEBUG_VIEW_TOGGLE_EVENT_IDS[p_group]),
            .enabled = is_voxel_debug_view_toggle_enabled(p_group, applied_voxel_debug_view_mode)};
    };
    context.voxel_debug_view_toggles = VoxelDebugViewToggles{
        .lighting = RendRecipeSelectVK{.event = std::string(VOXEL_DEBUG_VIEW_LIGHTING_EVENT_ID),
            .branch = get_voxel_debug_view_lighting_branch(applied_voxel_debug_view_mode)},
        .enabled = make_toggle(0),
        .tree_view = make_toggle(1),
        .tree_draw = RendRecipeSelectVK{.event = std::string(VOXEL_DEBUG_TREE_DRAW_EVENT_ID),
            .branch = get_voxel_debug_tree_draw_branch(applied_voxel_debug_view_mode, applied_voxel_debug_draw_style)},
        .pre_deferred_propagation = make_toggle(2),
    };
    for (size_t entity = 0; entity < dynamic_mesh->get_entity_count(); ++entity) {
        context.dynamic_mesh_entity_ids.emplace_back(dynamic_mesh->get_entity_key(entity));
    }
    context.dynamic_mesh_transform_member = EntityDrawData::TRANSFORM;
    return context;
}

RendGraphRecipeImportsVK ApplicationVulkanDeferredPBRTest::make_deferred_recipe_imports(bool p_use_summed_area_table) {
    // The renderer supplies the swap chain.
    RendGraphRecipeImportsVK imports;
    imports.add(GBUFFER_CAMERA_INFO_BUFFER, *gbuffer_camera_info_ubos)
        .add(COMPOSITION_CAMERA_INFO_BUFFER, *composition_camera_info_ubos)
        .add(SPOT_LIGHT_INFO_BUFFER, *spot_light_info_buffers);
    for (uint32_t light_index = 0; light_index < SPOT_LIGHT_COUNT; ++light_index) {
        const SpotLightGraphIds ids = make_spot_light_graph_ids(light_index);
        imports.add(ids.shadow_light_info, *shadow_light_info_buffers[light_index]).add(ids.light_billboard_info, *light_billboard_ubos[light_index]);
    }
    imports.add_task(create_frame_sequence_acquire_task_desc(p_use_summed_area_table)).add_task(create_imgui_task_desc());
    if (mesh_acceleration_structure != MEM_NULL) {
        imports.add(MESH_ACCELERATION_STRUCTURE_KEY, *mesh_acceleration_structure).add_task(create_mesh_acceleration_structure_build_task_desc());
    }
    dynamic_mesh->add_imports(imports);
    return imports;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_mesh_acceleration_structure_build_task_desc() {
    // Build the acceleration structures once, on the graph's first recording. A recreated task builds them again on its
    // first recording.
    return RendTaskDescVK(render_allocator,
        RT_BUILD_MESH_ACCELERATION_STRUCTURE,
        [this, acceleration_structure = mesh_acceleration_structure](const RendGraphContextVK& /*p_context*/) {
            auto build = make_unique<RTBuildAccelerationStructuresVK>(
                render_allocator, render_context, TOP_LEVEL_ACCELERATION_STRUCTURE, [acceleration_structure](VkCommandBuffer p_command_buffer) {
                    acceleration_structure->record_build(p_command_buffer);
                });
            return make_unique<RTTriggeredVK>(render_allocator, std::move(build), true);
        });
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
            return make_unique<RTCPUInFlightAcquireVK>(
                render_allocator, render_context, FRAME_SEQUENCE_IMAGE_VIEW, FRAME_SEQUENCE_IMAGE_EVENT_ID);
        },
        std::string(FRAME_SEQUENCE_EVENT_IDS[p_use_summed_area_table ? 1U : 0U]),
        render_context);
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

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_imgui_task_desc() {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_DRAW_IMGUI;
    task_desc.alias(RenderTargetsVK::color_id(0), REND_RES_SWAP_CHAIN);
    task_desc.factory = [this](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTImGuiVK>(render_allocator, render_context, imgui_overlay.get());
    };
    return task_desc;
}

ApplicationVulkanDeferredPBRTest::CompositionCameraInfo ApplicationVulkanDeferredPBRTest::make_composition_camera_info() const {
    return CompositionCameraInfo{
        .inverse_view_projection = glm::inverse(camera_component.proj * camera_component.view),
        .camera_world_position = glm::vec4(camera_position, 1.0F),
    };
}

bool ApplicationVulkanDeferredPBRTest::is_voxel_debug_view_toggle_enabled(size_t p_group, VoxelDebugViewMode p_mode) {
    switch (p_group) {
    case 0:
        return p_mode != VoxelDebugViewMode::DISABLED;
    case 1:
        return p_mode == VoxelDebugViewMode::POST_SVO_BUILD || p_mode == VoxelDebugViewMode::OCCUPANCY;
    case 2:
        return p_mode == VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION;
    default:
        break;
    }
    throw std::runtime_error(std::format("Unknown voxel debug view toggle group {}.", p_group));
}

uint32_t ApplicationVulkanDeferredPBRTest::get_voxel_debug_view_lighting_branch(VoxelDebugViewMode p_mode) {
    return p_mode == VoxelDebugViewMode::DISABLED ? 0U : 1U;
}

uint32_t ApplicationVulkanDeferredPBRTest::get_voxel_debug_tree_draw_branch(VoxelDebugViewMode p_mode, VoxelDebugDrawStyle p_style) {
    return static_cast<uint32_t>(p_style) + (p_mode == VoxelDebugViewMode::OCCUPANCY ? 2U : 0U);
}

VoxelDebugViewMode ApplicationVulkanDeferredPBRTest::get_next_voxel_debug_view_mode(VoxelDebugViewMode p_mode) const {
    static constexpr std::array<VoxelDebugViewMode, 4> CYCLE{
        VoxelDebugViewMode::DISABLED,
        VoxelDebugViewMode::POST_SVO_BUILD,
        VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION,
        VoxelDebugViewMode::OCCUPANCY,
    };
    const auto* const current = std::ranges::find(CYCLE, p_mode);
    if (current == CYCLE.end()) {
        throw std::runtime_error("Unknown voxel debug view mode.");
    }
    size_t index = static_cast<size_t>(current - CYCLE.begin());
    do {
        index = (index + 1) % CYCLE.size();
    } while (!voxel_structure->supports_debug_view(CYCLE[index]));
    return CYCLE[index];
}

void ApplicationVulkanDeferredPBRTest::apply_voxel_debug_view_mode() {
    const VoxelDebugViewMode mode = voxel_debug_view_mode.load(std::memory_order_relaxed);
    const VoxelDebugDrawStyle style = voxel_debug_draw_style.load(std::memory_order_relaxed);
    if (mode == applied_voxel_debug_view_mode && style == applied_voxel_debug_draw_style) {
        return;
    }
    // The style selects the index count the voxel structure's tree draw setup writes and the branch that draws it, so both
    // change between the same frames.
    if (voxel_structure->supports_debug_draw_styles()) {
        voxel_structure->set_debug_draw_style(style);
    }
    // Emitted between frames, so every toggled and multiplexed task of both graphs records the new mode from the same frame
    // on.
    EventManager* event_manager = EventManager::get_singleton();
    event_manager->emit(
        VOXEL_DEBUG_VIEW_LIGHTING_EVENT_ID, RTMultiplexerVK::Command{.branch = get_voxel_debug_view_lighting_branch(mode)});
    event_manager->emit(
        VOXEL_DEBUG_TREE_DRAW_EVENT_ID, RTMultiplexerVK::Command{.branch = get_voxel_debug_tree_draw_branch(mode, style)});
    for (size_t group = 0; group < VOXEL_DEBUG_VIEW_TOGGLE_EVENT_IDS.size(); ++group) {
        event_manager->emit(VOXEL_DEBUG_VIEW_TOGGLE_EVENT_IDS[group],
            RTToggleVK::Command{.enabled = is_voxel_debug_view_toggle_enabled(group, mode)});
    }
    applied_voxel_debug_view_mode = mode;
    applied_voxel_debug_draw_style = style;
}

void ApplicationVulkanDeferredPBRTest::finish_voxel_static_build() {
    // The renderer advances current_frame only after a frame executes a graph, which records the pending construction.
    if (!voxel_static_build_pending || render_context->current_frame <= voxel_static_build_registration_frame) {
        return;
    }
    EventManager::get_singleton()->emit(VOXEL_STATIC_BUILD_EVENT_ID, RTTriggeredVK::Command::enable_for(0));
    voxel_static_build_pending = false;
    stdout_log(WBE_CHANNEL_DEBUG)->message("Voxel static construction recorded once.");
}

} // namespace WhiteBirdEngine
