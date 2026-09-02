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
#include "core/allocator/i_allocator.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/function_utils.hh"
#include "function/input_manager/input_manager.hh"
#include "function/input_manager/key_codes.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include "global/global.hh"
#include "material_bindpoint_test_utils.hh"
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

namespace {

constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOWMAP_COLOR_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_COLOR_FORMAT;

} // namespace

class ApplicationVulkanDrawCubeTest : public WBE::ApplicationGame {
public:
    virtual void logic_update(double p_delta) override;
    virtual void render_update(double p_delta) override;

    void setup_draw_task();

private:
    void setup_render_context();
    void load_resources();
    void setup_static_scene_resources();
    void setup_spot_light();
    void create_shadow_map_resources();
    void compile_shadow_pipeline();
    void compile_main_pipeline();
    void write_main_dynamic_descriptors();
    void register_draw_cube_render_graph();
    void add_render_graph_resources(WBE::RendGraphDescVK& p_pipeline_desc) const;
    std::pair<WBE::VertexInputDescriptionVK, VkDeviceSize> build_vertex_input_data(WBE::RefRes<WBE::RUGraphicsPipeline> p_pipeline) const;
    WBE::RendTaskDescVK create_shadow_task_desc() const;
    WBE::RendTaskDescVK create_main_task_desc() const;

    WBE::RendererVK* renderer_vk = nullptr;
    const WBE::BaseRenderDataVK* base_render_data = nullptr;
    WBE::RenderAllocator* render_allocator = nullptr;
    WBE::RenderDatabase* render_database = nullptr;
    WBE::RefRes<WBE::RUMesh3D> cube_mesh;
    WBE::RefRes<WBE::RUMaterial> default_material;
    WBE::RefRes<WBE::RUGraphicsPipeline> shadow_pipeline_resource;

    // Kept alive for the lifetime of the test because graph resources and pipeline-owned descriptors retain these objects.
    WBE::Ref<WBE::ROGraphicsPipelineVK> shadow_graphics_pipeline = WBE::MEM_NULL;
    WBE::Ref<WBE::ROGraphicsPipelineVK> main_graphics_pipeline = WBE::MEM_NULL;
    WBE::GraphicsPipelineCompilerVK::DescriptorSetBindings main_descriptor_set_bindings;
    WBE::Ref<WBE::ROImageVK> shadow_depth_image;
    WBE::Ref<WBE::ROImageVK> shadow_color_image;
    WBE::Ref<WBE::ROImageViewVK> shadow_depth_view;
    WBE::Ref<WBE::ROImageViewVK> shadow_color_view;
    WBE::Ref<WBE::ROSamplerVK> shadow_sampler;
    WBE::Ref<WBE::ROSamplerVK> material_sampler;
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    WBE::ComponentSpotLight spot_light_component{};
    VkExtent2D shadow_extent{};
    WBE::ComponentCamera camera_component{};
    glm::vec3 camera_position = TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = default_test_camera_yaw();
    float camera_pitch = default_test_camera_pitch();
};

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Directory({"test_env"}));
    {
        ApplicationVulkanDrawCubeTest game_application;
        {
            WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiler");
            game_application.setup_draw_task();
            game_application.run();
            size_t renderer_alloc_max = game_application.get_renderer_allocator()->get_internal_fragmentation_tracker();
            auto global_alloc_max_by_arena = WBE::EngineCore::get_singleton()->pool_allocator->get_internal_fragmentation_tracker();
            for (size_t arena_index = 0; arena_index < global_alloc_max_by_arena.size(); ++arena_index) {
                WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)
                    ->message(std::format("Global allocator arena {} max used: {}", arena_index, global_alloc_max_by_arena[arena_index]));
            }
            WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator max used: {}", renderer_alloc_max));
        }
    }
    const auto& global_profiles = global->engine_core->profiling_manager->get_profile_data(WBE::WBE_CHANNEL_GLOBAL);
    for (const auto& profile : global_profiles) {
        WBE::stdout_log(WBE::WBE_CHANNEL_GLOBAL)->message(std::format("{}: Delta: {} seconds", profile.message, profile.delta));
    }
    return EXIT_SUCCESS;
}

