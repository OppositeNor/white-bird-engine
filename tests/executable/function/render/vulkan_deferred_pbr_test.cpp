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
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <format>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

constexpr std::string_view REND_RES_TEST_DEFERRED_COMPOSITION_GRAPHICS_PIPELINE = "REND_RES_TEST_DEFERRED_COMPOSITION_GRAPHICS_PIPELINE";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW";
WBE_LABEL(RT_RUN_DEFERRED_GBUFFER, WBE_RENDER_TASK);
WBE_LABEL(RT_RUN_DEFERRED_COMPOSITION, WBE_RENDER_TASK);

constexpr VkFormat GBUFFER_ALBEDO_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_NORMAL_FORMAT = VK_FORMAT_R16G16_SNORM;
constexpr VkFormat GBUFFER_RMA_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOWMAP_COLOR_FORMAT = ROImageVK::Builder::SHADOW_MAP_COLOR_FORMAT;

class ApplicationVulkanDeferredPBRTest : public ApplicationGame {
public:
    virtual void logic_update(double p_delta) override;
    virtual void render_update(double p_delta) override;

    void setup_draw_task();

private:
    struct CompositionCameraInfo {
        glm::mat4 inverse_view_projection;
        glm::vec4 camera_world_position;
    };

    void setup_render_context();
    void load_resources();
    void setup_static_scene_resources();
    void setup_spot_light();
    void create_shadow_map_resources();
    void create_gbuffer_resources();
    void compile_shadow_pipeline();
    void compile_composition_pipeline();
    void write_dynamic_camera_descriptors(CompiledStaticOpaqueTrianglesVK& p_gbuffer);
    void write_dynamic_light_descriptors();
    void register_deferred_render_graph(RendTaskDescVK p_gbuffer_task);
    void add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const;
    RendTaskDescVK create_shadow_task_desc() const;
    RendTaskDescVK create_composition_task_desc() const;
    CompositionCameraInfo make_composition_camera_info() const;

    RendererVK* renderer_vk = nullptr;
    const BaseRenderDataVK* base_render_data = nullptr;
    RenderAllocator* render_allocator = nullptr;
    RenderDatabase* render_database = nullptr;
    RefRes<RUMesh3D> sponza_mesh;
    RefRes<RUGraphicsPipeline> shadow_pipeline_resource;
    RefRes<RUGraphicsPipeline> composition_pipeline_resource;

    // Kept alive for the lifetime of the test because the graph resources and
    // descriptor sets reference these objects by raw handles.
    Ref<ROGraphicsPipelineVK> shadow_graphics_pipeline = MEM_NULL;
    Ref<ROGraphicsPipelineVK> composition_graphics_pipeline = MEM_NULL;
    std::vector<Ref<RODescriptorSetVK>> shadow_descriptor_sets;
    std::vector<Ref<RODescriptorSetVK>> composition_descriptor_sets;
    GraphicsPipelineCompilerVK::DescriptorSetBindings shadow_descriptor_set_bindings;
    GraphicsPipelineCompilerVK::DescriptorSetBindings composition_descriptor_set_bindings;
    Ref<ROImageVK> gbuffer_albedo_image;
    Ref<ROImageVK> gbuffer_normal_image;
    Ref<ROImageVK> gbuffer_rma_image;
    Ref<ROImageVK> gbuffer_depth_image;
    Ref<ROImageViewVK> gbuffer_albedo_view;
    Ref<ROImageViewVK> gbuffer_normal_view;
    Ref<ROImageViewVK> gbuffer_rma_view;
    Ref<ROImageViewVK> gbuffer_depth_view;
    Ref<ROImageVK> shadow_depth_image;
    Ref<ROImageVK> shadow_color_image;
    Ref<ROImageViewVK> shadow_depth_view;
    Ref<ROImageViewVK> shadow_color_view;
    Ref<ROSamplerVK> shadow_sampler;
    Ref<ROSamplerVK> gbuffer_sampler;
    Ref<ROBufferVK> gbuffer_camera_info_ubo;
    Ref<ROBufferVK> composition_camera_info_ubo;
    Ref<ROBufferVK> spot_light_info_ubo;
    ComponentSpotLight spot_light_component{};
    float spot_light_yaw = 0.0F;
    float spot_light_pitch = 0.0F;
    VkExtent2D shadow_extent{};
    ComponentCamera camera_component{};
    glm::vec3 camera_position = TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = default_test_camera_yaw();
    float camera_pitch = default_test_camera_pitch();
};

