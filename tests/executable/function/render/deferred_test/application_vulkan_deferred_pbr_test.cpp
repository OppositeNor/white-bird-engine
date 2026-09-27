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
#include "application_vulkan_deferred_pbr_test.hh"
#include "../render_test.hh"
#include "core/allocator/i_allocator.hh"
#include "core/engine_core.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "function/ecs/components.hh"
#include "function/input_manager/input_manager.hh"
#include "function/input_manager/key_codes.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/static_set_compiler_vk.hh"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "platform/file_system/path.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <limits>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

constexpr glm::vec2 LIGHT_BILLBOARD_SIZE = glm::vec2(0.35F, 0.35F);
struct BillboardInfoVKUBO {
    glm::vec4 position = glm::vec4(0.0F);
    glm::vec2 size = glm::vec2(0.0F);
    glm::vec2 size_padding = glm::vec2(0.0F);
    glm::vec4 rotation_axis = glm::vec4(0.0F);
};

BillboardInfoVKUBO make_light_billboard_info(const ComponentSpotLight& p_spot_light_component) {
    return BillboardInfoVKUBO{
        .position = glm::vec4(p_spot_light_component.position, 1.0F),
        .size = LIGHT_BILLBOARD_SIZE,
        .rotation_axis = glm::vec4(0.0F, 1.0F, 0.0F, 0.0F),
    };
}

SpotLightInfosVKUBO make_spot_light_infos_ubo(const ComponentSpotLight& p_spot_light_component) {
    SpotLightInfosVKUBO result;
    result.spot_light_info[0] = make_spot_light_info_ubo(p_spot_light_component);
    result.spot_light_count = 1;
    return result;
}

Ref<ROCMultislotsVK> create_per_frame_buffers(
    RenderAllocator* p_allocator, const BaseRenderDataVK* p_render_data, VkDeviceSize p_buffer_size, VkBufferUsageFlags p_usage) {
    return make_ref<ROCMultislotsVK>(p_allocator,
        p_allocator,
        p_render_data->frames_in_flight,
        [p_render_data, p_buffer_size, p_usage](uint32_t /*p_frame_slot*/) -> Ref<IRenderObjectVK> {
            return ROBufferVK::Builder().set_properties(p_buffer_size, p_usage).create_ref(p_render_data);
        });
}

} // namespace

void ApplicationVulkanDeferredPBRTest::logic_update(double p_delta) {
    write_completed_bloom_acquire();
    control(p_delta);
    update_descriptors();
    dynamic_mesh_time += p_delta;
    const float angle = static_cast<float>(dynamic_mesh_time);
    std::array<glm::mat4, 2> transforms {};
    transforms[0] = glm::translate(glm::mat4(1.0F), glm::vec3(0.6F * std::sin(angle * 0.5F), 0.75F, 0.0F)) *
                    glm::rotate(glm::mat4(1.0F), angle, glm::vec3(0.0F, 1.0F, 0.0F)) *
                    glm::scale(glm::mat4(1.0F), glm::vec3(0.3F, 0.45F, 0.3F));
    transforms[1] = glm::translate(glm::mat4(1.0F), glm::vec3(-1.2F, 0.0F, -0.5F)) * transforms[0];
    add_render_job([this, transforms]() {
        dynamic_mesh->publish_visible_transforms(transforms);
    });
}