void ApplicationVulkanDrawCubeTest::logic_update(double p_delta) {
    WBE::InputManager* input_manager = WBE::InputManager::get_singleton();
    if (input_manager == nullptr) {
        return;
    }

    float delta = static_cast<float>(p_delta);
    if (input_manager->get_input_status(WBE::KEYCODE_LEFT).is_down()) {
        camera_yaw -= TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_RIGHT).is_down()) {
        camera_yaw += TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_UP).is_down()) {
        camera_pitch += TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_DOWN).is_down()) {
        camera_pitch -= TEST_CAMERA_TURN_SPEED * delta;
    }
    camera_pitch = std::clamp(camera_pitch, -TEST_CAMERA_PITCH_LIMIT, TEST_CAMERA_PITCH_LIMIT);

    glm::vec3 forward = test_camera_forward(camera_yaw, camera_pitch);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    float move_delta = TEST_CAMERA_MOVE_SPEED * delta;

    if (input_manager->get_input_status(WBE::KEYCODE_W).is_down()) {
        camera_position += forward * move_delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_S).is_down()) {
        camera_position -= forward * move_delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_D).is_down()) {
        camera_position += right * move_delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_A).is_down()) {
        camera_position -= right * move_delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_E).is_down()) {
        camera_position += up * move_delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_Q).is_down()) {
        camera_position -= up * move_delta;
    }

    camera_component.view = test_camera_view(camera_position, camera_yaw, camera_pitch);

    WBE::ROBufferVK* camera_info_buffer = camera_info_ubo.get();
    if (camera_info_buffer == nullptr) {
        return;
    }
    WBE::CameraInfoVKUBO camera_info = make_camera_info_ubo(camera_component);
    add_render_job([camera_info, camera_info_buffer]() {
        camera_info_buffer->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
    });
}

void ApplicationVulkanDrawCubeTest::render_update(double /* p_delta */) {
}

void ApplicationVulkanDrawCubeTest::setup_draw_task() {
    setup_render_context();
    load_resources();
    setup_static_scene_resources();
    setup_spot_light();
    create_shadow_map_resources();
    compile_shadow_pipeline();
    compile_main_pipeline();
    write_main_dynamic_descriptors();
    register_draw_cube_render_graph();
}

void ApplicationVulkanDrawCubeTest::setup_render_context() {
    renderer_vk = WBE::RendererVK::get_singleton();
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = get_renderer_allocator().get();
    render_database = WBE::RenderDatabase::get_singleton();
}

void ApplicationVulkanDrawCubeTest::load_resources() {
    auto* resource_manager = WBE::ResourceManager::get_singleton();
    cube_mesh = resource_manager->get_resource<WBE::RUMesh3D>("cube");
    if (cube_mesh == nullptr) {
        throw std::runtime_error("Failed to setup draw cube test: cube mesh resource was not found.");
    }
    default_material = resource_manager->get_resource<WBE::RUMaterial>("default_material");
    if (default_material == nullptr || default_material->graphics_pipeline == nullptr ||
        default_material->graphics_pipeline->resource_key == nullptr) {
        throw std::runtime_error("Failed to setup draw cube test: default material has no compiled graphics pipeline key.");
    }
    shadow_pipeline_resource = resource_manager->get_resource<WBE::RUGraphicsPipeline>("shadowmap_spot");
    if (shadow_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup draw cube test: 'shadowmap_spot' graphics pipeline resource not found.");
    }
}

void ApplicationVulkanDrawCubeTest::setup_static_scene_resources() {
    render_database->set_static_opaque_triangles(cube_mesh.get());
    renderer_vk->update_static_opaque_triangles();
}

void ApplicationVulkanDrawCubeTest::setup_spot_light() {
    spot_light_component = make_default_test_spot_light_component(32.0F);
    shadow_extent = VkExtent2D{
        .width = spot_light_component.shadow_map_resolution,
        .height = spot_light_component.shadow_map_resolution,
    };
}

