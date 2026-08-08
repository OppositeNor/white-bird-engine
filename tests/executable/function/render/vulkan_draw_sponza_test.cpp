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
#include "generated/type_uuid.gen.hh"
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
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
constexpr std::string_view REND_RES_TEST_DRAW_SPONZA_GRAPHICS_PIPELINE = "REND_RES_TEST_DRAW_SPONZA_GRAPHICS_PIPELINE";
constexpr std::string_view REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW = "REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW";

} // namespace WhiteBirdEngine

namespace WBE = WhiteBirdEngine;

namespace {

constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOWMAP_COLOR_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_COLOR_FORMAT;
constexpr VkFormat MAIN_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

} // namespace

class ApplicationVulkanDrawSponzaTest : public WBE::ApplicationGame {
public:
    virtual void logic_update(double p_delta) override;
    virtual void render_update(double p_delta) override;

    void setup_draw_task();

private:
    struct GraphicsPipeline {
        WBE::RenderDatabase::SubRegion draw_sub_region{};
        WBE::Ref<WBE::ROGraphicsPipelineVK> graphics_pipeline;
        std::vector<WBE::Ref<WBE::RODescriptorSetVK>> descriptor_sets;
        WBE::GraphicsPipelineCompilerVK::DescriptorSetBindings descriptor_set_bindings;
    };
    void setup_render_context();
    void load_resources();
    void setup_static_scene_resources();
    void setup_spot_light();
    void create_shadow_map_resources();
    void create_main_depth_resources();
    void compile_shadow_pipeline();
    GraphicsPipeline compile_main_pipeline(const WBE::RenderDatabase::OpaqueTrianglesInfo::MaterialUnit& p_material_unit);
    void write_main_dynamic_descriptors(GraphicsPipeline& p_pipeline);
    void register_draw_sponza_render_graph();
    void add_render_graph_resources(WBE::RendGraphDescVK& p_pipeline_desc, uint32_t p_graphics_pipeline_index) const;
    WBE::RendTaskDescVK create_shadow_task_desc() const;
    WBE::RendTaskDescVK create_main_task_desc(uint32_t p_pipeline_index) const;

    WBE::RendererVK* renderer_vk = nullptr;
    const WBE::BaseRenderDataVK* base_render_data = nullptr;
    WBE::RenderAllocator* render_allocator = nullptr;
    WBE::RenderDatabase* render_database = nullptr;
    WBE::RefRes<WBE::RUMesh3D> sponza_mesh;
    WBE::RefRes<WBE::RUMaterial> default_material;
    WBE::RefRes<WBE::RUGraphicsPipeline> shadow_pipeline_resource;

    // Kept alive for the lifetime of the test because the graph resources and
    // the main pipeline's descriptor set reference these by raw handles.
    WBE::Ref<WBE::ROGraphicsPipelineVK> shadow_graphics_pipeline = WBE::MEM_NULL;
    std::vector<GraphicsPipeline> main_graphics_pipelines;
    std::vector<WBE::Ref<WBE::RODescriptorSetVK>> shadow_descriptor_sets;
    WBE::Ref<WBE::ROImageVK> shadow_depth_image;
    WBE::Ref<WBE::ROImageVK> shadow_color_image;
    WBE::Ref<WBE::ROImageViewVK> shadow_depth_view;
    WBE::Ref<WBE::ROImageViewVK> shadow_color_view;
    WBE::Ref<WBE::ROImageVK> main_depth_image;
    WBE::Ref<WBE::ROImageViewVK> main_depth_view;
    WBE::Ref<WBE::ROSamplerVK> shadow_sampler;
    WBE::Ref<WBE::ROSamplerVK> material_sampler;
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    WBE::ComponentSpotLight spot_light_component{};
    VkExtent2D shadow_extent{};
    WBE::ComponentCamera camera_component{};
    glm::vec3 camera_position = WBE::TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = WBE::default_test_camera_yaw();
    float camera_pitch = WBE::default_test_camera_pitch();
};

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Directory({"test_env"}));
    {
        ApplicationVulkanDrawSponzaTest game_application;
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

void ApplicationVulkanDrawSponzaTest::logic_update(double p_delta) {
    WBE::InputManager* input_manager = WBE::InputManager::get_singleton();
    if (input_manager == nullptr) {
        return;
    }

    float delta = static_cast<float>(p_delta);
    if (input_manager->get_input_status(WBE::KEYCODE_LEFT).is_down()) {
        camera_yaw -= WBE::TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_RIGHT).is_down()) {
        camera_yaw += WBE::TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_UP).is_down()) {
        camera_pitch += WBE::TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(WBE::KEYCODE_DOWN).is_down()) {
        camera_pitch -= WBE::TEST_CAMERA_TURN_SPEED * delta;
    }
    camera_pitch = std::clamp(camera_pitch, -WBE::TEST_CAMERA_PITCH_LIMIT, WBE::TEST_CAMERA_PITCH_LIMIT);

    glm::vec3 forward = WBE::test_camera_forward(camera_yaw, camera_pitch);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    float move_delta = WBE::TEST_CAMERA_MOVE_SPEED * delta;

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

    camera_component.view = WBE::test_camera_view(camera_position, camera_yaw, camera_pitch);

    WBE::ROBufferVK* camera_info_buffer = camera_info_ubo.get();
    if (camera_info_buffer == nullptr) {
        return;
    }
    WBE::CameraInfoVKUBO camera_info = WBE::make_camera_info_ubo(camera_component);
    add_render_job([camera_info, camera_info_buffer]() {
        camera_info_buffer->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
    });
}