void ApplicationVulkanDeferredPBRTest::logic_update(double p_delta) { // NOLINT
    InputManager* input_manager = InputManager::get_singleton();
    if (input_manager == nullptr) {
        return;
    }

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

    camera_component.view = test_camera_view(camera_position, camera_yaw, camera_pitch);
    ROBufferVK* gbuffer_camera_buffer = gbuffer_camera_info_ubo.get();
    ROBufferVK* composition_camera_buffer = composition_camera_info_ubo.get();
    ROBufferVK* spot_light_buffer = spot_light_info_ubo.get();
    if (gbuffer_camera_buffer == nullptr || composition_camera_buffer == nullptr || spot_light_buffer == nullptr) {
        return;
    }
    CameraInfoVKUBO gbuffer_camera_info = make_camera_info_ubo(camera_component);
    CompositionCameraInfo composition_camera_info = make_composition_camera_info();
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    add_render_job(
        [gbuffer_camera_info, composition_camera_info, spot_light_info, gbuffer_camera_buffer, composition_camera_buffer, spot_light_buffer]() {
            gbuffer_camera_buffer->update_buffer(&gbuffer_camera_info, sizeof(CameraInfoVKUBO));
            composition_camera_buffer->update_buffer(&composition_camera_info, sizeof(CompositionCameraInfo));
            spot_light_buffer->update_buffer(&spot_light_info, sizeof(SpotLightInfoVKUBO));
        });
}

void ApplicationVulkanDeferredPBRTest::render_update(double /* p_delta */) {
}

void ApplicationVulkanDeferredPBRTest::setup_draw_task() {
    setup_render_context();
    load_resources();
    setup_static_scene_resources();
    setup_spot_light();
    create_shadow_map_resources();
    create_gbuffer_resources();
    compile_shadow_pipeline();
    StaticOpaqueTrianglesCompileConfigVK compile_config;
    compile_config.task_id = RT_RUN_DEFERRED_GBUFFER;
    compile_config.graphics_pipeline_resource_key = "deferred_gbuffer";
    compile_config.color_attachment_ids = {std::string(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW),
        std::string(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW),
        std::string(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW)};
    compile_config.depth_attachment_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW);
    compile_config.color_attachment_formats = {GBUFFER_ALBEDO_FORMAT, GBUFFER_NORMAL_FORMAT, GBUFFER_RMA_FORMAT};
    compile_config.depth_attachment_format = GBUFFER_DEPTH_FORMAT;
    compile_config.clear_values.resize(4);
    compile_config.clear_values[3].depthStencil = {.depth = 1.0F, .stencil = 0};
    auto compiled_gbuffer = renderer_vk->compile_static_opaque_triangles(compile_config);
    compiled_gbuffer.task.prev.emplace_back(RT_RUN_PIPELINE_SHADOWMAP);
    compile_composition_pipeline();
    write_dynamic_camera_descriptors(compiled_gbuffer);
    write_dynamic_light_descriptors();
    register_deferred_render_graph(std::move(compiled_gbuffer.task));
}

void ApplicationVulkanDeferredPBRTest::setup_render_context() {
    renderer_vk = RendererVK::get_singleton();
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = get_renderer_allocator().get();
    render_database = RenderDatabase::get_singleton();
    camera_component = make_default_test_camera_component(base_render_data->swap_chain_aspect_ratio, 100.0F);
}

void ApplicationVulkanDeferredPBRTest::load_resources() {
    auto* resource_manager = ResourceManager::get_singleton();
    sponza_mesh = resource_manager->get_resource<RUMesh3D>("sponza.mesh");
    if (sponza_mesh == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: sponza mesh resource was not found.");
    }
    shadow_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("shadowmap_spot");
    if (shadow_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'shadowmap_spot' graphics pipeline resource not found.");
    }
    composition_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("deferred_composition");
    if (composition_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'deferred_composition' graphics pipeline resource not found.");
    }
}

void ApplicationVulkanDeferredPBRTest::setup_static_scene_resources() {
    render_database->set_static_opaque_triangles(sponza_mesh.get());
    renderer_vk->update_static_opaque_triangles();
}

void ApplicationVulkanDeferredPBRTest::setup_spot_light() {
    spot_light_component = make_default_test_spot_light_component(32.0F);
    spot_light_yaw = std::atan2(spot_light_component.direction.z, spot_light_component.direction.x);
    spot_light_pitch = std::asin(std::clamp(spot_light_component.direction.y, -1.0F, 1.0F));
    shadow_extent = VkExtent2D{
        .width = spot_light_component.shadow_map_resolution,
        .height = spot_light_component.shadow_map_resolution,
    };
}