void ApplicationVulkanDrawCubeTest::create_shadow_map_resources() {

    shadow_depth_image = WBE::ROImageVK::Builder().shadow_map_depth(spot_light_component).create_ref(base_render_data);
    shadow_color_image = WBE::ROImageVK::Builder().shadow_map_color(spot_light_component).create_ref(base_render_data);

    VkImageSubresourceRange depth_subresource{
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    VkImageSubresourceRange color_subresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    shadow_depth_view = WBE::ROImageViewVK::Builder()
                            .set_image(shadow_depth_image)
                            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                            .set_format(SHADOWMAP_DEPTH_FORMAT)
                            .set_subresource_range(depth_subresource)
                            .create_ref(base_render_data);
    shadow_color_view = WBE::ROImageViewVK::Builder()
                            .set_image(shadow_color_image)
                            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                            .set_format(SHADOWMAP_COLOR_FORMAT)
                            .set_subresource_range(color_subresource)
                            .create_ref(base_render_data);

    shadow_sampler = WBE::ROSamplerVK::Builder()
                         .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                         .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                         .create_ref(base_render_data);
    material_sampler = WBE::ROSamplerVK::Builder()
                           .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                           .set_address_mode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                           .create_ref(base_render_data);
}

void ApplicationVulkanDrawCubeTest::compile_shadow_pipeline() {
    WBE::GraphicsPipelineCompilerVKContext shadow_compiler_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {SHADOWMAP_COLOR_FORMAT},
        .depth_attachment_format = SHADOWMAP_DEPTH_FORMAT,
    };
    WBE::RenderBindpointDeclarationsBuilder shadow_bindpoint_builder;
    shadow_bindpoint_builder
        .add_buffer_decleration("light_camera_info.view", &spot_light_component.view, sizeof(spot_light_component.view))
        .add_buffer_decleration("light_camera_info.proj", &spot_light_component.proj, sizeof(spot_light_component.proj));
    auto shadow_bindpoint_declarations = shadow_bindpoint_builder.build();
    auto compiled =
        WBE::GraphicsPipelineCompilerVK(shadow_compiler_context).compile("shadowmap_spot", shadow_pipeline_resource, shadow_bindpoint_declarations);
    shadow_graphics_pipeline = compiled.graphics_pipeline;
}

void ApplicationVulkanDrawCubeTest::compile_main_pipeline() {
    camera_component = make_default_test_camera_component(base_render_data->swap_chain_aspect_ratio);
    WBE::RenderBindpointDeclarationsBuilder bindpoint_builder;
    WBE::SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    uint32_t spot_light_count = 1;
    auto material_params = add_material_texture_bindings(bindpoint_builder, default_material, default_material, material_sampler);
    bindpoint_builder.add_buffer_decleration("camera_info.view", &camera_component.view, sizeof(camera_component.view))
        .add_buffer_decleration("camera_info.proj", &camera_component.proj, sizeof(camera_component.proj))
        .add_buffer_decleration("light.lights[0].info", &spot_light_info, sizeof(spot_light_info))
        .add_buffer_decleration("light.light_count", &spot_light_count, sizeof(spot_light_count))
        .add_image_decleration("light.lights[0].shadowmap", shadow_depth_view)
        .add_sampler_decleration("light.lights[0].shadowmap_sampler", shadow_sampler)
        .add_buffer_decleration("material_params", material_params.data(), material_params.size());
    auto main_bindpoint_declarations = bindpoint_builder.build();

    WBE::GraphicsPipelineCompilerVKContext main_compiler_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
    };
    auto compiled = WBE::GraphicsPipelineCompilerVK(main_compiler_context)
                        .compile(default_material->graphics_pipeline->resource_key, default_material->graphics_pipeline, main_bindpoint_declarations);
    main_graphics_pipeline = compiled.graphics_pipeline;
    main_descriptor_set_bindings = std::move(compiled.descriptor_set_bindings);
}

void ApplicationVulkanDrawCubeTest::write_main_dynamic_descriptors() {
    camera_info_ubo = WBE::ROBufferVK::Builder()
                          .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                          .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                          .set_size(sizeof(WBE::CameraInfoVKUBO))
                          .create_ref(base_render_data);
    WBE::CameraInfoVKUBO camera_info = make_camera_info_ubo(camera_component);
    camera_info_ubo->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
    auto [set, binding] = main_descriptor_set_bindings.at("camera_info");
    main_graphics_pipeline->write_descriptor_set(set,
        WBE::RODescriptorSetVK::Writer().set_binding(binding).add_uniform_buffer_info(WBE::RODescriptorSetVK::Writer::BufferInfo{
            .buffer = camera_info_ubo,
            .offset = 0,
            .range = sizeof(WBE::CameraInfoVKUBO),
        }));
}

void ApplicationVulkanDrawCubeTest::register_draw_cube_render_graph() {
    WBE::RendGraphDescVK pipeline_desc(render_allocator);
    add_render_graph_resources(pipeline_desc);
    pipeline_desc.tasks.push_back(create_shadow_task_desc());
    pipeline_desc.tasks.push_back(create_main_task_desc());
    renderer_vk->register_render_graph("draw_cube_pipeline", std::move(pipeline_desc));
    renderer_vk->activate_render_graph("draw_cube_pipeline");
}

