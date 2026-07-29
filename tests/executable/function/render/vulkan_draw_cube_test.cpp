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
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/render_task.hh"
#include "function/render/spot_light_info.hh"
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
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

namespace {

constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOWMAP_COLOR_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_COLOR_FORMAT;

WBE::CameraInfoVKUBO default_camera_info(float p_aspect_ratio) {
    WBE::CameraInfoVKUBO camera_info{
        .view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F)),
        .proj = glm::perspective(glm::radians(45.0F), p_aspect_ratio, 0.1F, 10.0F),
    };
    camera_info.proj[1][1] *= -1;
    return camera_info;
}

WBE::SpotLightInfoVKUBO make_spot_light_info_ubo(const WBE::SpotLightInfo& p_spot_light) {
    return WBE::SpotLightInfoVKUBO{.view = p_spot_light.view,
        .proj = p_spot_light.proj,
        .position = p_spot_light.position,
        .intensity = p_spot_light.intensity,
        .direction = p_spot_light.direction,
        .inner_cone_cos = p_spot_light.inner_cone_cos,
        .color = p_spot_light.color,
        .outer_cone_cos = p_spot_light.outer_cone_cos,
        .range = p_spot_light.range};
}

std::vector<uint8_t> build_spot_light_infos_uniform_data(const WBE::SpotLightInfo& p_spot_light) {
    WBE::SpotLightInfosVKUBO spot_light_infos{};
    spot_light_infos.spot_light_info[0] = make_spot_light_info_ubo(p_spot_light);
    spot_light_infos.spot_light_count = 1;

    std::vector<uint8_t> result(sizeof(spot_light_infos));
    std::memcpy(result.data(), &spot_light_infos, result.size());
    return result;
}

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
    WBE::RendTaskDescVK create_shadow_task_desc() const;
    WBE::RendTaskDescVK create_main_task_desc() const;

    WBE::RendererVK* renderer_vk = nullptr;
    const WBE::BaseRenderDataVK* base_render_data = nullptr;
    WBE::RenderAllocator* render_allocator = nullptr;
    WBE::RenderDatabase* render_database = nullptr;
    WBE::RefRes<WBE::RUMesh3D> cube_mesh;
    WBE::RefRes<WBE::RUMaterial> default_material;
    WBE::RefRes<WBE::RUGraphicsPipeline> shadow_pipeline_resource;

    // Kept alive for the lifetime of the test because the graph resources and
    // the main pipeline's descriptor set reference these by raw handles.
    WBE::Ref<WBE::ROGraphicsPipelineVK> shadow_graphics_pipeline = WBE::MEM_NULL;
    WBE::Ref<WBE::ROGraphicsPipelineVK> main_graphics_pipeline = WBE::MEM_NULL;
    std::vector<WBE::Ref<WBE::RODescriptorSetVK>> shadow_descriptor_sets;
    std::vector<WBE::Ref<WBE::RODescriptorSetVK>> main_descriptor_sets;
    WBE::Ref<WBE::ROImageVK> shadow_depth_image;
    WBE::Ref<WBE::ROImageVK> shadow_color_image;
    WBE::Ref<WBE::ROImageViewVK> shadow_depth_view;
    WBE::Ref<WBE::ROImageViewVK> shadow_color_view;
    WBE::Ref<WBE::ROSamplerVK> shadow_sampler;
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    WBE::SpotLightInfo spot_light{};
    VkExtent2D shadow_extent{};
    WBE::CameraInfoVKUBO camera_info{};
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

void ApplicationVulkanDrawCubeTest::logic_update(double /* p_delta */) {
}