void ApplicationVulkanDeferredPBRTest::create_shadow_map_resources() {
    shadow_depth_image = ROImageVK::Builder().shadow_map_depth(spot_light_component).create_ref(base_render_data);
    shadow_color_image = ROImageVK::Builder().shadow_map_color(spot_light_component).create_ref(base_render_data);

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
    shadow_depth_view = ROImageViewVK::Builder()
                            .set_image(shadow_depth_image)
                            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                            .set_format(SHADOWMAP_DEPTH_FORMAT)
                            .set_subresource_range(depth_subresource)
                            .create_ref(base_render_data);
    shadow_color_view = ROImageViewVK::Builder()
                            .set_image(shadow_color_image)
                            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                            .set_format(SHADOWMAP_COLOR_FORMAT)
                            .set_subresource_range(color_subresource)
                            .create_ref(base_render_data);
    shadow_sampler = ROSamplerVK::Builder()
                         .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                         .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                         .create_ref(base_render_data);
}

void ApplicationVulkanDeferredPBRTest::create_gbuffer_resources() {
    VkExtent2D extent = base_render_data->swap_chain_extent;
    gbuffer_albedo_image = ROImageVK::Builder()
                               .set_format(GBUFFER_ALBEDO_FORMAT)
                               .image_2d(extent)
                               .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                               .create_ref(base_render_data);
    gbuffer_normal_image = ROImageVK::Builder()
                               .set_format(GBUFFER_NORMAL_FORMAT)
                               .image_2d(extent)
                               .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                               .create_ref(base_render_data);
    gbuffer_rma_image = ROImageVK::Builder()
                            .set_format(GBUFFER_RMA_FORMAT)
                            .image_2d(extent)
                            .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                            .create_ref(base_render_data);
    gbuffer_depth_image = ROImageVK::Builder()
                              .set_format(GBUFFER_DEPTH_FORMAT)
                              .image_2d(extent)
                              .set_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                              .create_ref(base_render_data);

    VkImageSubresourceRange color_subresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    VkImageSubresourceRange depth_subresource{
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    gbuffer_albedo_view = ROImageViewVK::Builder()
                              .set_image(gbuffer_albedo_image)
                              .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                              .set_format(GBUFFER_ALBEDO_FORMAT)
                              .set_subresource_range(color_subresource)
                              .create_ref(base_render_data);
    gbuffer_normal_view = ROImageViewVK::Builder()
                              .set_image(gbuffer_normal_image)
                              .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                              .set_format(GBUFFER_NORMAL_FORMAT)
                              .set_subresource_range(color_subresource)
                              .create_ref(base_render_data);
    gbuffer_rma_view = ROImageViewVK::Builder()
                           .set_image(gbuffer_rma_image)
                           .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                           .set_format(GBUFFER_RMA_FORMAT)
                           .set_subresource_range(color_subresource)
                           .create_ref(base_render_data);
    gbuffer_depth_view = ROImageViewVK::Builder()
                             .set_image(gbuffer_depth_image)
                             .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                             .set_format(GBUFFER_DEPTH_FORMAT)
                             .set_subresource_range(depth_subresource)
                             .create_ref(base_render_data);
    gbuffer_sampler = ROSamplerVK::Builder()
                          .set_filter(VK_FILTER_NEAREST, VK_FILTER_NEAREST)
                          .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                          .create_ref(base_render_data);
}

void ApplicationVulkanDeferredPBRTest::compile_shadow_pipeline() {
    GraphicsPipelineCompilerVKContext compiler_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {SHADOWMAP_COLOR_FORMAT},
        .depth_attachment_format = SHADOWMAP_DEPTH_FORMAT,
    };
    RenderBindpointDeclarationsBuilder bindpoint_builder;
    bindpoint_builder
        .add_buffer_decleration("light_camera_info.view", &spot_light_component.view, sizeof(spot_light_component.view))
        .add_buffer_decleration("light_camera_info.proj", &spot_light_component.proj, sizeof(spot_light_component.proj));
    auto compiled =
        GraphicsPipelineCompilerVK(compiler_context).compile("shadowmap_spot", shadow_pipeline_resource, bindpoint_builder.build());
    shadow_graphics_pipeline = compiled.graphics_pipeline;
    shadow_descriptor_sets = std::move(compiled.descriptor_sets);
    shadow_descriptor_set_bindings = std::move(compiled.descriptor_set_bindings);
}

