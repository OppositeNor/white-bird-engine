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
#include "core/reflection/reflection_defs.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/function_utils.hh"
#include "function/input_manager/input_manager.hh"
#include "function/input_manager/key_codes.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_run_static_opaque_triangles_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "glm/ext/matrix_transform.hpp"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

constexpr std::string_view REND_RES_TEST_DIRECT_LIGHTING_COMPUTE_PIPELINE = "REND_RES_TEST_DIRECT_LIGHTING_COMPUTE_PIPELINE";
constexpr std::string_view REND_RES_TEST_INDIRECT_LIGHTING_COMPUTE_PIPELINE = "REND_RES_TEST_INDIRECT_LIGHTING_COMPUTE_PIPELINE";
constexpr std::string_view REND_RES_TEST_POST_PROCESSING_COMPUTE_PIPELINE = "REND_RES_TEST_POST_PROCESSING_COMPUTE_PIPELINE";
constexpr std::string_view REND_RES_TEST_PRESENT_POST_PROCESSING_GRAPHICS_PIPELINE = "REND_RES_TEST_PRESENT_POST_PROCESSING_GRAPHICS_PIPELINE";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW";
constexpr std::string_view REND_RES_TEST_DIRECT_LIGHTING_IMAGE_VIEW = "REND_RES_TEST_DIRECT_LIGHTING_IMAGE_VIEW";
constexpr std::string_view REND_RES_TEST_INDIRECT_LIGHTING_IMAGE_VIEW = "REND_RES_TEST_INDIRECT_LIGHTING_IMAGE_VIEW";
constexpr std::string_view REND_RES_TEST_POST_PROCESSED_IMAGE_VIEW = "REND_RES_TEST_POST_PROCESSED_IMAGE_VIEW";
WBE_LABEL(RT_RUN_DEFERRED_GBUFFER, WBE_RENDER_TASK);
WBE_LABEL(RT_RUN_DIRECT_LIGHTING, WBE_RENDER_TASK);
WBE_LABEL(RT_RUN_INDIRECT_LIGHTING, WBE_RENDER_TASK);
WBE_LABEL(RT_RUN_POST_PROCESSING, WBE_RENDER_TASK);
WBE_LABEL(RT_RUN_PRESENT_POST_PROCESSING, WBE_RENDER_TASK);