void ApplicationVulkanDrawSponzaTest::render_update(double /* p_delta */) {
}

void ApplicationVulkanDrawSponzaTest::setup_draw_task() {
    setup_render_context();
    load_resources();
    setup_static_scene_resources();
    setup_spot_light();
    create_shadow_map_resources();
    create_main_depth_resources();
    const auto& static_opaque_info = render_database->get_static_opaque_triangles_info();
    if (static_opaque_info.materials.empty()) {
        throw std::runtime_error("Failed to setup draw sponza test: static opaque triangles produced no material batches.");
    }
    compile_shadow_pipeline();
    for (const auto& material_unit : static_opaque_info.materials) {
        main_graphics_pipelines.push_back(compile_main_pipeline(material_unit));
    }
    for (auto& graphics_pipeline : main_graphics_pipelines) {
        write_main_dynamic_descriptors(graphics_pipeline);
    }
    register_draw_sponza_render_graph();
}

void ApplicationVulkanDrawSponzaTest::setup_render_context() {
    renderer_vk = WBE::RendererVK::get_singleton();
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = get_renderer_allocator().get();
    render_database = WBE::RenderDatabase::get_singleton();
}

void ApplicationVulkanDrawSponzaTest::load_resources() {
    auto* resource_manager = WBE::ResourceManager::get_singleton();
    sponza_mesh = resource_manager->get_resource<WBE::RUMesh3D>("sponza.mesh");
    if (sponza_mesh == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: sponza mesh resource was not found.");
    }
    default_material = resource_manager->get_resource<WBE::RUMaterial>("default_material");
    if (default_material == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: default material resource was not found.");
    }
    shadow_pipeline_resource = resource_manager->get_resource<WBE::RUGraphicsPipeline>("shadowmap_spot");
    if (shadow_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: 'shadowmap_spot' graphics pipeline resource not found.");
    }
}

void ApplicationVulkanDrawSponzaTest::setup_static_scene_resources() {
    render_database->set_static_opaque_triangles(sponza_mesh.get());
    renderer_vk->update_static_opaque_triangles();
}

void ApplicationVulkanDrawSponzaTest::setup_spot_light() {
    spot_light_component = WBE::make_default_test_spot_light_component(32.0F);
    shadow_extent = VkExtent2D{
        .width = spot_light_component.shadow_map_resolution,
        .height = spot_light_component.shadow_map_resolution,
    };
}