void ApplicationVulkanDrawCubeTest::render_update(double p_delta) {
    if (camera_info_ubo == WBE::MEM_NULL) {
        return;
    }
    camera_info.view = glm::rotate(camera_info.view, static_cast<float>(p_delta), glm::vec3(0.0F, 1.0F, 0.0F));
    camera_info_ubo->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
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
    std::array<WBE::ComponentSpotLight, 1> source_spot_lights{
        WBE::ComponentSpotLight{
            .position = glm::vec3(2.5F, 3.5F, 2.5F),
            .direction = glm::normalize(glm::vec3(-1.0F, -1.4F, -1.0F)),
            .color = glm::vec3(1.0F, 0.95F, 0.85F),
            .intensity = 8.0F,
            .inner_cone_cos = std::cos(glm::radians(18.0F)),
            .outer_cone_cos = std::cos(glm::radians(28.0F)),
            .range = 12.0F,
        },
    };
    render_database->set_static_spot_lights(
        std::span<const WBE::ComponentSpotLight>(source_spot_lights.data(), source_spot_lights.size()));
    const auto& static_spot_lights = render_database->get_static_spot_lights();
    if (static_spot_lights.empty()) {
        throw std::runtime_error("Failed to setup draw cube test: no spot lights registered.");
    }
    spot_light = static_spot_lights[0];
    shadow_extent = VkExtent2D{.width = spot_light.shadow_map_resolution, .height = spot_light.shadow_map_resolution};
}

void ApplicationVulkanDrawCubeTest::create_shadow_map_resources() {
    const std::set<uint32_t> queue_families(
        base_render_data->queue_family_indices.begin(), base_render_data->queue_family_indices.end());

    shadow_depth_image =
        WBE::ROImageVK::Builder().shadow_map_depth(spot_light).set_queue_families(queue_families).create_ref(base_render_data);
    shadow_color_image =
        WBE::ROImageVK::Builder().shadow_map_color(spot_light).set_queue_families(queue_families).create_ref(base_render_data);

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
    std::array<WBE::RenderBindpointDeclaration, 2> shadow_bindpoint_declarations{
        WBE::RenderBindpointDeclaration{.field_path = "light_camera_info.view", .data = spot_light.view},
        WBE::RenderBindpointDeclaration{.field_path = "light_camera_info.proj", .data = spot_light.proj},
    };
    auto [compiled_graphics_pipeline, compiled_descriptor_sets] =
        WBE::GraphicsPipelineCompilerVK(shadow_compiler_context)
            .compile("shadowmap_spot",
                shadow_pipeline_resource,
                std::span<const WBE::RenderBindpointDeclaration>(
                    shadow_bindpoint_declarations.data(), shadow_bindpoint_declarations.size()));
    shadow_graphics_pipeline = compiled_graphics_pipeline;
    shadow_descriptor_sets = std::move(compiled_descriptor_sets);
}

void ApplicationVulkanDrawCubeTest::compile_main_pipeline() {
    camera_info = default_camera_info(base_render_data->swap_chain_aspect_ratio);
    auto main_bindpoint_declarations = WBE::RenderBindpointDeclarationsBuilder()
        .add_decleratoin("camera_info.view", camera_info.view)
        .add_decleratoin("camera_info.proj", camera_info.proj)
        .add_decleratoin("spot_light_infos", build_spot_light_infos_uniform_data(spot_light))
        .add_resource_bindpoint(default_material)
        .build();

    WBE::GraphicsPipelineCompilerVKContext main_compiler_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
    };
    auto [compiled_graphics_pipeline, compiled_descriptor_sets] =
        WBE::GraphicsPipelineCompilerVK(main_compiler_context)
            .compile(default_material->graphics_pipeline->resource_key,
                default_material->graphics_pipeline,
                std::span<const WBE::RenderBindpointDeclaration>(
                    main_bindpoint_declarations.data(), main_bindpoint_declarations.size()));
    main_graphics_pipeline = compiled_graphics_pipeline;
    main_descriptor_sets = std::move(compiled_descriptor_sets);
}

void ApplicationVulkanDrawCubeTest::write_main_dynamic_descriptors() {
    if (main_descriptor_sets.empty()) {
        throw std::runtime_error("Failed to setup draw cube test: main pipeline produced no descriptor sets.");
    }
    camera_info_ubo = WBE::ROBufferVK::Builder()
                          .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                          .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                          .set_size(sizeof(WBE::CameraInfoVKUBO))
                          .create_ref(base_render_data);
    camera_info_ubo->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
    main_descriptor_sets[0]->write_descriptor_set(
        WBE::RODescriptorSetVK::Writer().set_binding(0).add_uniform_buffer_info(WBE::RODescriptorSetVK::Writer::BufferInfo{
            .buffer = camera_info_ubo,
            .offset = 0,
            .range = sizeof(WBE::CameraInfoVKUBO),
        }));
    constexpr uint32_t SHADOWMAP_BINDING = 6;
    constexpr uint32_t SHADOWMAP_SAMPLER_BINDING = 7;
    main_descriptor_sets[0]->write_descriptor_set(WBE::RODescriptorSetVK::Writer()
            .set_binding(SHADOWMAP_BINDING)
            .add_sampled_image_info(WBE::RODescriptorSetVK::Writer::ImageInfo{
                .image_view = shadow_depth_view,
                .image_layout = VK_IMAGE_LAYOUT_GENERAL,
            }));
    main_descriptor_sets[0]->write_descriptor_set(WBE::RODescriptorSetVK::Writer()
            .set_binding(SHADOWMAP_SAMPLER_BINDING)
            .add_sampler_info(WBE::RODescriptorSetVK::Writer::ImageInfo{
                .sampler = shadow_sampler,
            }));
}