void ApplicationVulkanDeferredPBRTest::compile_composition_pipeline() {
    CompositionCameraInfo camera_info = make_composition_camera_info();
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    uint32_t spot_light_count = 1;
    RenderBindpointDeclarationsBuilder bindpoint_builder;
    bindpoint_builder.add_buffer_decleration("camera_info", &camera_info, sizeof(camera_info))
        .add_buffer_decleration("light.lights[0].info", &spot_light_info, sizeof(spot_light_info))
        .add_image_decleration("light.lights[0].shadowmap", shadow_depth_view)
        .add_sampler_decleration("light.lights[0].shadowmap_sampler", shadow_sampler)
        .add_buffer_decleration("light.light_count", &spot_light_count, sizeof(spot_light_count))
        .add_image_decleration("gbuffer_albedo", gbuffer_albedo_view)
        .add_image_decleration("gbuffer_octa_normal", gbuffer_normal_view)
        .add_image_decleration("gbuffer_rma", gbuffer_rma_view)
        .add_image_decleration("gbuffer_depth", gbuffer_depth_view)
        .add_sampler_decleration("gbuffer_sampler", gbuffer_sampler);
    GraphicsPipelineCompilerVKContext compiler_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
    };
    auto compiled = GraphicsPipelineCompilerVK(compiler_context)
                        .compile("deferred_composition", composition_pipeline_resource, bindpoint_builder.build());
    composition_graphics_pipeline = compiled.graphics_pipeline;
    composition_descriptor_sets = std::move(compiled.descriptor_sets);
    composition_descriptor_set_bindings = std::move(compiled.descriptor_set_bindings);
}

void ApplicationVulkanDeferredPBRTest::write_dynamic_camera_descriptors(CompiledStaticOpaqueTrianglesVK& p_gbuffer) {
    if (p_gbuffer.descriptor_sets.empty() || composition_descriptor_sets.empty()) {
        throw std::runtime_error("Failed to setup deferred test: compiled pipelines produced no descriptor sets.");
    }
    gbuffer_camera_info_ubo = ROBufferVK::Builder()
                                  .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                  .set_domain(DeviceDomain::HOST_DEVICE)
                                  .set_size(sizeof(CameraInfoVKUBO))
                                  .create_ref(base_render_data);
    CameraInfoVKUBO gbuffer_camera_info = make_camera_info_ubo(camera_component);
    gbuffer_camera_info_ubo->update_buffer(&gbuffer_camera_info, sizeof(gbuffer_camera_info));
    auto [gbuffer_set, gbuffer_binding] = p_gbuffer.descriptor_set_bindings.at("camera_info");
    p_gbuffer.descriptor_sets.at(gbuffer_set)
        ->write_descriptor_set(
            RODescriptorSetVK::Writer().set_binding(gbuffer_binding).add_uniform_buffer_info(gbuffer_camera_info_ubo, 0, sizeof(CameraInfoVKUBO)));

    composition_camera_info_ubo = ROBufferVK::Builder()
                                      .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                      .set_domain(DeviceDomain::HOST_DEVICE)
                                      .set_size(sizeof(CompositionCameraInfo))
                                      .create_ref(base_render_data);
    CompositionCameraInfo composition_camera_info = make_composition_camera_info();
    composition_camera_info_ubo->update_buffer(&composition_camera_info, sizeof(composition_camera_info));
    auto [composition_set, composition_binding] = composition_descriptor_set_bindings.at("camera_info");
    composition_descriptor_sets[composition_set]->write_descriptor_set(
        RODescriptorSetVK::Writer().set_binding(composition_binding).add_uniform_buffer_info(composition_camera_info_ubo, 0, sizeof(CompositionCameraInfo)));
}