void ApplicationVulkanDrawSponzaTest::create_shadow_map_resources() {
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

void ApplicationVulkanDrawSponzaTest::create_main_depth_resources() {
    main_depth_image = WBE::ROImageVK::Builder()
                           .set_format(MAIN_DEPTH_FORMAT)
                           .image_2d(base_render_data->swap_chain_extent)
                           .set_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                           .create_ref(base_render_data);
    VkImageSubresourceRange depth_subresource{
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    main_depth_view = WBE::ROImageViewVK::Builder()
                          .set_image(main_depth_image)
                          .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                          .set_format(MAIN_DEPTH_FORMAT)
                          .set_subresource_range(depth_subresource)
                          .create_ref(base_render_data);
}

void ApplicationVulkanDrawSponzaTest::compile_shadow_pipeline() {
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
    shadow_descriptor_sets = std::move(compiled.descriptor_sets);
}

ApplicationVulkanDrawSponzaTest::GraphicsPipeline ApplicationVulkanDrawSponzaTest::compile_main_pipeline(
    const WBE::RenderDatabase::OpaqueTrianglesInfo::MaterialUnit& p_material_unit) {
    if (p_material_unit.material == nullptr) {
        throw std::runtime_error("Failed to setup draw sponza test: material batch resource was not propagated to the render database.");
    }
    camera_component = WBE::make_default_test_camera_component(base_render_data->swap_chain_aspect_ratio, 100.0F);
    WBE::RenderBindpointDeclarationsBuilder bindpoint_builder;
    WBE::SpotLightInfoVKUBO spot_light_info = WBE::make_spot_light_info_ubo(spot_light_component);
    uint32_t spot_light_count = 1;
    auto material_params =
        WBE::add_material_texture_bindings(bindpoint_builder, default_material, p_material_unit.material, material_sampler);
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
        .depth_attachment_format = MAIN_DEPTH_FORMAT,
    };
    auto compiled = WBE::GraphicsPipelineCompilerVK(main_compiler_context)
                        .compile(p_material_unit.material->graphics_pipeline->resource_key,
                            p_material_unit.material->graphics_pipeline,
                            main_bindpoint_declarations);
    return {.draw_sub_region = p_material_unit.sub_region,
        .graphics_pipeline = compiled.graphics_pipeline,
        .descriptor_sets = std::move(compiled.descriptor_sets),
        .descriptor_set_bindings = std::move(compiled.descriptor_set_bindings)};
}

void ApplicationVulkanDrawSponzaTest::write_main_dynamic_descriptors(GraphicsPipeline& p_pipeline) {
    if (p_pipeline.descriptor_sets.empty()) {
        throw std::runtime_error("Failed to setup draw sponza test: main pipeline produced no descriptor sets.");
    }
    if (camera_info_ubo.get() == nullptr) {
        camera_info_ubo = WBE::ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                              .set_size(sizeof(WBE::CameraInfoVKUBO))
                              .create_ref(base_render_data);
        WBE::CameraInfoVKUBO camera_info = WBE::make_camera_info_ubo(camera_component);
        camera_info_ubo->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
    }
    auto [set, binding] = p_pipeline.descriptor_set_bindings.at("camera_info");
    p_pipeline.descriptor_sets[set]->write_descriptor_set(WBE::RODescriptorSetVK::Writer().set_binding(binding).add_uniform_buffer_info(
        camera_info_ubo, 0, sizeof(WBE::CameraInfoVKUBO)));
}

void ApplicationVulkanDrawSponzaTest::register_draw_sponza_render_graph() {
    WBE::RendGraphDescVK pipeline_desc(render_allocator);
    pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK),
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .res = WBE::MEM_NULL});
    pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW),
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .res = main_depth_view});
    for (uint32_t i = 0; i < main_graphics_pipelines.size(); ++i) {
        add_render_graph_resources(pipeline_desc, i);
    }
    pipeline_desc.tasks.emplace_back(create_shadow_task_desc());
    for (uint32_t i = 0; i < main_graphics_pipelines.size(); ++i) {
        pipeline_desc.tasks.emplace_back(create_main_task_desc(i));
    }
    renderer_vk->register_render_graph("draw_sponza_pipeline", std::move(pipeline_desc));
    renderer_vk->activate_render_graph("draw_sponza_pipeline");
}