void ApplicationVulkanDrawCubeTest::register_draw_cube_render_graph() {
    WBE::RendGraphDescVK pipeline_desc(render_allocator);
    add_render_graph_resources(pipeline_desc);
    pipeline_desc.tasks.emplace_back(create_shadow_task_desc());
    pipeline_desc.tasks.emplace_back(create_main_task_desc());
    renderer_vk->register_render_graph("draw_cube_pipeline", std::move(pipeline_desc));
    renderer_vk->activate_render_graph("draw_cube_pipeline");
}

void ApplicationVulkanDrawCubeTest::add_render_graph_resources(WBE::RendGraphDescVK& p_pipeline_desc) const {
    p_pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = WBE::REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE,
        .res_type_id = WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID,
        .res = shadow_graphics_pipeline});
    p_pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = WBE::REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW,
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .res = shadow_color_view});
    p_pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = WBE::REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW,
        .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
        .res = shadow_depth_view});
    p_pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = WBE::REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE,
        .res_type_id = WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID,
        .res = main_graphics_pipeline});
}

WBE::RendTaskDescVK ApplicationVulkanDrawCubeTest::create_shadow_task_desc() const {
    WBE::RendTaskDescVK shadow_task_desc(render_allocator);
    shadow_task_desc.task_id = WBE::RT_RUN_PIPELINE_SHADOWMAP;
    shadow_task_desc.create = [task_render_allocator = render_allocator,
                                  task_base_render_data = base_render_data,
                                  task_shadow_extent = shadow_extent,
                                  task_shadow_descriptor_sets = shadow_descriptor_sets]() -> WBE::Unique<WBE::RenderTaskVK> {
        std::array<VkClearValue, 2> shadow_clear_values{};
        shadow_clear_values[0].color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}};
        shadow_clear_values[1].depthStencil = {.depth = 1.0F, .stencil = 0};
        std::array<WBE::RendResID, 1> shadow_color_attachment_ids{WBE::REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW};
        return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            std::span<const VkClearValue>(shadow_clear_values.data(), shadow_clear_values.size()),
            task_shadow_extent,
            std::span<const WBE::RendResID>(shadow_color_attachment_ids.data(), shadow_color_attachment_ids.size()),
            WBE::REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW,
            WBE::REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE,
            WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
            WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
            std::span<const WBE::Ref<WBE::RODescriptorSetVK>>(task_shadow_descriptor_sets.data(), task_shadow_descriptor_sets.size()));
    };
    return shadow_task_desc;
}

WBE::RendTaskDescVK ApplicationVulkanDrawCubeTest::create_main_task_desc() const {
    WBE::RendTaskDescVK main_task_desc(render_allocator);
    main_task_desc.task_id = WBE::RT_RUN_PIPELINE;
    main_task_desc.prev.emplace_back(WBE::RT_RUN_PIPELINE_SHADOWMAP);
    main_task_desc.create = [task_render_allocator = render_allocator,
                                task_base_render_data = base_render_data,
                                task_main_descriptor_sets = main_descriptor_sets]() -> WBE::Unique<WBE::RenderTaskVK> {
        return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
            task_base_render_data->swap_chain_extent,
            WBE::REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK,
            WBE::REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE,
            WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
            WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
            std::span<const WBE::Ref<WBE::RODescriptorSetVK>>(task_main_descriptor_sets.data(), task_main_descriptor_sets.size()));
    };
    return main_task_desc;
}