constexpr VkFormat GBUFFER_ALBEDO_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_NORMAL_FORMAT = VK_FORMAT_R16G16_SNORM;
constexpr VkFormat GBUFFER_RMA_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat RSM_FLUX_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat RSM_NORMAL_FORMAT = VK_FORMAT_R16G16_SNORM;
constexpr VkFormat LIGHTING_OUTPUT_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;

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

    void control(double p_delta);
    void update_descriptors();

    void setup_render_context();
    void load_resources();
    void setup_static_scene_resources();
    void setup_spot_light();
    void create_shadow_map_resources();
    void create_gbuffer_resources();
    void create_lighting_output_resources();
    Unique<RTRunStaticOpaqueTrianglesVK> compile_shadow_task();
    void compile_lighting_pipelines();
    void write_dynamic_camera_descriptors(RTRunStaticOpaqueTrianglesVK& p_gbuffer_task);
    void write_dynamic_light_descriptors();
    void register_deferred_render_graph(
        Unique<RTRunStaticOpaqueTrianglesVK>& p_shadow_task, Unique<RTRunStaticOpaqueTrianglesVK>& p_gbuffer_task);
    void add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const;
    RendTaskDescVK create_direct_lighting_task_desc() const;
    RendTaskDescVK create_indirect_lighting_task_desc() const;
    RendTaskDescVK create_post_processing_task_desc() const;
    RendTaskDescVK create_present_post_processing_task_desc() const;
    CompositionCameraInfo make_composition_camera_info() const;

    RendererVK* renderer_vk = nullptr;
    const BaseRenderDataVK* base_render_data = nullptr;
    RenderAllocator* render_allocator = nullptr;
    RenderDatabase* render_database = nullptr;
    RefRes<RUMesh3D> sponza_mesh;
    RefRes<RUShader> direct_lighting_shader_resource;
    RefRes<RUGraphicsPipeline> present_post_processing_pipeline_resource;
    RefRes<RUShader> indirect_lighting_shader_resource;
    RefRes<RUShader> post_processing_shader_resource;

    Ref<ROComputePipelineVK> direct_lighting_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> indirect_lighting_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> post_processing_compute_pipeline = MEM_NULL;
    Ref<ROGraphicsPipelineVK> present_post_processing_graphics_pipeline = MEM_NULL;
    std::vector<Ref<RODescriptorSetVK>> direct_lighting_descriptor_sets;
    std::vector<Ref<RODescriptorSetVK>> indirect_lighting_descriptor_sets;
    std::vector<Ref<RODescriptorSetVK>> post_processing_descriptor_sets;
    std::vector<Ref<RODescriptorSetVK>> present_post_processing_descriptor_sets;
    ComputePipelineCompilerVK::DescriptorSetBindings direct_lighting_descriptor_set_bindings;
    ComputePipelineCompilerVK::DescriptorSetBindings indirect_lighting_descriptor_set_bindings;
    ComputePipelineCompilerVK::DescriptorSetBindings post_processing_descriptor_set_bindings;
    GraphicsPipelineCompilerVK::DescriptorSetBindings present_post_processing_descriptor_set_bindings;
    Ref<ROImageVK> gbuffer_albedo_image;
    Ref<ROImageVK> gbuffer_normal_image;
    Ref<ROImageVK> gbuffer_rma_image;
    Ref<ROImageVK> gbuffer_depth_image;
    Ref<ROImageVK> direct_lighting_image;
    Ref<ROImageVK> indirect_lighting_image;
    Ref<ROImageVK> post_processed_image;
    Ref<ROImageViewVK> gbuffer_albedo_view;
    Ref<ROImageViewVK> gbuffer_normal_view;
    Ref<ROImageViewVK> gbuffer_rma_view;
    Ref<ROImageViewVK> gbuffer_depth_view;
    Ref<ROImageViewVK> direct_lighting_view;
    Ref<ROImageViewVK> indirect_lighting_view;
    Ref<ROImageViewVK> post_processed_view;
    Ref<ROImageVK> shadow_depth_image;
    Ref<ROImageVK> shadow_color_image;
    Ref<ROImageVK> shadow_normal_image;
    Ref<ROImageViewVK> shadow_depth_view;
    Ref<ROImageViewVK> shadow_color_view;
    Ref<ROImageViewVK> shadow_normal_view;
    Ref<ROSamplerVK> shadow_sampler;
    Ref<ROSamplerVK> gbuffer_sampler;
    Ref<ROSamplerVK> lighting_sampler;
    Ref<ROBufferVK> gbuffer_camera_info_ubo;
    Ref<ROBufferVK> composition_camera_info_ubo;
    Ref<ROBufferVK> spot_light_info_ubo;
    std::vector<std::pair<Ref<RODescriptorSetVK>, uint32_t>> gbuffer_camera_descriptor_bindings;
    std::vector<std::pair<Ref<RODescriptorSetVK>, uint32_t>> shadow_camera_descriptor_bindings;
    ComponentSpotLight spot_light_component{};
    float spot_light_yaw = 0.0F;
    float spot_light_pitch = 0.0F;
    VkExtent2D shadow_extent{};
    ComponentCamera camera_component{};
    glm::vec3 camera_position = TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = default_test_camera_yaw();
    float camera_pitch = default_test_camera_pitch();
};

void ApplicationVulkanDeferredPBRTest::logic_update(double p_delta) {
    control(p_delta);
    update_descriptors();
}

void ApplicationVulkanDeferredPBRTest::control(double p_delta) { // NOLINT
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
}