void ApplicationVulkanDeferredPBRTest::control(double p_delta) { // NOLINT
    InputManager* input_manager = InputManager::get_singleton();
    if (input_manager == nullptr) {
        return;
    }

    bool render_graph_switch_key_down = input_manager->get_input_status(KEYCODE_R).is_down();
    if (render_graph_switch_key_down && !render_graph_switch_key_was_down) {
        bool requested = !summed_area_table_requested.load(std::memory_order_relaxed);
        summed_area_table_requested.store(requested, std::memory_order_relaxed);
    }
    render_graph_switch_key_was_down = render_graph_switch_key_down;

    bool voxel_debug_view_key_down = input_manager->get_input_status(KEYCODE_V).is_down();
    if (voxel_debug_view_key_down && !voxel_debug_view_key_was_down) {
        VoxelDebugViewMode mode = voxel_debug_view_mode.load(std::memory_order_relaxed);
        switch (mode) {
        case VoxelDebugViewMode::DISABLED:
            mode = VoxelDebugViewMode::POST_OCT_TREE_BUILD;
            break;
        case VoxelDebugViewMode::POST_OCT_TREE_BUILD:
            mode = VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION;
            break;
        case VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION:
            mode = VoxelDebugViewMode::DISABLED;
            break;
        }
        voxel_debug_view_mode.store(mode, std::memory_order_relaxed);
    }
    voxel_debug_view_key_was_down = voxel_debug_view_key_down;

    bool frame_capture_key_down = input_manager->get_input_status(KEYCODE_F1).is_down();
    if (frame_capture_key_down && !frame_capture_key_was_down) {
        frame_capture_requested.store(true, std::memory_order_relaxed);
    }
    frame_capture_key_was_down = frame_capture_key_down;

    bool bloom_acquire_key_down = input_manager->get_input_status(KEYCODE_B).is_down();
    if (bloom_acquire_key_down && !bloom_acquire_key_was_down && !bloom_acquire_in_flight) {
        bloom_acquire_requested.store(true, std::memory_order_release);
        bloom_acquire_in_flight = true;
    }
    bloom_acquire_key_was_down = bloom_acquire_key_down;

    float delta = static_cast<float>(p_delta);
    if (input_manager->get_input_status(KEYCODE_LEFT).is_down()) {
        camera_yaw -= TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(KEYCODE_RIGHT).is_down()) {
        camera_yaw += TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(KEYCODE_UP).is_down()) {
        camera_pitch += TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(KEYCODE_DOWN).is_down()) {
        camera_pitch -= TEST_CAMERA_TURN_SPEED * delta;
    }
    camera_pitch = std::clamp(camera_pitch, -TEST_CAMERA_PITCH_LIMIT, TEST_CAMERA_PITCH_LIMIT);

    glm::vec3 forward = test_camera_forward(camera_yaw, camera_pitch);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    float move_delta = TEST_CAMERA_MOVE_SPEED * delta;

    if (input_manager->get_input_status(KEYCODE_W).is_down()) {
        camera_position += forward * move_delta;
    }
    if (input_manager->get_input_status(KEYCODE_S).is_down()) {
        camera_position -= forward * move_delta;
    }
    if (input_manager->get_input_status(KEYCODE_D).is_down()) {
        camera_position += right * move_delta;
    }
    if (input_manager->get_input_status(KEYCODE_A).is_down()) {
        camera_position -= right * move_delta;
    }
    if (input_manager->get_input_status(KEYCODE_E).is_down()) {
        camera_position += up * move_delta;
    }
    if (input_manager->get_input_status(KEYCODE_Q).is_down()) {
        camera_position -= up * move_delta;
    }

    bool spot_light_changed = false;
    if (input_manager->get_input_status(KEYCODE_I).is_down()) {
        spot_light_component.position.z -= move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(KEYCODE_K).is_down()) {
        spot_light_component.position.z += move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(KEYCODE_J).is_down()) {
        spot_light_component.position.x -= move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(KEYCODE_L).is_down()) {
        spot_light_component.position.x += move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(KEYCODE_U).is_down()) {
        spot_light_component.position.y += move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(KEYCODE_O).is_down()) {
        spot_light_component.position.y -= move_delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(KEYCODE_Y).is_down()) {
        spot_light_pitch += TEST_CAMERA_TURN_SPEED * delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(KEYCODE_P).is_down()) {
        spot_light_pitch -= TEST_CAMERA_TURN_SPEED * delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(KEYCODE_H).is_down()) {
        spot_light_yaw -= TEST_CAMERA_TURN_SPEED * delta;
        spot_light_changed = true;
    }
    if (input_manager->get_input_status(KEYCODE_SEMICOLON).is_down()) {
        spot_light_yaw += TEST_CAMERA_TURN_SPEED * delta;
        spot_light_changed = true;
    }
    if (spot_light_changed) {
        spot_light_pitch = std::clamp(spot_light_pitch, -TEST_CAMERA_PITCH_LIMIT, TEST_CAMERA_PITCH_LIMIT);
        spot_light_component.direction = test_camera_forward(spot_light_yaw, spot_light_pitch);
        glm::vec3 light_up =
            std::abs(spot_light_component.direction.y) > 0.99F ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
        spot_light_component.view =
            glm::lookAt(spot_light_component.position, spot_light_component.position + spot_light_component.direction, light_up);
    }
}