void ApplicationVulkanDrawSponzaTest::add_render_graph_resources(
    WBE::RendGraphDescVK& p_pipeline_desc, uint32_t p_graphics_pipeline_index) const {
    p_pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE),
        .res_type_id = WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID,
        .res = shadow_graphics_pipeline});
    p_pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW),
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .res = shadow_color_view});
    p_pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW),
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .res = shadow_depth_view});
    p_pipeline_desc.resources.emplace_back(
        WBE::RendResVK{.res_id = std::format("{}.{}", WBE::REND_RES_TEST_DRAW_SPONZA_GRAPHICS_PIPELINE, p_graphics_pipeline_index),
            .res_type_id = WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID,
            .res = main_graphics_pipelines[p_graphics_pipeline_index].graphics_pipeline});
}

WBE::RendTaskDescVK ApplicationVulkanDrawSponzaTest::create_shadow_task_desc() const {
    WBE::RendTaskDescVK shadow_task_desc(render_allocator);
    shadow_task_desc.task_id = WBE::RT_RUN_PIPELINE_SHADOWMAP;
    shadow_task_desc.create = [task_render_allocator = render_allocator,
                                  task_base_render_data = base_render_data,
                                  task_shadow_extent = shadow_extent,
                                  task_shadow_descriptor_sets = shadow_descriptor_sets]() -> WBE::Unique<WBE::RenderTaskVK> {
        std::array<VkClearValue, 2> shadow_clear_values{};
        shadow_clear_values[0].color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}};
        shadow_clear_values[1].depthStencil = {.depth = 1.0F, .stencil = 0};
        std::array<std::string_view, 1> shadow_color_attachment_ids{WBE::REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW};
        return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            std::span<const VkClearValue>(shadow_clear_values.data(), shadow_clear_values.size()),
            task_shadow_extent,
            std::span<std::string_view>(shadow_color_attachment_ids.data(), shadow_color_attachment_ids.size()),
            WBE::REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW,
            WBE::REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE,
            WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
            WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
            std::span<const WBE::Ref<WBE::RODescriptorSetVK>>(task_shadow_descriptor_sets.data(), task_shadow_descriptor_sets.size()));
    };
    return shadow_task_desc;
}

WBE::RendTaskDescVK ApplicationVulkanDrawSponzaTest::create_main_task_desc(uint32_t p_pipeline_index) const {
    WBE::RendTaskDescVK main_task_desc(render_allocator);
    auto make_task_id = [](uint32_t p_index) {
        return WBE::dynam_hash(std::format("RT_RUN_SPONZA_PIPELINE_{}", p_index));
    };
    main_task_desc.task_id = make_task_id(p_pipeline_index);
    main_task_desc.prev.emplace_back(
        p_pipeline_index == 0 ? WBE::RT_RUN_PIPELINE_SHADOWMAP : make_task_id(p_pipeline_index - 1));
    main_task_desc.create = [task_render_allocator = render_allocator,
                                task_base_render_data = base_render_data,
                                task_main_descriptor_sets = main_graphics_pipelines[p_pipeline_index].descriptor_sets,
                                task_draw_sub_region = main_graphics_pipelines[p_pipeline_index].draw_sub_region,
                                p_pipeline_index]() -> WBE::Unique<WBE::RenderTaskVK> {
        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}};
        clear_values[1].depthStencil = {.depth = 1.0F, .stencil = 0};
        std::array<std::string_view, 1> color_attachment_ids{WBE::REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK};
        VkAttachmentLoadOp attachment_load_op =
            p_pipeline_index == 0 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            std::span<const VkClearValue>(clear_values.data(), clear_values.size()),
            task_base_render_data->swap_chain_extent,
            std::span<const std::string_view>(color_attachment_ids.data(), color_attachment_ids.size()),
            WBE::REND_RES_TEST_DRAW_SPONZA_DEPTH_IMAGE_VIEW,
            std::format("{}.{}", WBE::REND_RES_TEST_DRAW_SPONZA_GRAPHICS_PIPELINE, p_pipeline_index),
            WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
            WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
            std::span<const WBE::Ref<WBE::RODescriptorSetVK>>(task_main_descriptor_sets.data(), task_main_descriptor_sets.size()),
            WBE::IndexedDrawRegionVK{.first_index = static_cast<uint32_t>(task_draw_sub_region.starting_index),
                .index_count = static_cast<uint32_t>(task_draw_sub_region.size),
                .vertex_offset = 0},
            attachment_load_op,
            attachment_load_op);
    };
    return main_task_desc;
}