void ApplicationVulkanDeferredPBRTest::write_dynamic_light_descriptors() {
    if (shadow_descriptor_sets.empty() || composition_descriptor_sets.empty()) {
        throw std::runtime_error("Failed to setup deferred test: compiled light pipelines produced no descriptor sets.");
    }
    spot_light_info_ubo = ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .set_domain(DeviceDomain::HOST_DEVICE)
                              .set_size(sizeof(SpotLightInfoVKUBO))
                              .create_ref(base_render_data);
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    spot_light_info_ubo->update_buffer(&spot_light_info, sizeof(spot_light_info));

    auto [shadow_set, shadow_binding] = shadow_descriptor_set_bindings.at("light_camera_info");
    shadow_descriptor_sets.at(shadow_set)
        ->write_descriptor_set(
            RODescriptorSetVK::Writer().set_binding(shadow_binding).add_uniform_buffer_info(spot_light_info_ubo, 0, sizeof(CameraInfoVKUBO)));

    auto [composition_set, composition_binding] = composition_descriptor_set_bindings.at("light.lights.info");
    composition_descriptor_sets.at(composition_set)
        ->write_descriptor_set(RODescriptorSetVK::Writer()
                .set_binding(composition_binding)
                .set_dst_array_element(0)
                .add_uniform_buffer_info(spot_light_info_ubo, 0, sizeof(SpotLightInfoVKUBO)));
}

void ApplicationVulkanDeferredPBRTest::register_deferred_render_graph(RendTaskDescVK p_gbuffer_task) {
    RendGraphDescVK pipeline_desc(render_allocator);
    add_render_graph_resources(pipeline_desc);
    pipeline_desc.tasks.emplace_back(create_shadow_task_desc());
    pipeline_desc.tasks.emplace_back(std::move(p_gbuffer_task));
    pipeline_desc.tasks.emplace_back(create_composition_task_desc());
    renderer_vk->register_render_graph("deferred_pipeline", std::move(pipeline_desc));
    renderer_vk->activate_render_graph("deferred_pipeline");
}

void ApplicationVulkanDeferredPBRTest::add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const {
    p_pipeline_desc.resources.emplace_back(RendResVK{
        .res_id = std::string(REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK), .res_type_id = TypeUUID<ROImageViewVK>::UUID, .res = MEM_NULL});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_DEFERRED_COMPOSITION_GRAPHICS_PIPELINE),
        .res_type_id = TypeUUID<ROGraphicsPipelineVK>::UUID,
        .res = composition_graphics_pipeline});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE),
        .res_type_id = TypeUUID<ROGraphicsPipelineVK>::UUID,
        .res = shadow_graphics_pipeline});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW),
        .res_type_id = TypeUUID<ROImageViewVK>::UUID,
        .res = shadow_color_view});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW),
        .res_type_id = TypeUUID<ROImageViewVK>::UUID,
        .res = shadow_depth_view});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW),
        .res_type_id = TypeUUID<ROImageViewVK>::UUID,
        .res = gbuffer_albedo_view});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW),
        .res_type_id = TypeUUID<ROImageViewVK>::UUID,
        .res = gbuffer_normal_view});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW),
        .res_type_id = TypeUUID<ROImageViewVK>::UUID,
        .res = gbuffer_rma_view});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW),
        .res_type_id = TypeUUID<ROImageViewVK>::UUID,
        .res = gbuffer_depth_view});
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_shadow_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_PIPELINE_SHADOWMAP;
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_shadow_extent = shadow_extent,
                           task_descriptor_sets = shadow_descriptor_sets]() -> Unique<RenderTaskVK> {
        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}};
        clear_values[1].depthStencil = {.depth = 1.0F, .stencil = 0};
        std::array<std::string_view, 1> color_attachment_ids{REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW};
        return make_unique<RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            std::span<const VkClearValue>(clear_values.data(), clear_values.size()),
            task_shadow_extent,
            std::span<const std::string_view>(color_attachment_ids.data(), color_attachment_ids.size()),
            REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW,
            REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE,
            REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
            REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
            std::span<const Ref<RODescriptorSetVK>>(task_descriptor_sets.data(), task_descriptor_sets.size()));
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_composition_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_DEFERRED_COMPOSITION;
    task_desc.prev.emplace_back(RT_RUN_DEFERRED_GBUFFER);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_descriptor_sets = composition_descriptor_sets]() -> Unique<RenderTaskVK> {
        return make_unique<RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
            task_base_render_data->swap_chain_extent,
            REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK,
            REND_RES_TEST_DEFERRED_COMPOSITION_GRAPHICS_PIPELINE,
            std::nullopt,
            std::nullopt,
            std::span<const Ref<RODescriptorSetVK>>(task_descriptor_sets.data(), task_descriptor_sets.size()));
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

namespace WBE = WhiteBirdEngine;

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Directory({"test_env"}));
    {
        WBE::ApplicationVulkanDeferredPBRTest game_application;
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