void ApplicationVulkanDrawCubeTest::add_render_graph_resources(WBE::RendGraphDescVK& p_pipeline_desc) const {
    p_pipeline_desc.add_resource(REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE, shadow_graphics_pipeline)
        .add_resource(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, shadow_color_view)
        .add_resource(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, shadow_depth_view)
        .add_resource(REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE, main_graphics_pipeline);
}

std::pair<WBE::VertexInputDescriptionVK, VkDeviceSize> ApplicationVulkanDrawCubeTest::build_vertex_input_data(
    WBE::RefRes<WBE::RUGraphicsPipeline> p_pipeline) const {
    if (p_pipeline == nullptr || p_pipeline->vertex_stage == nullptr) {
        throw std::runtime_error("Failed to setup draw cube test: vertex shader resource is missing.");
    }
    const auto& triangles_info = render_database->get_static_opaque_triangles_info();
    if (triangles_info.draw_groups.size() != 1) {
        throw std::runtime_error("Failed to setup draw cube test: expected one vertex draw group.");
    }
    std::string_view entry_name;
    if (p_pipeline->vertex_stage_entry != nullptr && p_pipeline->vertex_stage_entry.get()[0] != '\0') {
        entry_name = p_pipeline->vertex_stage_entry.get();
    } else if (p_pipeline->vertex_stage->vertex_entry != nullptr && p_pipeline->vertex_stage->vertex_entry.get()[0] != '\0') {
        entry_name = p_pipeline->vertex_stage->vertex_entry.get();
    }
    const auto& draw_group = triangles_info.draw_groups[0];
    return {WBE::build_vertex_input_description_vk(
                WBE::ShaderMetadataLoader().load(p_pipeline->vertex_stage), entry_name, draw_group.vertex_layout),
        static_cast<VkDeviceSize>(draw_group.vertex_buffer_byte_offset)};
}

WBE::RendTaskDescVK ApplicationVulkanDrawCubeTest::create_shadow_task_desc() const {
    auto [vertex_input, vertex_buffer_offset] = build_vertex_input_data(shadow_pipeline_resource);
    WBE::RendTaskDescVK shadow_task_desc(render_allocator);
    shadow_task_desc.task_id = RT_RUN_PIPELINE_SHADOWMAP;
    shadow_task_desc.create = [task_render_allocator = render_allocator,
                                  task_base_render_data = base_render_data,
                                  task_shadow_extent = shadow_extent,
                                  vertex_input,
                                  vertex_buffer_offset]() -> WBE::Unique<WBE::RenderTaskVK> {
        std::array<VkClearValue, 2> shadow_clear_values{};
        shadow_clear_values[0].color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}};
        shadow_clear_values[1].depthStencil = {.depth = 0.0F, .stencil = 0};
        std::array<std::string_view, 1> shadow_color_attachment_ids{REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW};
        return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            std::span<const VkClearValue>(shadow_clear_values.data(), shadow_clear_values.size()),
            task_shadow_extent,
            std::span<const std::string_view>(shadow_color_attachment_ids.data(), shadow_color_attachment_ids.size()),
            REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW,
            REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE,
            WBE::REND_RES_STATIC_VERTICES,
            WBE::REND_RES_STATIC_INDICES,
            std::nullopt,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            vertex_input,
            vertex_buffer_offset);
    };
    return shadow_task_desc;
}

WBE::RendTaskDescVK ApplicationVulkanDrawCubeTest::create_main_task_desc() const {
    auto [vertex_input, vertex_buffer_offset] = build_vertex_input_data(default_material->graphics_pipeline);
    WBE::RendTaskDescVK main_task_desc(render_allocator);
    main_task_desc.task_id = RT_RUN_PIPELINE;
    main_task_desc.after(RT_RUN_PIPELINE_SHADOWMAP);
    main_task_desc.create = [task_render_allocator = render_allocator,
                                task_base_render_data = base_render_data,
                                vertex_input,
                                vertex_buffer_offset]() -> WBE::Unique<WBE::RenderTaskVK> {
        return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
            task_base_render_data->swap_chain_extent,
            WBE::REND_RES_SWAP_CHAIN_IMAGE,
            REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE,
            WBE::REND_RES_STATIC_VERTICES,
            WBE::REND_RES_STATIC_INDICES,
            std::nullopt,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            vertex_input,
            vertex_buffer_offset);
    };
    return main_task_desc;
}