void ApplicationVulkanDeferredPBRTest::update_descriptors() {
    camera_component.view = test_camera_view(camera_position, camera_yaw, camera_pitch);
    CameraInfoVKUBO gbuffer_camera_info = make_camera_info_ubo(camera_component);
    CompositionCameraInfo composition_camera_info = make_composition_camera_info();
    SpotLightInfosVKUBO spot_light_infos = make_spot_light_infos_ubo(spot_light_component);
    BillboardInfoVKUBO light_billboard_info = make_light_billboard_info(spot_light_component);
    add_render_job([this, gbuffer_camera_info, composition_camera_info, spot_light_infos, light_billboard_info]() {
        if (gbuffer_camera_info_ubos == MEM_NULL || composition_camera_info_ubos == MEM_NULL ||
            spot_light_info_buffers == MEM_NULL || light_billboard_ubo == MEM_NULL) {
            throw std::runtime_error("Failed to update deferred descriptors: per-frame buffers are not initialized.");
        }
        gbuffer_camera_info_ubos->publish_buffer_update(&gbuffer_camera_info, 0, sizeof(gbuffer_camera_info));
        composition_camera_info_ubos->publish_buffer_update(&composition_camera_info, 0, sizeof(composition_camera_info));
        spot_light_info_buffers->publish_buffer_update(
            spot_light_infos.spot_light_info.data(), 0, sizeof(spot_light_infos.spot_light_info));
        light_billboard_ubo->update_buffer(&light_billboard_info, sizeof(light_billboard_info));
    });
}

void ApplicationVulkanDeferredPBRTest::render_update(double /* p_delta */) {
    if (frame_capture_requested.exchange(false, std::memory_order_relaxed)) {
        const Path output_directory = std::filesystem::absolute(Path("test_env") / "captures");
        std::filesystem::create_directories(output_directory);
        const auto timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const Path output_path =
            output_directory / std::format("deferred_{}_frame_{}.png", timestamp, base_render_data->current_frame);
        renderer_vk->request_frame_capture(output_path.string());
    } else {
        WBE::on_exec_test_render(*renderer_vk, base_render_data->current_frame);
    }
    bool requested = summed_area_table_requested.load(std::memory_order_relaxed);
    if (requested == summed_area_table_active) {
        return;
    }
    renderer_vk->activate_render_graph(requested ? DEFERRED_SUMMED_AREA_TABLE_RENDER_GRAPH : DEFERRED_MIPMAP_RENDER_GRAPH);
    summed_area_table_active = requested;
    stdout_log(WBE_CHANNEL_DEBUG)->message(requested ? "Deferred shadow filtering: summed-area table." : "Deferred shadow filtering: mipmaps.");
}