void ApplicationVulkanDeferredPBRTest::update_descriptors() {
    camera_component.view = test_camera_view(camera_position, camera_yaw, camera_pitch);
    CameraInfoVKUBO gbuffer_camera_info = make_camera_info_ubo(camera_component);
    CompositionCameraInfo composition_camera_info = make_composition_camera_info();
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    add_render_job([this, gbuffer_camera_info, composition_camera_info, spot_light_info]() {
        Ref<ROBufferVK> new_gbuffer_camera_ubo = ROBufferVK::Builder()
                                                     .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                                     .set_domain(DeviceDomain::HOST_DEVICE)
                                                     .set_size(sizeof(CameraInfoVKUBO))
                                                     .create_ref(base_render_data);
        new_gbuffer_camera_ubo->update_buffer(&gbuffer_camera_info, sizeof(gbuffer_camera_info));
        for (auto& [descriptor_set, binding] : gbuffer_camera_descriptor_bindings) {
            descriptor_set->write_descriptor_set(RODescriptorSetVK::Writer().set_binding(binding).add_uniform_buffer_info(
                new_gbuffer_camera_ubo, 0, sizeof(CameraInfoVKUBO)));
        }

        Ref<ROBufferVK> new_composition_camera_ubo = ROBufferVK::Builder()
                                                         .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                                         .set_domain(DeviceDomain::HOST_DEVICE)
                                                         .set_size(sizeof(CompositionCameraInfo))
                                                         .create_ref(base_render_data);
        new_composition_camera_ubo->update_buffer(&composition_camera_info, sizeof(composition_camera_info));
        auto [direct_camera_set, direct_camera_binding] = direct_lighting_descriptor_set_bindings.at("camera_info");
        direct_lighting_descriptor_sets.at(direct_camera_set)
            ->write_descriptor_set(RODescriptorSetVK::Writer()
                    .set_binding(direct_camera_binding)
                    .add_uniform_buffer_info(new_composition_camera_ubo, 0, sizeof(CompositionCameraInfo)));
        auto [indirect_camera_set, indirect_camera_binding] = indirect_lighting_descriptor_set_bindings.at("camera_info");
        indirect_lighting_descriptor_sets.at(indirect_camera_set)
            ->write_descriptor_set(RODescriptorSetVK::Writer()
                    .set_binding(indirect_camera_binding)
                    .add_uniform_buffer_info(new_composition_camera_ubo, 0, sizeof(CompositionCameraInfo)));

        Ref<ROBufferVK> new_spot_light_info_ubo = ROBufferVK::Builder()
                                                      .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                                      .set_domain(DeviceDomain::HOST_DEVICE)
                                                      .set_size(sizeof(SpotLightInfoVKUBO))
                                                      .create_ref(base_render_data);
        new_spot_light_info_ubo->update_buffer(&spot_light_info, sizeof(spot_light_info));
        for (auto& [descriptor_set, binding] : shadow_camera_descriptor_bindings) {
            descriptor_set->write_descriptor_set(RODescriptorSetVK::Writer().set_binding(binding).add_uniform_buffer_info(
                new_spot_light_info_ubo, 0, sizeof(SpotLightInfoVKUBO)));
        }
        auto [direct_light_set, direct_light_binding] = direct_lighting_descriptor_set_bindings.at("light.lights.info");
        direct_lighting_descriptor_sets.at(direct_light_set)
            ->write_descriptor_set(RODescriptorSetVK::Writer()
                    .set_binding(direct_light_binding)
                    .set_dst_array_element(0)
                    .add_uniform_buffer_info(new_spot_light_info_ubo, 0, sizeof(SpotLightInfoVKUBO)));
        auto [indirect_light_set, indirect_light_binding] = indirect_lighting_descriptor_set_bindings.at("light.lights.info");
        indirect_lighting_descriptor_sets.at(indirect_light_set)
            ->write_descriptor_set(RODescriptorSetVK::Writer()
                    .set_binding(indirect_light_binding)
                    .set_dst_array_element(0)
                    .add_uniform_buffer_info(new_spot_light_info_ubo, 0, sizeof(SpotLightInfoVKUBO)));

        gbuffer_camera_info_ubo = std::move(new_gbuffer_camera_ubo);
        composition_camera_info_ubo = std::move(new_composition_camera_ubo);
        spot_light_info_ubo = std::move(new_spot_light_info_ubo);
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
    create_lighting_output_resources();
    auto shadow_task = compile_shadow_task();
    RTRunStaticOpaqueTrianglesVK::Config task_config;
    VkClearValue color_clear{};
    VkClearValue depth_clear{.depthStencil = {.depth = 0.0F, .stencil = 0}};
    task_config.set_graphics_pipeline_resource_key("deferred_gbuffer")
        .add_color_attachment(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW, GBUFFER_ALBEDO_FORMAT)
        .add_color_attachment(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW, GBUFFER_NORMAL_FORMAT)
        .add_color_attachment(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW, GBUFFER_RMA_FORMAT)
        .set_depth_attachment(REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW, GBUFFER_DEPTH_FORMAT)
        .add_clear_value(color_clear)
        .add_clear_value(color_clear)
        .add_clear_value(color_clear)
        .add_clear_value(depth_clear);
    auto gbuffer_task = make_unique<RTRunStaticOpaqueTrianglesVK>(render_allocator, renderer_vk, task_config);
    compile_lighting_pipelines();
    write_dynamic_camera_descriptors(*gbuffer_task);
    write_dynamic_light_descriptors();
    register_deferred_render_graph(shadow_task, gbuffer_task);
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
    direct_lighting_shader_resource = resource_manager->get_resource<RUShader>("direct_lighting_shader");
    if (direct_lighting_shader_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'direct_lighting_shader' resource not found.");
    }
    indirect_lighting_shader_resource = resource_manager->get_resource<RUShader>("indirect_lighting_shader");
    if (indirect_lighting_shader_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'indirect_lighting_shader' resource not found.");
    }
    post_processing_shader_resource = resource_manager->get_resource<RUShader>("post_processing_shader");
    if (post_processing_shader_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'post_processing_shader' resource not found.");
    }
    present_post_processing_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("present_post_processing");
    if (present_post_processing_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'present_post_processing' graphics pipeline resource not found.");
    }
}

void ApplicationVulkanDeferredPBRTest::setup_static_scene_resources() {
    render_database->set_static_opaque_triangles(sponza_mesh.get());
    renderer_vk->update_static_opaque_triangles();
}

void ApplicationVulkanDeferredPBRTest::setup_spot_light() {
    spot_light_component = make_default_test_spot_light_component(128.0F);
    spot_light_yaw = std::atan2(spot_light_component.direction.z, spot_light_component.direction.x);
    spot_light_pitch = std::asin(std::clamp(spot_light_component.direction.y, -1.0F, 1.0F));
    shadow_extent = VkExtent2D{
        .width = spot_light_component.shadow_map_resolution,
        .height = spot_light_component.shadow_map_resolution,
    };
}

void ApplicationVulkanDeferredPBRTest::create_shadow_map_resources() {
    shadow_depth_image = ROImageVK::Builder().shadow_map_depth(spot_light_component).create_ref(base_render_data);
    shadow_color_image = ROImageVK::Builder()
                             .set_format(RSM_FLUX_FORMAT)
                             .image_2d(shadow_extent)
                             .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                             .create_ref(base_render_data);
    shadow_normal_image = ROImageVK::Builder()
                              .set_format(RSM_NORMAL_FORMAT)
                              .image_2d(shadow_extent)
                              .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                              .create_ref(base_render_data);

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
                            .set_format(RSM_FLUX_FORMAT)
                            .set_subresource_range(color_subresource)
                            .create_ref(base_render_data);
    shadow_normal_view = ROImageViewVK::Builder()
                             .set_image(shadow_normal_image)
                             .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                             .set_format(RSM_NORMAL_FORMAT)
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

void ApplicationVulkanDeferredPBRTest::create_lighting_output_resources() {
    VkExtent2D extent = base_render_data->swap_chain_extent;
    VkImageSubresourceRange color_subresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    direct_lighting_image = ROImageVK::Builder()
                                .set_format(LIGHTING_OUTPUT_FORMAT)
                                .image_2d(extent)
                                .set_usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                .create_ref(base_render_data);
    indirect_lighting_image = ROImageVK::Builder()
                                  .set_format(LIGHTING_OUTPUT_FORMAT)
                                  .image_2d(extent)
                                  .set_usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                  .create_ref(base_render_data);
    post_processed_image = ROImageVK::Builder()
                               .set_format(LIGHTING_OUTPUT_FORMAT)
                               .image_2d(extent)
                               .set_usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                               .create_ref(base_render_data);
    direct_lighting_view = ROImageViewVK::Builder()
                               .set_image(direct_lighting_image)
                               .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                               .set_format(LIGHTING_OUTPUT_FORMAT)
                               .set_subresource_range(color_subresource)
                               .create_ref(base_render_data);
    indirect_lighting_view = ROImageViewVK::Builder()
                                 .set_image(indirect_lighting_image)
                                 .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                 .set_format(LIGHTING_OUTPUT_FORMAT)
                                 .set_subresource_range(color_subresource)
                                 .create_ref(base_render_data);
    post_processed_view = ROImageViewVK::Builder()
                              .set_image(post_processed_image)
                              .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                              .set_format(LIGHTING_OUTPUT_FORMAT)
                              .set_subresource_range(color_subresource)
                              .create_ref(base_render_data);
    lighting_sampler = ROSamplerVK::Builder()
                           .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                           .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                           .create_ref(base_render_data);
}

Unique<RTRunStaticOpaqueTrianglesVK> ApplicationVulkanDeferredPBRTest::compile_shadow_task() {
    RTRunStaticOpaqueTrianglesVK::Config task_config;
    VkClearValue shadow_color_clear{};
    VkClearValue shadow_normal_clear{};
    VkClearValue shadow_depth_clear{.depthStencil = {.depth = 0.0F, .stencil = 0}};
    task_config.set_render_extent(shadow_extent)
        .add_color_attachment(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, RSM_FLUX_FORMAT)
        .add_color_attachment(REND_RES_TEST_RSM_NORMAL_IMAGE_VIEW, RSM_NORMAL_FORMAT)
        .set_depth_attachment(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, SHADOWMAP_DEPTH_FORMAT)
        .add_clear_value(shadow_color_clear)
        .add_clear_value(shadow_normal_clear)
        .add_clear_value(shadow_depth_clear)
        .add_graphics_pipeline_override("forward_cook_torrance_pbr", "rsm_spot")
        .add_graphics_pipeline_override("forward_cook_torrance_pbr_masked", "rsm_spot_masked");
    auto task = make_unique<RTRunStaticOpaqueTrianglesVK>(render_allocator, renderer_vk, task_config);
    for (size_t batch_index = 0; batch_index < task->get_compiled_batch_count(); ++batch_index) {
        auto [set, binding] = task->get_descriptor_binding(batch_index, "light_camera_info");
        shadow_camera_descriptor_bindings.emplace_back(task->get_descriptor_set(batch_index, set), binding);
    }
    return task;
}

void ApplicationVulkanDeferredPBRTest::compile_lighting_pipelines() {
    CompositionCameraInfo camera_info = make_composition_camera_info();
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    uint32_t spot_light_count = 1;

    RenderBindpointDeclarationsBuilder direct_bindpoints;
    direct_bindpoints.add_buffer_decleration("camera_info", &camera_info, sizeof(camera_info))
        .add_buffer_decleration("light.lights[0].info", &spot_light_info, sizeof(spot_light_info))
        .add_image_decleration("light.lights[0].shadowmap", shadow_depth_view)
        .add_image_decleration("light.lights[0].rsm_flux", shadow_color_view)
        .add_image_decleration("light.lights[0].rsm_octa_normal", shadow_normal_view)
        .add_sampler_decleration("light.lights[0].shadowmap_sampler", shadow_sampler)
        .add_buffer_decleration("light.light_count", &spot_light_count, sizeof(spot_light_count))
        .add_image_decleration("gbuffer_albedo", gbuffer_albedo_view)
        .add_image_decleration("gbuffer_octa_normal", gbuffer_normal_view)
        .add_image_decleration("gbuffer_rma", gbuffer_rma_view)
        .add_image_decleration("gbuffer_depth", gbuffer_depth_view)
        .add_sampler_decleration("gbuffer_sampler", gbuffer_sampler)
        .add_image_decleration("direct_lighting_output", direct_lighting_view);

    ComputePipelineCompilerVKContext compute_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
    };
    auto direct_compiled = ComputePipelineCompilerVK(compute_context)
                               .compile("direct_lighting_shader", direct_lighting_shader_resource, direct_bindpoints.build());
    direct_lighting_compute_pipeline = direct_compiled.compute_pipeline;
    direct_lighting_descriptor_sets = std::move(direct_compiled.descriptor_sets);
    direct_lighting_descriptor_set_bindings = std::move(direct_compiled.descriptor_set_bindings);

    RenderBindpointDeclarationsBuilder indirect_bindpoints;
    indirect_bindpoints.add_buffer_decleration("camera_info", &camera_info, sizeof(camera_info))
        .add_buffer_decleration("light.lights[0].info", &spot_light_info, sizeof(spot_light_info))
        .add_image_decleration("light.lights[0].shadowmap", shadow_depth_view)
        .add_image_decleration("light.lights[0].rsm_flux", shadow_color_view)
        .add_image_decleration("light.lights[0].rsm_octa_normal", shadow_normal_view)
        .add_sampler_decleration("light.lights[0].shadowmap_sampler", shadow_sampler)
        .add_buffer_decleration("light.light_count", &spot_light_count, sizeof(spot_light_count))
        .add_image_decleration("gbuffer_albedo", gbuffer_albedo_view)
        .add_image_decleration("gbuffer_octa_normal", gbuffer_normal_view)
        .add_image_decleration("gbuffer_rma", gbuffer_rma_view)
        .add_image_decleration("gbuffer_depth", gbuffer_depth_view)
        .add_sampler_decleration("gbuffer_sampler", gbuffer_sampler)
        .add_image_decleration("indirect_lighting_output", indirect_lighting_view);
    auto indirect_compiled = ComputePipelineCompilerVK(compute_context)
                                 .compile("indirect_lighting_shader", indirect_lighting_shader_resource, indirect_bindpoints.build());
    indirect_lighting_compute_pipeline = indirect_compiled.compute_pipeline;
    indirect_lighting_descriptor_sets = std::move(indirect_compiled.descriptor_sets);
    indirect_lighting_descriptor_set_bindings = std::move(indirect_compiled.descriptor_set_bindings);

    RenderBindpointDeclarationsBuilder post_bindpoints;
    post_bindpoints.add_image_decleration("gbuffer_rma", gbuffer_rma_view)
        .add_image_decleration("gbuffer_depth", gbuffer_depth_view)
        .add_image_decleration("direct_lighting_texture", direct_lighting_view)
        .add_image_decleration("indirect_lighting_texture", indirect_lighting_view)
        .add_image_decleration("post_processed_output", post_processed_view)
        .add_sampler_decleration("gbuffer_sampler", gbuffer_sampler);
    auto post_compiled = ComputePipelineCompilerVK(compute_context)
                             .compile("post_processing_shader", post_processing_shader_resource, post_bindpoints.build());
    post_processing_compute_pipeline = post_compiled.compute_pipeline;
    post_processing_descriptor_sets = std::move(post_compiled.descriptor_sets);
    post_processing_descriptor_set_bindings = std::move(post_compiled.descriptor_set_bindings);

    RenderBindpointDeclarationsBuilder present_bindpoints;
    present_bindpoints.add_image_decleration("post_processed_texture", post_processed_view).add_sampler_decleration("post_processed_sampler", lighting_sampler);
    GraphicsPipelineCompilerVKContext present_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
    };
    auto present_compiled =
        GraphicsPipelineCompilerVK(present_context)
            .compile("present_post_processing", present_post_processing_pipeline_resource, present_bindpoints.build());
    present_post_processing_graphics_pipeline = present_compiled.graphics_pipeline;
    present_post_processing_descriptor_sets = std::move(present_compiled.descriptor_sets);
    present_post_processing_descriptor_set_bindings = std::move(present_compiled.descriptor_set_bindings);
}

void ApplicationVulkanDeferredPBRTest::write_dynamic_camera_descriptors(RTRunStaticOpaqueTrianglesVK& p_gbuffer_task) {
    if (p_gbuffer_task.get_compiled_batch_count() == 0 || direct_lighting_descriptor_sets.empty() ||
        indirect_lighting_descriptor_sets.empty() || post_processing_descriptor_sets.empty()) {
        throw std::runtime_error("Failed to setup deferred test: compiled pipelines produced no descriptor sets.");
    }
    gbuffer_camera_info_ubo = ROBufferVK::Builder()
                                  .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                  .set_domain(DeviceDomain::HOST_DEVICE)
                                  .set_size(sizeof(CameraInfoVKUBO))
                                  .create_ref(base_render_data);
    CameraInfoVKUBO gbuffer_camera_info = make_camera_info_ubo(camera_component);
    gbuffer_camera_info_ubo->update_buffer(&gbuffer_camera_info, sizeof(gbuffer_camera_info));
    for (size_t batch_index = 0; batch_index < p_gbuffer_task.get_compiled_batch_count(); ++batch_index) {
        auto [gbuffer_set, gbuffer_binding] = p_gbuffer_task.get_descriptor_binding(batch_index, "camera_info");
        auto descriptor_set = p_gbuffer_task.get_descriptor_set(batch_index, gbuffer_set);
        gbuffer_camera_descriptor_bindings.emplace_back(descriptor_set, gbuffer_binding);
        descriptor_set->write_descriptor_set(
            RODescriptorSetVK::Writer().set_binding(gbuffer_binding).add_uniform_buffer_info(gbuffer_camera_info_ubo, 0, sizeof(CameraInfoVKUBO)));
    }

    composition_camera_info_ubo = ROBufferVK::Builder()
                                      .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                      .set_domain(DeviceDomain::HOST_DEVICE)
                                      .set_size(sizeof(CompositionCameraInfo))
                                      .create_ref(base_render_data);
    CompositionCameraInfo composition_camera_info = make_composition_camera_info();
    composition_camera_info_ubo->update_buffer(&composition_camera_info, sizeof(composition_camera_info));
    auto [direct_set, direct_binding] = direct_lighting_descriptor_set_bindings.at("camera_info");
    direct_lighting_descriptor_sets[direct_set]->write_descriptor_set(
        RODescriptorSetVK::Writer().set_binding(direct_binding).add_uniform_buffer_info(composition_camera_info_ubo, 0, sizeof(CompositionCameraInfo)));
    auto [indirect_set, indirect_binding] = indirect_lighting_descriptor_set_bindings.at("camera_info");
    indirect_lighting_descriptor_sets[indirect_set]->write_descriptor_set(
        RODescriptorSetVK::Writer().set_binding(indirect_binding).add_uniform_buffer_info(composition_camera_info_ubo, 0, sizeof(CompositionCameraInfo)));
}

void ApplicationVulkanDeferredPBRTest::write_dynamic_light_descriptors() {
    if (shadow_camera_descriptor_bindings.empty() || direct_lighting_descriptor_sets.empty() ||
        indirect_lighting_descriptor_sets.empty()) {
        throw std::runtime_error("Failed to setup deferred test: compiled light pipelines produced no descriptor sets.");
    }
    spot_light_info_ubo = ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .set_domain(DeviceDomain::HOST_DEVICE)
                              .set_size(sizeof(SpotLightInfoVKUBO))
                              .create_ref(base_render_data);
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    spot_light_info_ubo->update_buffer(&spot_light_info, sizeof(spot_light_info));

    for (auto& [descriptor_set, binding] : shadow_camera_descriptor_bindings) {
        descriptor_set->write_descriptor_set(RODescriptorSetVK::Writer().set_binding(binding).add_uniform_buffer_info(
            spot_light_info_ubo, 0, sizeof(SpotLightInfoVKUBO)));
    }

    auto [direct_set, direct_binding] = direct_lighting_descriptor_set_bindings.at("light.lights.info");
    direct_lighting_descriptor_sets.at(direct_set)
        ->write_descriptor_set(RODescriptorSetVK::Writer()
                .set_binding(direct_binding)
                .set_dst_array_element(0)
                .add_uniform_buffer_info(spot_light_info_ubo, 0, sizeof(SpotLightInfoVKUBO)));
    auto [indirect_set, indirect_binding] = indirect_lighting_descriptor_set_bindings.at("light.lights.info");
    indirect_lighting_descriptor_sets.at(indirect_set)
        ->write_descriptor_set(RODescriptorSetVK::Writer()
                .set_binding(indirect_binding)
                .set_dst_array_element(0)
                .add_uniform_buffer_info(spot_light_info_ubo, 0, sizeof(SpotLightInfoVKUBO)));
}

void ApplicationVulkanDeferredPBRTest::register_deferred_render_graph(
    Unique<RTRunStaticOpaqueTrianglesVK>& p_shadow_task, Unique<RTRunStaticOpaqueTrianglesVK>& p_gbuffer_task) {
    RendGraphDescVK pipeline_desc(render_allocator);
    add_render_graph_resources(pipeline_desc);
    pipeline_desc.tasks.emplace_back(render_allocator, RT_RUN_PIPELINE_SHADOWMAP, std::move(p_shadow_task));
    RendTaskDescVK gbuffer_task_desc(render_allocator, RT_RUN_DEFERRED_GBUFFER, std::move(p_gbuffer_task));
    gbuffer_task_desc.prev.emplace_back(RT_RUN_PIPELINE_SHADOWMAP);
    pipeline_desc.tasks.emplace_back(std::move(gbuffer_task_desc));
    pipeline_desc.tasks.emplace_back(create_direct_lighting_task_desc());
    pipeline_desc.tasks.emplace_back(create_indirect_lighting_task_desc());
    pipeline_desc.tasks.emplace_back(create_post_processing_task_desc());
    pipeline_desc.tasks.emplace_back(create_present_post_processing_task_desc());
    renderer_vk->register_render_graph("deferred_pipeline", std::move(pipeline_desc));
    renderer_vk->activate_render_graph("deferred_pipeline");
}

void ApplicationVulkanDeferredPBRTest::add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const {
    p_pipeline_desc.resources.emplace_back(RendResVK{
        .res_id = std::string(REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK), .res_type_id = TypeUUID<ROImageViewVK>::UUID, .res = MEM_NULL});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_DIRECT_LIGHTING_COMPUTE_PIPELINE),
        .res_type_id = TypeUUID<ROComputePipelineVK>::UUID,
        .res = direct_lighting_compute_pipeline});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_INDIRECT_LIGHTING_COMPUTE_PIPELINE),
        .res_type_id = TypeUUID<ROComputePipelineVK>::UUID,
        .res = indirect_lighting_compute_pipeline});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_POST_PROCESSING_COMPUTE_PIPELINE),
        .res_type_id = TypeUUID<ROComputePipelineVK>::UUID,
        .res = post_processing_compute_pipeline});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_PRESENT_POST_PROCESSING_GRAPHICS_PIPELINE),
        .res_type_id = TypeUUID<ROGraphicsPipelineVK>::UUID,
        .res = present_post_processing_graphics_pipeline});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW),
        .res_type_id = TypeUUID<ROImageViewVK>::UUID,
        .res = shadow_color_view});
    p_pipeline_desc.resources.emplace_back(RendResVK{
        .res_id = std::string(REND_RES_TEST_RSM_NORMAL_IMAGE_VIEW), .res_type_id = TypeUUID<ROImageViewVK>::UUID, .res = shadow_normal_view});
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
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_DIRECT_LIGHTING_IMAGE_VIEW),
        .res_type_id = TypeUUID<ROImageViewVK>::UUID,
        .res = direct_lighting_view});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_INDIRECT_LIGHTING_IMAGE_VIEW),
        .res_type_id = TypeUUID<ROImageViewVK>::UUID,
        .res = indirect_lighting_view});
    p_pipeline_desc.resources.emplace_back(RendResVK{.res_id = std::string(REND_RES_TEST_POST_PROCESSED_IMAGE_VIEW),
        .res_type_id = TypeUUID<ROImageViewVK>::UUID,
        .res = post_processed_view});
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_direct_lighting_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_DIRECT_LIGHTING;
    task_desc.prev.emplace_back(RT_RUN_DEFERRED_GBUFFER);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_descriptor_sets = direct_lighting_descriptor_sets]() -> Unique<RenderTaskVK> {
        std::array<ResAccessVK, 7> read_accesses{{
            {.res_id = std::string(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_RSM_NORMAL_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
        }};
        std::array<ResAccessVK, 1> write_accesses{{
            {.res_id = std::string(REND_RES_TEST_DIRECT_LIGHTING_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_WRITE_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
        }};
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator,
            task_base_render_data,
            REND_RES_TEST_DIRECT_LIGHTING_COMPUTE_PIPELINE,
            dispatch_group_count,
            std::span<const Ref<RODescriptorSetVK>>(task_descriptor_sets.data(), task_descriptor_sets.size()),
            std::span<const ResAccessVK>(read_accesses.data(), read_accesses.size()),
            std::span<const ResAccessVK>(write_accesses.data(), write_accesses.size()));
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_indirect_lighting_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_INDIRECT_LIGHTING;
    task_desc.prev.emplace_back(RT_RUN_DEFERRED_GBUFFER);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_descriptor_sets = indirect_lighting_descriptor_sets]() -> Unique<RenderTaskVK> {
        std::array<ResAccessVK, 7> read_accesses{{
            {.res_id = std::string(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_RSM_NORMAL_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
        }};
        std::array<ResAccessVK, 1> write_accesses{{
            {.res_id = std::string(REND_RES_TEST_INDIRECT_LIGHTING_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_WRITE_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
        }};
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator,
            task_base_render_data,
            REND_RES_TEST_INDIRECT_LIGHTING_COMPUTE_PIPELINE,
            dispatch_group_count,
            std::span<const Ref<RODescriptorSetVK>>(task_descriptor_sets.data(), task_descriptor_sets.size()),
            std::span<const ResAccessVK>(read_accesses.data(), read_accesses.size()),
            std::span<const ResAccessVK>(write_accesses.data(), write_accesses.size()));
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_post_processing_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_POST_PROCESSING;
    task_desc.prev.emplace_back(RT_RUN_DIRECT_LIGHTING);
    task_desc.prev.emplace_back(RT_RUN_INDIRECT_LIGHTING);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_descriptor_sets = post_processing_descriptor_sets]() -> Unique<RenderTaskVK> {
        std::array<ResAccessVK, 5> read_accesses{{
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW),
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_DIRECT_LIGHTING_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {.res_id = std::string(REND_RES_TEST_INDIRECT_LIGHTING_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
        }};
        std::array<ResAccessVK, 1> write_accesses{{
            {.res_id = std::string(REND_RES_TEST_POST_PROCESSED_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_WRITE_BIT, .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
        }};
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator,
            task_base_render_data,
            REND_RES_TEST_POST_PROCESSING_COMPUTE_PIPELINE,
            dispatch_group_count,
            std::span<const Ref<RODescriptorSetVK>>(task_descriptor_sets.data(), task_descriptor_sets.size()),
            std::span<const ResAccessVK>(read_accesses.data(), read_accesses.size()),
            std::span<const ResAccessVK>(write_accesses.data(), write_accesses.size()));
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_present_post_processing_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_PRESENT_POST_PROCESSING;
    task_desc.prev.emplace_back(RT_RUN_POST_PROCESSING);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_descriptor_sets = present_post_processing_descriptor_sets]() -> Unique<RenderTaskVK> {
        std::array<ResAccessVK, 1> read_accesses{{
            {.res_id = std::string(REND_RES_TEST_POST_PROCESSED_IMAGE_VIEW), .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT},
        }};
        return make_unique<RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
            task_base_render_data->swap_chain_extent,
            REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK,
            REND_RES_TEST_PRESENT_POST_PROCESSING_GRAPHICS_PIPELINE,
            std::nullopt,
            std::nullopt,
            std::span<const Ref<RODescriptorSetVK>>(task_descriptor_sets.data(), task_descriptor_sets.size()),
            std::nullopt,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VertexInputDescriptionVK{},
            0,
            std::span<const ResAccessVK>(read_accesses.data(), read_accesses.size()));
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