void ApplicationVulkanDeferredPBRTest::setup_draw_task() {
    setup_render_context();
    load_resources();
    setup_static_scene_resources();
    dynamic_mesh = make_unique<DynamicMeshRenderFeature>(render_allocator,
        renderer_vk,
        ResourceManager::get_singleton()->get_resource<RUMesh3D>("model_cube.mesh"),
        gbuffer_opaque_pipeline_resource,
        2);
    setup_spot_light();
    create_shadow_map_resources();
    create_gbuffer_resources();
    create_lighting_output_resources();
    OctTreeRenderFeature::Implementation oct_tree_implementation = OctTreeRenderFeature::Implementation::PASS_PUBLISHED;
    if (const char* selection = std::getenv("WBE_OCT_TREE_IMPLEMENTATION")) {
        if (std::string_view(selection) == "1") {
            oct_tree_implementation = OctTreeRenderFeature::Implementation::ATOMIC;
        }
        else if (std::string_view(selection) != "2") {
            throw std::runtime_error("WBE_OCT_TREE_IMPLEMENTATION must be 1 or 2.");
        }
    }
    oct_tree = make_unique<OctTreeRenderFeature>(render_allocator, renderer_vk, static_set_resources,
        &voxel_debug_view_mode, oct_tree_implementation);
    stdout_log(WBE_CHANNEL_DEBUG)->message(oct_tree_implementation == OctTreeRenderFeature::Implementation::PASS_PUBLISHED
        ? "Octree construction: oct_tree_2 (pass publication)." : "Octree construction: oct_tree (atomic publication).");
    compile_shadow_moments_summed_area_table_pipelines();
    compile_lighting_pipelines();
    auto mipmap_shadow_opaque_task = create_shadow_task(false);
    auto summed_area_table_shadow_opaque_task = create_shadow_task(false);
    auto mipmap_gbuffer_opaque_task = create_gbuffer_task(false);
    auto summed_area_table_gbuffer_opaque_task = create_gbuffer_task(false);
    write_dynamic_camera_descriptors();
    write_dynamic_light_descriptors();
    setup_light_billboard_overlay_resources();
    register_deferred_render_graph(DEFERRED_MIPMAP_RENDER_GRAPH, false, mipmap_shadow_opaque_task, mipmap_gbuffer_opaque_task);
    register_deferred_render_graph(
        DEFERRED_SUMMED_AREA_TABLE_RENDER_GRAPH, true, summed_area_table_shadow_opaque_task, summed_area_table_gbuffer_opaque_task);
    renderer_vk->activate_render_graph(DEFERRED_MIPMAP_RENDER_GRAPH);
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_gbuffer_task(bool p_load_existing, bool p_double_sided) const {
    VkClearValue color_clear{};
    VkClearValue depth_clear{.depthStencil = {.depth = 0.0F, .stencil = 0}};
    std::array<VkClearValue, 4> clear_values{color_clear, color_clear, color_clear, depth_clear};
    std::array<std::string_view, 3> color_attachments{GBUFFER_ALBEDO_IMAGE_VIEW, GBUFFER_NORMAL_IMAGE_VIEW, GBUFFER_RMA_IMAGE_VIEW};
    RendTaskDescVK descriptor = make_static_set_task_desc(render_allocator,
        base_render_data,
        p_double_sided ? RT_RUN_DEFERRED_GBUFFER_DOUBLE_SIDED : RT_RUN_DEFERRED_GBUFFER,
        static_set_resources,
        p_double_sided ? gbuffer_double_sided_graphics_pipeline : gbuffer_opaque_graphics_pipeline,
        false,
        clear_values,
        base_render_data->swap_chain_extent,
        color_attachments,
        GBUFFER_DEPTH_IMAGE_VIEW,
        p_load_existing ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        p_load_existing ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        p_double_sided);
    return descriptor;
}

void ApplicationVulkanDeferredPBRTest::setup_render_context() {
    renderer_vk = RendererVK::get_singleton();
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = get_renderer_allocator().get();
    camera_component = make_default_test_camera_component(base_render_data->swap_chain_aspect_ratio, 100.0F);
}

void ApplicationVulkanDeferredPBRTest::load_resources() {
    auto* resource_manager = ResourceManager::get_singleton();
    stdout_log(WBE_CHANNEL_DEBUG)->message("Loading render resources...");
    auto resource_chunk = make_ref<ResourceChunkLoadFile>(
        global_allocator(), "res_chunks/rendering_test_resources.json", resource_manager->get_allocator());
    stdout_log(WBE_CHANNEL_DEBUG)->message("Finished loading render resources.");
    resource_manager->register_chunk(resource_chunk);
    sponza_opaque_set = resource_manager->get_resource<RUStaticOpaqueSet>("intel_sponza_with_curtains.static_opaque_set");
    sponza_opaque_double_sided_set =
        resource_manager->get_resource<RUStaticOpaqueDoubleSidedSet>("intel_sponza_with_curtains.static_opaque_double_sided_set");
    shadow_double_sided_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("rsm_spot_opaque_double_sided_static");
    gbuffer_double_sided_pipeline_resource =
        resource_manager->get_resource<RUGraphicsPipeline>("deferred_gbuffer_opaque_double_sided_static");
    shadow_opaque_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("rsm_spot_static");
    gbuffer_opaque_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("deferred_gbuffer_static");
    shadow_moments_summed_area_table_horizontal_shader_resource =
        resource_manager->get_resource<RUShader>("shadow_moments_summed_area_table_horizontal_shader");
    shadow_moments_summed_area_table_vertical_shader_resource =
        resource_manager->get_resource<RUShader>("shadow_moments_summed_area_table_vertical_shader");
    direct_lighting_shader_resource = resource_manager->get_resource<RUShader>("direct_lighting_shader");
    direct_lighting_summed_area_table_shader_resource =
        resource_manager->get_resource<RUShader>("direct_lighting_summed_area_table_shader");
    indirect_lighting_rsm_shader_resource = resource_manager->get_resource<RUShader>("indirect_lighting_rsm_shader");
    indirect_lighting_rsm_debug_view_shader_resource =
        resource_manager->get_resource<RUShader>("indirect_lighting_rsm_debug_view_shader");
    combine_lighting_shader_resource = resource_manager->get_resource<RUShader>("combine_lighting_shader");
    bloom_extract_shader_resource = resource_manager->get_resource<RUShader>("bloom_shader_extract");
    bloom_ping_write_shader_resource = resource_manager->get_resource<RUShader>("bloom_shader_ping_write");
    bloom_pong_write_shader_resource = resource_manager->get_resource<RUShader>("bloom_shader_pong_write");

    present_post_processing_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("present_post_processing");
}

void ApplicationVulkanDeferredPBRTest::setup_static_scene_resources() {
    static_set_resources = create_static_set_resources(
        renderer_vk, sponza_opaque_set, MEM_NULL, gbuffer_opaque_pipeline_resource, MEM_NULL, gbuffer_opaque_pipeline_resource, sponza_opaque_double_sided_set);
    GraphicsPipelineCompilerVKContext shadow_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {RSM_FLUX_FORMAT, RSM_NORMAL_FORMAT, SHADOW_MOMENTS_FORMAT},
        .depth_attachment_format = SHADOWMAP_DEPTH_FORMAT,
    };
    shadow_opaque_graphics_pipeline =
        GraphicsPipelineCompilerVK(shadow_context).compile("deferred_shadow_opaque", shadow_opaque_pipeline_resource).graphics_pipeline;
    shadow_double_sided_graphics_pipeline = GraphicsPipelineCompilerVK(shadow_context)
                                                .compile("deferred_shadow_double_sided", shadow_double_sided_pipeline_resource)
                                                .graphics_pipeline;
    GraphicsPipelineCompilerVKContext gbuffer_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {GBUFFER_ALBEDO_FORMAT, GBUFFER_NORMAL_FORMAT, GBUFFER_RMA_FORMAT},
        .depth_attachment_format = GBUFFER_DEPTH_FORMAT,
    };
    gbuffer_opaque_graphics_pipeline =
        GraphicsPipelineCompilerVK(gbuffer_context).compile("deferred_gbuffer_opaque", gbuffer_opaque_pipeline_resource).graphics_pipeline;
    gbuffer_double_sided_graphics_pipeline = GraphicsPipelineCompilerVK(gbuffer_context)
                                                 .compile("deferred_gbuffer_double_sided", gbuffer_double_sided_pipeline_resource)
                                                 .graphics_pipeline;

}

void ApplicationVulkanDeferredPBRTest::setup_spot_light() {
    spot_light_component = make_default_test_spot_light_component(128.0F);
    spot_light_yaw = std::atan2(spot_light_component.direction.z, spot_light_component.direction.x);
    spot_light_pitch = std::asin(std::clamp(spot_light_component.direction.y, -1.0F, 1.0F));
    shadow_extent = VkExtent2D{
        .width = spot_light_component.shadow_map_resolution,
        .height = spot_light_component.shadow_map_resolution,
    };
    shadow_mipmap_level_count = std::bit_width(std::max(shadow_extent.width, shadow_extent.height));
}

void ApplicationVulkanDeferredPBRTest::setup_light_billboard_overlay_resources() {
    auto* resource_manager = ResourceManager::get_singleton();
    RefRes<RUGraphicsPipeline> pipeline_resource =
        resource_manager->get_resource<RUGraphicsPipeline>("billboard_face_camera_depth_tested_pipeline");
    if (pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred light billboard overlay: graphics pipeline resource was not found.");
    }
    RefRes<RUTexture> texture_resource = resource_manager->get_resource<RUTexture>("spot_light_billboard");
    if (texture_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred light billboard overlay: texture resource was not found.");
    }

    BillboardInfoVKUBO light_billboard_info = make_light_billboard_info(spot_light_component);
    light_billboard_ubo = ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .initial_data(&light_billboard_info, sizeof(light_billboard_info))
                              .create_ref(base_render_data);
    light_billboard_sampler = ROSamplerVK::Builder()
                                  .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                                  .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                                  .create_ref(base_render_data);

    GraphicsPipelineCompilerVKContext compiler_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
        .depth_attachment_format = GBUFFER_DEPTH_FORMAT,
    };
    GraphicsPipelineCompilerVK compiler(compiler_context);
    light_billboard_texture_view = compiler.compile_texture_view("spot_light_billboard", texture_resource);
    auto compiled = compiler.compile("deferred_light_billboard_pipeline", pipeline_resource);
    light_billboard_pipeline = compiled.graphics_pipeline;
}

void ApplicationVulkanDeferredPBRTest::create_shadow_map_resources() {
    shadow_depth_image = ROImageVK::Builder().shadow_map_depth(spot_light_component).create_ref(base_render_data);
    shadow_color_image = ROImageVK::Builder()
                             .set_properties_2d(RSM_FLUX_FORMAT, shadow_extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                             .create_ref(base_render_data);
    shadow_normal_image = ROImageVK::Builder()
                              .set_properties_2d(RSM_NORMAL_FORMAT, shadow_extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                              .create_ref(base_render_data);
    shadow_moments_image = ROImageVK::Builder()
                               .set_properties_2d(SHADOW_MOMENTS_FORMAT,
                                   shadow_extent,
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                               .set_mipmap_level(shadow_mipmap_level_count)
                               .create_ref(base_render_data);
    shadow_moments_summed_area_table_horizontal_image =
        ROImageVK::Builder()
            .set_properties_2d(SHADOW_MOMENTS_FORMAT, shadow_extent, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
            .create_ref(base_render_data);
    shadow_moments_summed_area_table_image =
        ROImageVK::Builder()
            .set_properties_2d(SHADOW_MOMENTS_FORMAT, shadow_extent, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
            .create_ref(base_render_data);
    shadow_depth_view = ROImageViewVK::Builder()
                            .set_image(shadow_depth_image)
                            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                            .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                            .set_format(SHADOWMAP_DEPTH_FORMAT)
                            .create_ref(base_render_data);
    shadow_color_view = ROImageViewVK::Builder()
                            .set_image(shadow_color_image)
                            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                            .set_format(RSM_FLUX_FORMAT)
                            .create_ref(base_render_data);
    shadow_normal_view = ROImageViewVK::Builder()
                             .set_image(shadow_normal_image)
                             .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                             .set_format(RSM_NORMAL_FORMAT)
                             .create_ref(base_render_data);
    shadow_moments_attachment_view = ROImageViewVK::Builder()
                                         .set_image(shadow_moments_image)
                                         .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                         .set_format(SHADOW_MOMENTS_FORMAT)
                                         .create_ref(base_render_data);
    shadow_moments_sampled_view = ROImageViewVK::Builder()
                                      .set_image(shadow_moments_image)
                                      .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                      .set_format(SHADOW_MOMENTS_FORMAT)
                                      .set_mip_level_count(shadow_mipmap_level_count)
                                      .create_ref(base_render_data);
    shadow_moments_summed_area_table_horizontal_view = ROImageViewVK::Builder()
                                                           .set_image(shadow_moments_summed_area_table_horizontal_image)
                                                           .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                                           .set_format(SHADOW_MOMENTS_FORMAT)
                                                           .create_ref(base_render_data);
    shadow_moments_summed_area_table_view = ROImageViewVK::Builder()
                                                .set_image(shadow_moments_summed_area_table_image)
                                                .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                                .set_format(SHADOW_MOMENTS_FORMAT)
                                                .create_ref(base_render_data);
    shadow_sampler = ROSamplerVK::Builder()
                         .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                         .set_mipmap_mode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                         .set_lod_range(0.0F, static_cast<float>(shadow_mipmap_level_count - 1))
                         .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                         .create_ref(base_render_data);
}

void ApplicationVulkanDeferredPBRTest::create_gbuffer_resources() {
    VkExtent2D extent = base_render_data->swap_chain_extent;
    gbuffer_albedo_image =
        ROImageVK::Builder()
            .set_properties_2d(GBUFFER_ALBEDO_FORMAT, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
            .create_ref(base_render_data);
    gbuffer_normal_image =
        ROImageVK::Builder()
            .set_properties_2d(GBUFFER_NORMAL_FORMAT, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
            .create_ref(base_render_data);
    gbuffer_rma_image = ROImageVK::Builder()
                            .set_properties_2d(GBUFFER_RMA_FORMAT, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                            .create_ref(base_render_data);
    gbuffer_depth_image = ROImageVK::Builder()
                              .set_properties_2d(GBUFFER_DEPTH_FORMAT, extent, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                              .create_ref(base_render_data);
    gbuffer_albedo_view = ROImageViewVK::Builder()
                              .set_image(gbuffer_albedo_image)
                              .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                              .set_format(GBUFFER_ALBEDO_FORMAT)
                              .create_ref(base_render_data);
    gbuffer_normal_view = ROImageViewVK::Builder()
                              .set_image(gbuffer_normal_image)
                              .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                              .set_format(GBUFFER_NORMAL_FORMAT)
                              .create_ref(base_render_data);
    gbuffer_rma_view = ROImageViewVK::Builder()
                           .set_image(gbuffer_rma_image)
                           .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                           .set_format(GBUFFER_RMA_FORMAT)
                           .create_ref(base_render_data);
    gbuffer_depth_view = ROImageViewVK::Builder()
                             .set_image(gbuffer_depth_image)
                             .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                             .set_format(GBUFFER_DEPTH_FORMAT)
                             .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                             .create_ref(base_render_data);
    gbuffer_sampler = ROSamplerVK::Builder()
                          .set_filter(VK_FILTER_NEAREST, VK_FILTER_NEAREST)
                          .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                          .create_ref(base_render_data);
}

void ApplicationVulkanDeferredPBRTest::create_lighting_output_resources() {
    VkExtent2D extent = base_render_data->swap_chain_extent;
    bloom_extent = extent;
    if (bloom_extent.width > std::numeric_limits<size_t>::max() / bloom_extent.height / BLOOM_BYTES_PER_PIXEL) {
        throw std::runtime_error("Failed to allocate the deferred bloom acquisition buffer: image byte size is too large.");
    }
    bloom_acquire_buffer.resize(static_cast<size_t>(bloom_extent.width) * bloom_extent.height * BLOOM_BYTES_PER_PIXEL);
    direct_lighting_image = ROImageVK::Builder()
                                .set_properties_2d(LIGHTING_OUTPUT_FORMAT, extent, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                .create_ref(base_render_data);
    indirect_lighting_image = ROImageVK::Builder()
                                  .set_properties_2d(LIGHTING_OUTPUT_FORMAT, extent, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                  .create_ref(base_render_data);
    result_image = ROImageVK::Builder()
                       .set_properties_2d(LIGHTING_OUTPUT_FORMAT, extent, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                       .create_ref(base_render_data);
    bloom_ping_image = ROImageVK::Builder()
                           .set_properties_2d(LIGHTING_OUTPUT_FORMAT,
                               bloom_extent,
                               VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                           .create_ref(base_render_data);
    bloom_pong_image = ROImageVK::Builder()
                           .set_properties_2d(LIGHTING_OUTPUT_FORMAT, bloom_extent, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                           .create_ref(base_render_data);
    direct_lighting_view = ROImageViewVK::Builder()
                               .set_image(direct_lighting_image)
                               .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                               .set_format(LIGHTING_OUTPUT_FORMAT)
                               .create_ref(base_render_data);
    indirect_lighting_view = ROImageViewVK::Builder()
                                 .set_image(indirect_lighting_image)
                                 .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                 .set_format(LIGHTING_OUTPUT_FORMAT)
                                 .create_ref(base_render_data);
    result_image_view = ROImageViewVK::Builder()
                            .set_image(result_image)
                            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                            .set_format(LIGHTING_OUTPUT_FORMAT)
                            .create_ref(base_render_data);
    bloom_ping_image_view = ROImageViewVK::Builder()
                                .set_image(bloom_ping_image)
                                .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                .set_format(LIGHTING_OUTPUT_FORMAT)
                                .create_ref(base_render_data);
    bloom_pong_image_view = ROImageViewVK::Builder()
                                .set_image(bloom_pong_image)
                                .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                .set_format(LIGHTING_OUTPUT_FORMAT)
                                .create_ref(base_render_data);
    bloom_sampler = ROSamplerVK::Builder()
                        .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                        .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                        .create_ref(base_render_data);
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_shadow_task(bool p_load_existing, bool p_double_sided) const {
    VkClearValue shadow_color_clear{};
    VkClearValue shadow_normal_clear{};
    VkClearValue shadow_moments_clear{};
    shadow_moments_clear.color.float32[0] = 1.0F;
    shadow_moments_clear.color.float32[1] = 1.0F;
    VkClearValue shadow_depth_clear{.depthStencil = {.depth = 0.0F, .stencil = 0}};
    std::array<VkClearValue, 4> clear_values{shadow_color_clear, shadow_normal_clear, shadow_moments_clear, shadow_depth_clear};
    std::array<std::string_view, 3> color_attachments{SHADOWMAP_COLOR_IMAGE_VIEW, RSM_NORMAL_IMAGE_VIEW, SHADOW_MOMENTS_IMAGE_VIEW};
    RendTaskDescVK descriptor = make_static_set_task_desc(render_allocator,
        base_render_data,
        p_double_sided ? RT_RUN_DEFERRED_SHADOW_DOUBLE_SIDED : RT_RUN_DEFERRED_SHADOW_OPAQUE,
        static_set_resources,
        p_double_sided ? shadow_double_sided_graphics_pipeline : shadow_opaque_graphics_pipeline,
        false,
        clear_values,
        shadow_extent,
        color_attachments,
        SHADOWMAP_DEPTH_IMAGE_VIEW,
        p_load_existing ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        p_load_existing ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        p_double_sided);
    return descriptor;
}

void ApplicationVulkanDeferredPBRTest::compile_shadow_moments_summed_area_table_pipelines() {
    ComputePipelineCompilerVKContext compute_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
    };

    auto horizontal_compiled =
        ComputePipelineCompilerVK(compute_context).compile("shadow_moments_summed_area_table_horizontal_shader", shadow_moments_summed_area_table_horizontal_shader_resource);
    shadow_moments_summed_area_table_horizontal_compute_pipeline = horizontal_compiled.compute_pipeline;

    auto vertical_compiled =
        ComputePipelineCompilerVK(compute_context).compile("shadow_moments_summed_area_table_vertical_shader", shadow_moments_summed_area_table_vertical_shader_resource);
    shadow_moments_summed_area_table_vertical_compute_pipeline = vertical_compiled.compute_pipeline;
}

Ref<ROBufferVK> ApplicationVulkanDeferredPBRTest::create_initialized_buffer(
    VkBufferUsageFlags p_buffer_usage, const void* p_data, size_t p_size) const {
    return ROBufferVK::Builder().set_usage(p_buffer_usage).initial_data(p_data, p_size).create_ref(base_render_data);
}

ComputePipelineCompilerVKContext ApplicationVulkanDeferredPBRTest::make_compute_pipeline_context() const {
    return ComputePipelineCompilerVKContext{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
    };
}

GraphicsPipelineCompilerVKContext ApplicationVulkanDeferredPBRTest::make_present_graphics_pipeline_context() const {
    return GraphicsPipelineCompilerVKContext{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
    };
}

void ApplicationVulkanDeferredPBRTest::compile_lighting_pipelines() {
    composition_camera_info_ubos = create_per_frame_buffers(
        render_allocator, base_render_data, sizeof(CompositionCameraInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    spot_light_info_buffers = create_per_frame_buffers(render_allocator,
        base_render_data,
        sizeof(SpotLightInfoVKUBO) * MAX_SPOT_LIGHT_INFO_COUNT,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    uint32_t spot_light_count = 1;
    spot_light_count_buffer =
        create_initialized_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &spot_light_count, sizeof(spot_light_count));
    ComputePipelineCompilerVKContext compute_context = make_compute_pipeline_context();
    auto direct_compiled = ComputePipelineCompilerVK(compute_context).compile("direct_lighting_shader", direct_lighting_shader_resource);
    direct_lighting_compute_pipeline = direct_compiled.compute_pipeline;
    auto direct_summed_area_table_compiled =
        ComputePipelineCompilerVK(compute_context).compile("direct_lighting_summed_area_table_shader", direct_lighting_summed_area_table_shader_resource);
    direct_lighting_summed_area_table_compute_pipeline = direct_summed_area_table_compiled.compute_pipeline;
    auto indirect_compiled =
        ComputePipelineCompilerVK(compute_context).compile("indirect_lighting_rsm_shader", indirect_lighting_rsm_shader_resource);
    indirect_lighting_compute_pipeline = indirect_compiled.compute_pipeline;
    auto indirect_debug_view_compiled =
        ComputePipelineCompilerVK(compute_context).compile("indirect_lighting_rsm_debug_view_shader", indirect_lighting_rsm_debug_view_shader_resource);
    indirect_lighting_debug_view_compute_pipeline = indirect_debug_view_compiled.compute_pipeline;
    auto combine_compiled =
        ComputePipelineCompilerVK(compute_context).compile("combine_lighting_shader", combine_lighting_shader_resource);
    combine_lighting_compute_pipeline = combine_compiled.compute_pipeline;
    auto bloom_extract_compiled =
        ComputePipelineCompilerVK(compute_context).compile("bloom_shader_extract", bloom_extract_shader_resource);
    bloom_extract_compute_pipeline = bloom_extract_compiled.compute_pipeline;
    auto bloom_ping_write_compiled =
        ComputePipelineCompilerVK(compute_context).compile("bloom_shader_ping_write", bloom_ping_write_shader_resource);
    bloom_ping_write_compute_pipeline = bloom_ping_write_compiled.compute_pipeline;
    auto bloom_pong_write_compiled =
        ComputePipelineCompilerVK(compute_context).compile("bloom_shader_pong_write", bloom_pong_write_shader_resource);
    bloom_pong_write_compute_pipeline = bloom_pong_write_compiled.compute_pipeline;
    auto present_compiled =
        GraphicsPipelineCompilerVK(make_present_graphics_pipeline_context()).compile("present_post_processing", present_post_processing_pipeline_resource);
    present_post_processing_graphics_pipeline = present_compiled.graphics_pipeline;
}

void ApplicationVulkanDeferredPBRTest::write_dynamic_camera_descriptors() {
    gbuffer_camera_info_ubos =
        create_per_frame_buffers(render_allocator, base_render_data, sizeof(CameraInfoVKUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    CameraInfoVKUBO gbuffer_camera_info = make_camera_info_ubo(camera_component);
    CompositionCameraInfo composition_camera_info = make_composition_camera_info();

    gbuffer_camera_info_ubos->publish_buffer_update(&gbuffer_camera_info, 0, sizeof(gbuffer_camera_info));
    composition_camera_info_ubos->publish_buffer_update(&composition_camera_info, 0, sizeof(composition_camera_info));
    log_uniform_buffer_descriptors(gbuffer_camera_info_ubos, "G-buffer camera buffer");
    log_uniform_buffer_descriptors(composition_camera_info_ubos, "Composition camera buffer");
}

void ApplicationVulkanDeferredPBRTest::write_dynamic_light_descriptors() {
    SpotLightInfosVKUBO spot_light_infos = make_spot_light_infos_ubo(spot_light_component);

    spot_light_info_buffers->publish_buffer_update(spot_light_infos.spot_light_info.data(), 0, sizeof(spot_light_infos.spot_light_info));
    log_uniform_buffer_descriptors(spot_light_info_buffers, "Spot light buffer");
}

} // namespace WhiteBirdEngine
