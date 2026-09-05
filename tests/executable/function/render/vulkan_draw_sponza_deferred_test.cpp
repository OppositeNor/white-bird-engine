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
#include "core/engine_core.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/function_utils.hh"
#include "function/input_manager/input_manager.hh"
#include "function/input_manager/key_codes.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_vk.hh"
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
#include "function/render_vk/roc_per_frame_vk.hh"
#include "function/render_vk/rt_generate_mipmaps_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_run_static_set_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "glm/ext/matrix_transform.hpp"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
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

namespace {

Ref<ROCPerFrameVK> create_per_frame_uniform_buffers(
    RenderAllocator* p_allocator, const BaseRenderDataVK* p_render_data, VkDeviceSize p_buffer_size) {
    return make_ref<ROCPerFrameVK>(
        p_allocator, p_render_data, [p_render_data, p_buffer_size](uint32_t /*p_frame_slot*/) -> Ref<RenderObjectVK> {
            return ROBufferVK::Builder()
                .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                .set_domain(DeviceDomain::HOST_DEVICE)
                .set_size(p_buffer_size)
                .create_ref(p_render_data);
        });
}

} // namespace

constexpr std::string_view DEFERRED_MIPMAP_RENDER_GRAPH = "deferred_pipeline_mipmap";
constexpr std::string_view DEFERRED_SUMMED_AREA_TABLE_RENDER_GRAPH = "deferred_pipeline_summed_area_table";
constexpr std::string_view REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_COMPUTE_PIPELINE = "shadow_sat_horizontal_pipeline";
constexpr std::string_view REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_VERTICAL_COMPUTE_PIPELINE = "shadow_sat_vertical_pipeline";
constexpr std::string_view REND_RES_TEST_DIRECT_LIGHTING_COMPUTE_PIPELINE = "direct_lighting_pipeline";
constexpr std::string_view REND_RES_TEST_DIRECT_LIGHTING_SUMMED_AREA_TABLE_COMPUTE_PIPELINE = "direct_lighting_sat_pipeline";
constexpr std::string_view REND_RES_TEST_INDIRECT_LIGHTING_COMPUTE_PIPELINE = "indirect_lighting_pipeline";
constexpr std::string_view REND_RES_TEST_POST_PROCESSING_COMPUTE_PIPELINE = "post_processing_pipeline";
constexpr std::string_view REND_RES_TEST_PRESENT_POST_PROCESSING_GRAPHICS_PIPELINE = "present_post_processing_pipeline";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW = "gbuffer_albedo";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW = "gbuffer_normal";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW = "gbuffer_rma";
constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW = "gbuffer_depth";
constexpr std::string_view REND_RES_TEST_SHADOW_MOMENTS_IMAGE_VIEW = "shadow_moments";
constexpr std::string_view REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE_VIEW = "shadow_sat_horizontal";
constexpr std::string_view REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW = "shadow_sat";
constexpr std::string_view REND_RES_TEST_DIRECT_LIGHTING_IMAGE_VIEW = "direct_lighting";
constexpr std::string_view REND_RES_TEST_INDIRECT_LIGHTING_IMAGE_VIEW = "indirect_lighting";
constexpr std::string_view REND_RES_TEST_POST_PROCESSED_IMAGE_VIEW = "post_processed";
inline constexpr std::string_view RT_RUN_DEFERRED_GBUFFER = "run_gbuffer";
inline constexpr std::string_view RT_RUN_DEFERRED_GBUFFER_MASKED = "run_gbuffer_masked";
inline constexpr std::string_view RT_RUN_DEFERRED_SHADOW_OPAQUE = "run_shadow_opaque";
inline constexpr std::string_view RT_RUN_DEFERRED_SHADOW_MASKED = "run_shadow_masked";
inline constexpr std::string_view RT_GENERATE_SHADOW_MIPMAPS = "generate_shadow_mipmaps";
inline constexpr std::string_view RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL = "build_shadow_sat_horizontal";
inline constexpr std::string_view RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE_VERTICAL = "build_shadow_sat_vertical";
inline constexpr std::string_view RT_RUN_DIRECT_LIGHTING = "run_direct_lighting";
inline constexpr std::string_view RT_RUN_INDIRECT_LIGHTING = "run_indirect_lighting";
inline constexpr std::string_view RT_RUN_POST_PROCESSING = "run_post_processing";
inline constexpr std::string_view RT_RUN_PRESENT_POST_PROCESSING = "present_post_processing";

constexpr VkFormat GBUFFER_ALBEDO_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_NORMAL_FORMAT = VK_FORMAT_R16G16_SNORM;
constexpr VkFormat GBUFFER_RMA_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOW_MOMENTS_FORMAT = VK_FORMAT_R32G32_SFLOAT;
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

    struct TaskUniformBinding {
        RTRunStaticSetVK* task = nullptr;
        uint32_t set_index = 0;
        uint32_t binding = 0;
        uint32_t dst_array_element = 0;
    };

    struct ComputeUniformBinding {
        ROComputePipelineVK* pipeline = nullptr;
        uint32_t set_index = 0;
        uint32_t binding = 0;
        uint32_t dst_array_element = 0;
    };

    void control(double p_delta);
    void update_descriptors();
    Ref<ROBufferVK> get_frame_uniform_buffer(
        Ref<ROCPerFrameVK> p_buffer_container, uint32_t p_frame_slot, std::string_view p_buffer_label) const;
    void write_task_uniform_bindings(
        std::span<const TaskUniformBinding> p_bindings, Ref<ROCPerFrameVK> p_buffer_container, VkDeviceSize p_buffer_size);
    void write_compute_uniform_bindings(
        std::span<ComputeUniformBinding> p_bindings, Ref<ROCPerFrameVK> p_buffer_container, VkDeviceSize p_buffer_size);

    void setup_render_context();
    void load_resources();
    void setup_static_scene_resources();
    void setup_spot_light();
    void create_shadow_map_resources();
    void create_gbuffer_resources();
    void create_lighting_output_resources();
    Unique<RTRunStaticSetVK> compile_shadow_task(RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_pipeline, bool p_load_existing);
    Unique<RTRunStaticSetVK> compile_gbuffer_task(RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_pipeline, bool p_load_existing);
    void compile_shadow_moments_summed_area_table_pipelines();
    void compile_lighting_pipelines();
    void write_dynamic_camera_descriptors(std::span<RTRunStaticSetVK* const> p_gbuffer_tasks);
    void write_dynamic_light_descriptors(std::span<RTRunStaticSetVK* const> p_shadow_tasks);
    void register_deferred_render_graph(std::string_view p_graph_id,
        bool p_use_summed_area_table,
        Unique<RTRunStaticSetVK>& p_shadow_opaque_task,
        Unique<RTRunStaticSetVK>& p_shadow_masked_task,
        Unique<RTRunStaticSetVK>& p_gbuffer_opaque_task,
        Unique<RTRunStaticSetVK>& p_gbuffer_masked_task);
    void add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const;
    RendTaskDescVK create_shadow_mipmap_task_desc() const;
    RendTaskDescVK create_shadow_moments_summed_area_table_horizontal_task_desc() const;
    RendTaskDescVK create_shadow_moments_summed_area_table_vertical_task_desc() const;
    RendTaskDescVK create_direct_lighting_task_desc(bool p_use_summed_area_table) const;
    RendTaskDescVK create_indirect_lighting_task_desc() const;
    RendTaskDescVK create_post_processing_task_desc() const;
    RendTaskDescVK create_present_post_processing_task_desc() const;
    CompositionCameraInfo make_composition_camera_info() const;

    RendererVK* renderer_vk = nullptr;
    const BaseRenderDataVK* base_render_data = nullptr;
    RenderAllocator* render_allocator = nullptr;
    RefRes<RUStaticOpaqueSet> sponza_opaque_set;
    RefRes<RUStaticMaskedSet> sponza_masked_set;
    RefRes<RUGraphicsPipeline> shadow_opaque_pipeline_resource;
    RefRes<RUGraphicsPipeline> shadow_masked_pipeline_resource;
    RefRes<RUGraphicsPipeline> gbuffer_opaque_pipeline_resource;
    RefRes<RUGraphicsPipeline> gbuffer_masked_pipeline_resource;
    RefRes<RUShader> shadow_moments_summed_area_table_horizontal_shader_resource;
    RefRes<RUShader> shadow_moments_summed_area_table_vertical_shader_resource;
    RefRes<RUShader> direct_lighting_shader_resource;
    RefRes<RUShader> direct_lighting_summed_area_table_shader_resource;
    RefRes<RUGraphicsPipeline> present_post_processing_pipeline_resource;
    RefRes<RUShader> indirect_lighting_shader_resource;
    RefRes<RUShader> post_processing_shader_resource;

    Ref<ROComputePipelineVK> shadow_moments_summed_area_table_horizontal_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> shadow_moments_summed_area_table_vertical_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> direct_lighting_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> direct_lighting_summed_area_table_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> indirect_lighting_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> post_processing_compute_pipeline = MEM_NULL;
    Ref<ROGraphicsPipelineVK> present_post_processing_graphics_pipeline = MEM_NULL;
    ComputePipelineCompilerVK::DescriptorSetBindings direct_lighting_descriptor_set_bindings;
    ComputePipelineCompilerVK::DescriptorSetBindings direct_lighting_summed_area_table_descriptor_set_bindings;
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
    Ref<ROImageVK> shadow_moments_image;
    Ref<ROImageVK> shadow_moments_summed_area_table_horizontal_image;
    Ref<ROImageVK> shadow_moments_summed_area_table_image;
    Ref<ROImageViewVK> shadow_depth_view;
    Ref<ROImageViewVK> shadow_color_view;
    Ref<ROImageViewVK> shadow_normal_view;
    Ref<ROImageViewVK> shadow_moments_attachment_view;
    Ref<ROImageViewVK> shadow_moments_sampled_view;
    Ref<ROImageViewVK> shadow_moments_summed_area_table_horizontal_view;
    Ref<ROImageViewVK> shadow_moments_summed_area_table_view;
    Ref<ROSamplerVK> shadow_sampler;
    Ref<ROSamplerVK> gbuffer_sampler;
    Ref<ROSamplerVK> lighting_sampler;
    Ref<ROCPerFrameVK> gbuffer_camera_info_ubos;
    Ref<ROCPerFrameVK> composition_camera_info_ubos;
    Ref<ROCPerFrameVK> spot_light_info_ubos;
    std::vector<TaskUniformBinding> gbuffer_camera_bindings;
    std::vector<TaskUniformBinding> shadow_light_bindings;
    std::vector<ComputeUniformBinding> composition_camera_bindings;
    std::vector<ComputeUniformBinding> spot_light_bindings;
    ComponentSpotLight spot_light_component{};
    float spot_light_yaw = 0.0F;
    float spot_light_pitch = 0.0F;
    VkExtent2D shadow_extent{};
    uint32_t shadow_mipmap_level_count = 1;
    bool render_graph_switch_key_was_down = false;
    std::atomic<bool> summed_area_table_requested = false;
    bool summed_area_table_active = false;
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

    bool render_graph_switch_key_down = input_manager->get_input_status(KEYCODE_R).is_down();
    if (render_graph_switch_key_down && !render_graph_switch_key_was_down) {
        bool requested = !summed_area_table_requested.load(std::memory_order_relaxed);
        summed_area_table_requested.store(requested, std::memory_order_relaxed);
    }
    render_graph_switch_key_was_down = render_graph_switch_key_down;

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
        gbuffer_camera_info_ubos->publish_buffer_update(&gbuffer_camera_info, 0, sizeof(gbuffer_camera_info));
        composition_camera_info_ubos->publish_buffer_update(&composition_camera_info, 0, sizeof(composition_camera_info));
        spot_light_info_ubos->publish_buffer_update(&spot_light_info, 0, sizeof(spot_light_info));
    });
}

Ref<ROBufferVK> ApplicationVulkanDeferredPBRTest::get_frame_uniform_buffer(
    Ref<ROCPerFrameVK> p_buffer_container, uint32_t p_frame_slot, std::string_view p_buffer_label) const {
    if (p_buffer_container == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to resolve {}: container is null.", p_buffer_label));
    }
    Ref<ROBufferVK> buffer = p_buffer_container->get(p_frame_slot).dynamic_cast_ref<ROBufferVK>();
    if (buffer == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to resolve {} for frame slot {}.", p_buffer_label, p_frame_slot));
    }
    return buffer;
}

void ApplicationVulkanDeferredPBRTest::write_task_uniform_bindings(
    std::span<const TaskUniformBinding> p_bindings, Ref<ROCPerFrameVK> p_buffer_container, VkDeviceSize p_buffer_size) {
    for (const TaskUniformBinding& binding : p_bindings) {
        if (binding.task == nullptr) {
            throw std::runtime_error("Failed to write deferred task uniform binding: task is null.");
        }
        binding.task->write_descriptor_set(binding.set_index,
            RODescriptorSetVK::Writer()
                .set_binding(binding.binding)
                .set_dst_array_element(binding.dst_array_element)
                .add_uniform_buffer_info(p_buffer_container, 0, p_buffer_size));
    }
}

void ApplicationVulkanDeferredPBRTest::write_compute_uniform_bindings(
    std::span<ComputeUniformBinding> p_bindings, Ref<ROCPerFrameVK> p_buffer_container, VkDeviceSize p_buffer_size) {
    for (ComputeUniformBinding& binding : p_bindings) {
        if (binding.pipeline == nullptr) {
            throw std::runtime_error("Failed to write deferred compute uniform binding: pipeline is null.");
        }
        binding.pipeline->write_descriptor_set(binding.set_index,
            RODescriptorSetVK::Writer()
                .set_binding(binding.binding)
                .set_dst_array_element(binding.dst_array_element)
                .add_uniform_buffer_info(p_buffer_container, 0, p_buffer_size));
    }
}

void ApplicationVulkanDeferredPBRTest::render_update(double /* p_delta */) {
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
    setup_spot_light();
    create_shadow_map_resources();
    create_gbuffer_resources();
    create_lighting_output_resources();
    compile_shadow_moments_summed_area_table_pipelines();
    compile_lighting_pipelines();
    auto mipmap_shadow_opaque_task = compile_shadow_task(sponza_opaque_set, shadow_opaque_pipeline_resource, false);
    auto mipmap_shadow_masked_task = compile_shadow_task(sponza_masked_set, shadow_masked_pipeline_resource, true);
    auto summed_area_table_shadow_opaque_task = compile_shadow_task(sponza_opaque_set, shadow_opaque_pipeline_resource, false);
    auto summed_area_table_shadow_masked_task = compile_shadow_task(sponza_masked_set, shadow_masked_pipeline_resource, true);
    auto mipmap_gbuffer_opaque_task = compile_gbuffer_task(sponza_opaque_set, gbuffer_opaque_pipeline_resource, false);
    auto mipmap_gbuffer_masked_task = compile_gbuffer_task(sponza_masked_set, gbuffer_masked_pipeline_resource, true);
    auto summed_area_table_gbuffer_opaque_task = compile_gbuffer_task(sponza_opaque_set, gbuffer_opaque_pipeline_resource, false);
    auto summed_area_table_gbuffer_masked_task = compile_gbuffer_task(sponza_masked_set, gbuffer_masked_pipeline_resource, true);
    std::array<RTRunStaticSetVK*, 4> gbuffer_tasks{mipmap_gbuffer_opaque_task.get(),
        mipmap_gbuffer_masked_task.get(),
        summed_area_table_gbuffer_opaque_task.get(),
        summed_area_table_gbuffer_masked_task.get()};
    std::array<RTRunStaticSetVK*, 4> shadow_tasks{mipmap_shadow_opaque_task.get(),
        mipmap_shadow_masked_task.get(),
        summed_area_table_shadow_opaque_task.get(),
        summed_area_table_shadow_masked_task.get()};
    write_dynamic_camera_descriptors(gbuffer_tasks);
    write_dynamic_light_descriptors(shadow_tasks);
    register_deferred_render_graph(DEFERRED_MIPMAP_RENDER_GRAPH,
        false,
        mipmap_shadow_opaque_task,
        mipmap_shadow_masked_task,
        mipmap_gbuffer_opaque_task,
        mipmap_gbuffer_masked_task);
    register_deferred_render_graph(
        DEFERRED_SUMMED_AREA_TABLE_RENDER_GRAPH,
        true,
        summed_area_table_shadow_opaque_task,
        summed_area_table_shadow_masked_task,
        summed_area_table_gbuffer_opaque_task,
        summed_area_table_gbuffer_masked_task);
    renderer_vk->activate_render_graph(DEFERRED_MIPMAP_RENDER_GRAPH);
}

Unique<RTRunStaticSetVK> ApplicationVulkanDeferredPBRTest::compile_gbuffer_task(
    RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_pipeline, bool p_load_existing) {
    RTRunStaticSetVK::Config task_config;
    VkClearValue color_clear{};
    VkClearValue depth_clear{.depthStencil = {.depth = 0.0F, .stencil = 0}};
    task_config.add_color_attachment(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW, GBUFFER_ALBEDO_FORMAT)
        .add_color_attachment(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW, GBUFFER_NORMAL_FORMAT)
        .add_color_attachment(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW, GBUFFER_RMA_FORMAT)
        .set_depth_attachment(REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW, GBUFFER_DEPTH_FORMAT);
    if (p_load_existing) {
        task_config.set_color_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD).set_depth_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    } else {
        task_config.add_clear_value(color_clear).add_clear_value(color_clear).add_clear_value(color_clear).add_clear_value(depth_clear);
    }
    return make_unique<RTRunStaticSetVK>(render_allocator, renderer_vk, p_set, p_pipeline, task_config);
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
    sponza_opaque_set = resource_manager->get_resource<RUStaticOpaqueSet>("sponza.static_opaque_set");
    if (sponza_opaque_set == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: static opaque set resource was not found.");
    }
    sponza_masked_set = resource_manager->get_resource<RUStaticMaskedSet>("sponza.static_masked_set");
    if (sponza_masked_set == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: static masked set resource was not found.");
    }
    shadow_opaque_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("rsm_spot_static");
    if (shadow_opaque_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'rsm_spot_static' graphics pipeline resource not found.");
    }
    shadow_masked_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("rsm_spot_masked_static");
    if (shadow_masked_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'rsm_spot_masked_static' graphics pipeline resource not found.");
    }
    gbuffer_opaque_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("deferred_gbuffer_static");
    if (gbuffer_opaque_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'deferred_gbuffer_static' graphics pipeline resource not found.");
    }
    gbuffer_masked_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("deferred_gbuffer_masked_static");
    if (gbuffer_masked_pipeline_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'deferred_gbuffer_masked_static' graphics pipeline resource not found.");
    }
    shadow_moments_summed_area_table_horizontal_shader_resource =
        resource_manager->get_resource<RUShader>("shadow_moments_summed_area_table_horizontal_shader");
    if (shadow_moments_summed_area_table_horizontal_shader_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'shadow_moments_summed_area_table_horizontal_shader' resource not found.");
    }
    shadow_moments_summed_area_table_vertical_shader_resource =
        resource_manager->get_resource<RUShader>("shadow_moments_summed_area_table_vertical_shader");
    if (shadow_moments_summed_area_table_vertical_shader_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'shadow_moments_summed_area_table_vertical_shader' resource not found.");
    }
    direct_lighting_shader_resource = resource_manager->get_resource<RUShader>("direct_lighting_shader");
    if (direct_lighting_shader_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'direct_lighting_shader' resource not found.");
    }
    direct_lighting_summed_area_table_shader_resource =
        resource_manager->get_resource<RUShader>("direct_lighting_summed_area_table_shader");
    if (direct_lighting_summed_area_table_shader_resource == nullptr) {
        throw std::runtime_error("Failed to setup deferred test: 'direct_lighting_summed_area_table_shader' resource not found.");
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
    shadow_moments_image = ROImageVK::Builder()
                               .set_format(SHADOW_MOMENTS_FORMAT)
                               .image_2d(shadow_extent)
                               .set_mipmap_level(shadow_mipmap_level_count)
                               .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                               .create_ref(base_render_data);
    shadow_moments_summed_area_table_horizontal_image = ROImageVK::Builder()
                                                            .set_format(SHADOW_MOMENTS_FORMAT)
                                                            .image_2d(shadow_extent)
                                                            .set_usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                                            .create_ref(base_render_data);
    shadow_moments_summed_area_table_image = ROImageVK::Builder()
                                                 .set_format(SHADOW_MOMENTS_FORMAT)
                                                 .image_2d(shadow_extent)
                                                 .set_usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
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
    shadow_moments_attachment_view = ROImageViewVK::Builder()
                                         .set_image(shadow_moments_image)
                                         .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                         .set_format(SHADOW_MOMENTS_FORMAT)
                                         .set_subresource_range(color_subresource)
                                         .create_ref(base_render_data);
    VkImageSubresourceRange moments_sampled_subresource = color_subresource;
    moments_sampled_subresource.levelCount = shadow_mipmap_level_count;
    shadow_moments_sampled_view = ROImageViewVK::Builder()
                                      .set_image(shadow_moments_image)
                                      .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                      .set_format(SHADOW_MOMENTS_FORMAT)
                                      .set_subresource_range(moments_sampled_subresource)
                                      .create_ref(base_render_data);
    shadow_moments_summed_area_table_horizontal_view = ROImageViewVK::Builder()
                                                           .set_image(shadow_moments_summed_area_table_horizontal_image)
                                                           .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                                           .set_format(SHADOW_MOMENTS_FORMAT)
                                                           .set_subresource_range(color_subresource)
                                                           .create_ref(base_render_data);
    shadow_moments_summed_area_table_view = ROImageViewVK::Builder()
                                                .set_image(shadow_moments_summed_area_table_image)
                                                .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                                .set_format(SHADOW_MOMENTS_FORMAT)
                                                .set_subresource_range(color_subresource)
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

Unique<RTRunStaticSetVK> ApplicationVulkanDeferredPBRTest::compile_shadow_task(
    RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_pipeline, bool p_load_existing) {
    RTRunStaticSetVK::Config task_config;
    VkClearValue shadow_color_clear{};
    VkClearValue shadow_normal_clear{};
    VkClearValue shadow_moments_clear{};
    shadow_moments_clear.color.float32[0] = 1.0F;
    shadow_moments_clear.color.float32[1] = 1.0F;
    VkClearValue shadow_depth_clear{.depthStencil = {.depth = 0.0F, .stencil = 0}};
    task_config.set_render_extent(shadow_extent)
        .add_color_attachment(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, RSM_FLUX_FORMAT)
        .add_color_attachment(REND_RES_TEST_RSM_NORMAL_IMAGE_VIEW, RSM_NORMAL_FORMAT)
        .add_color_attachment(REND_RES_TEST_SHADOW_MOMENTS_IMAGE_VIEW, SHADOW_MOMENTS_FORMAT)
        .set_depth_attachment(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, SHADOWMAP_DEPTH_FORMAT);
    if (p_load_existing) {
        task_config.set_color_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD).set_depth_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    } else {
        task_config.add_clear_value(shadow_color_clear)
            .add_clear_value(shadow_normal_clear)
            .add_clear_value(shadow_moments_clear)
            .add_clear_value(shadow_depth_clear);
    }
    auto task = make_unique<RTRunStaticSetVK>(render_allocator, renderer_vk, p_set, p_pipeline, task_config);
    return task;
}

void ApplicationVulkanDeferredPBRTest::compile_shadow_moments_summed_area_table_pipelines() {
    ComputePipelineCompilerVKContext compute_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
    };

    RenderBindpointDeclarationsBuilder horizontal_bindpoints;
    horizontal_bindpoints.add_image_decleration("shadow_moments_input", shadow_moments_attachment_view)
        .add_image_decleration("shadow_moments_horizontal_output", shadow_moments_summed_area_table_horizontal_view);
    auto horizontal_compiled = ComputePipelineCompilerVK(compute_context)
                                   .compile("shadow_moments_summed_area_table_horizontal_shader",
                                       shadow_moments_summed_area_table_horizontal_shader_resource,
                                       horizontal_bindpoints.build());
    shadow_moments_summed_area_table_horizontal_compute_pipeline = horizontal_compiled.compute_pipeline;

    RenderBindpointDeclarationsBuilder vertical_bindpoints;
    vertical_bindpoints
        .add_image_decleration("shadow_moments_horizontal_input", shadow_moments_summed_area_table_horizontal_view)
        .add_image_decleration("shadow_moments_summed_area_table_output", shadow_moments_summed_area_table_view);
    auto vertical_compiled = ComputePipelineCompilerVK(compute_context)
                                 .compile("shadow_moments_summed_area_table_vertical_shader",
                                     shadow_moments_summed_area_table_vertical_shader_resource,
                                     vertical_bindpoints.build());
    shadow_moments_summed_area_table_vertical_compute_pipeline = vertical_compiled.compute_pipeline;
}

void ApplicationVulkanDeferredPBRTest::compile_lighting_pipelines() {
    CompositionCameraInfo camera_info = make_composition_camera_info();
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    uint32_t spot_light_count = 1;

    auto make_direct_bindpoints = [&](Ref<ROImageViewVK> p_shadow_moments_view, bool p_use_summed_area_table) {
        RenderBindpointDeclarationsBuilder bindpoints;
        bindpoints.add_buffer_decleration("camera_info", &camera_info, sizeof(camera_info))
            .add_buffer_decleration("light.lights[0].info", &spot_light_info, sizeof(spot_light_info))
            .add_image_decleration("light.lights[0].shadowmap", p_shadow_moments_view);
        if (p_use_summed_area_table) {
            bindpoints.add_image_decleration("light.lights[0].shadow_moments_summed_area_table", shadow_moments_summed_area_table_view);
        }
        bindpoints.add_image_decleration("light.lights[0].rsm_flux", shadow_color_view)
            .add_image_decleration("light.lights[0].rsm_octa_normal", shadow_normal_view)
            .add_sampler_decleration("light.lights[0].shadowmap_sampler", shadow_sampler)
            .add_buffer_decleration("light.light_count", &spot_light_count, sizeof(spot_light_count))
            .add_image_decleration("gbuffer_albedo", gbuffer_albedo_view)
            .add_image_decleration("gbuffer_octa_normal", gbuffer_normal_view)
            .add_image_decleration("gbuffer_rma", gbuffer_rma_view)
            .add_image_decleration("gbuffer_depth", gbuffer_depth_view)
            .add_sampler_decleration("gbuffer_sampler", gbuffer_sampler)
            .add_image_decleration("direct_lighting_output", direct_lighting_view);
        return bindpoints;
    };
    RenderBindpointDeclarationsBuilder direct_bindpoints = make_direct_bindpoints(shadow_moments_sampled_view, false);

    ComputePipelineCompilerVKContext compute_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
    };
    auto direct_compiled = ComputePipelineCompilerVK(compute_context)
                               .compile("direct_lighting_shader", direct_lighting_shader_resource, direct_bindpoints.build());
    direct_lighting_compute_pipeline = direct_compiled.compute_pipeline;
    direct_lighting_descriptor_set_bindings = std::move(direct_compiled.descriptor_set_bindings);
    RenderBindpointDeclarationsBuilder direct_summed_area_table_bindpoints =
        make_direct_bindpoints(shadow_moments_attachment_view, true);
    auto direct_summed_area_table_compiled = ComputePipelineCompilerVK(compute_context)
                                                 .compile("direct_lighting_summed_area_table_shader",
                                                     direct_lighting_summed_area_table_shader_resource,
                                                     direct_summed_area_table_bindpoints.build());
    direct_lighting_summed_area_table_compute_pipeline = direct_summed_area_table_compiled.compute_pipeline;
    direct_lighting_summed_area_table_descriptor_set_bindings = std::move(direct_summed_area_table_compiled.descriptor_set_bindings);

    RenderBindpointDeclarationsBuilder indirect_bindpoints;
    indirect_bindpoints.add_buffer_decleration("camera_info", &camera_info, sizeof(camera_info))
        .add_buffer_decleration("light.lights[0].info", &spot_light_info, sizeof(spot_light_info))
        .add_image_decleration("light.lights[0].shadowmap", shadow_moments_attachment_view)
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
    present_post_processing_descriptor_set_bindings = std::move(present_compiled.descriptor_set_bindings);
}

void ApplicationVulkanDeferredPBRTest::write_dynamic_camera_descriptors(std::span<RTRunStaticSetVK* const> p_gbuffer_tasks) {
    if (p_gbuffer_tasks.empty()) {
        throw std::runtime_error("Failed to setup deferred test: compiled pipelines produced no descriptor sets.");
    }
    gbuffer_camera_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(CameraInfoVKUBO));
    CameraInfoVKUBO gbuffer_camera_info = make_camera_info_ubo(camera_component);
    gbuffer_camera_bindings.clear();
    gbuffer_camera_bindings.reserve(p_gbuffer_tasks.size());
    for (RTRunStaticSetVK* gbuffer_task : p_gbuffer_tasks) {
        if (gbuffer_task == nullptr) {
            throw std::runtime_error("Failed to setup deferred test: a G-buffer task is null.");
        }
        auto [gbuffer_set, gbuffer_binding] = gbuffer_task->get_descriptor_binding("camera_info");
        gbuffer_camera_bindings.push_back(TaskUniformBinding{.task = gbuffer_task, .set_index = gbuffer_set, .binding = gbuffer_binding});
    }

    composition_camera_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(CompositionCameraInfo));
    CompositionCameraInfo composition_camera_info = make_composition_camera_info();
    composition_camera_bindings.clear();
    auto [direct_set, direct_binding] = direct_lighting_descriptor_set_bindings.at("camera_info");
    composition_camera_bindings.push_back(ComputeUniformBinding{
        .pipeline = direct_lighting_compute_pipeline.get(),
        .set_index = direct_set,
        .binding = direct_binding,
    });
    auto [direct_summed_area_table_set, direct_summed_area_table_binding] =
        direct_lighting_summed_area_table_descriptor_set_bindings.at("camera_info");
    composition_camera_bindings.push_back(ComputeUniformBinding{
        .pipeline = direct_lighting_summed_area_table_compute_pipeline.get(),
        .set_index = direct_summed_area_table_set,
        .binding = direct_summed_area_table_binding,
    });
    auto [indirect_set, indirect_binding] = indirect_lighting_descriptor_set_bindings.at("camera_info");
    composition_camera_bindings.push_back(ComputeUniformBinding{
        .pipeline = indirect_lighting_compute_pipeline.get(),
        .set_index = indirect_set,
        .binding = indirect_binding,
    });

    for (uint32_t frame_slot = 0; frame_slot < gbuffer_camera_info_ubos->get_context_count(); ++frame_slot) {
        get_frame_uniform_buffer(gbuffer_camera_info_ubos, frame_slot, "deferred gbuffer camera uniform buffer")
            ->update_buffer(&gbuffer_camera_info, sizeof(gbuffer_camera_info));
    }
    for (uint32_t frame_slot = 0; frame_slot < composition_camera_info_ubos->get_context_count(); ++frame_slot) {
        get_frame_uniform_buffer(composition_camera_info_ubos, frame_slot, "deferred composition camera uniform buffer")
            ->update_buffer(&composition_camera_info, sizeof(composition_camera_info));
    }

    write_task_uniform_bindings(gbuffer_camera_bindings, gbuffer_camera_info_ubos, sizeof(CameraInfoVKUBO));
    write_compute_uniform_bindings(composition_camera_bindings, composition_camera_info_ubos, sizeof(CompositionCameraInfo));
}

void ApplicationVulkanDeferredPBRTest::write_dynamic_light_descriptors(std::span<RTRunStaticSetVK* const> p_shadow_tasks) {
    if (p_shadow_tasks.empty()) {
        throw std::runtime_error("Failed to setup deferred test: compiled light pipelines produced no descriptor sets.");
    }
    spot_light_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(SpotLightInfoVKUBO));
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    shadow_light_bindings.clear();

    for (RTRunStaticSetVK* shadow_task : p_shadow_tasks) {
        if (shadow_task == nullptr) {
            throw std::runtime_error("Failed to setup deferred test: a shadow task is null.");
        }
        auto [set, binding] = shadow_task->get_descriptor_binding("light_camera_info");
        shadow_light_bindings.push_back(TaskUniformBinding{.task = shadow_task, .set_index = set, .binding = binding});
    }

    spot_light_bindings.clear();
    auto [direct_set, direct_binding] = direct_lighting_descriptor_set_bindings.at("light.lights.info");
    spot_light_bindings.push_back(ComputeUniformBinding{
        .pipeline = direct_lighting_compute_pipeline.get(),
        .set_index = direct_set,
        .binding = direct_binding,
        .dst_array_element = 0,
    });
    auto [direct_summed_area_table_set, direct_summed_area_table_binding] =
        direct_lighting_summed_area_table_descriptor_set_bindings.at("light.lights.info");
    spot_light_bindings.push_back(ComputeUniformBinding{
        .pipeline = direct_lighting_summed_area_table_compute_pipeline.get(),
        .set_index = direct_summed_area_table_set,
        .binding = direct_summed_area_table_binding,
        .dst_array_element = 0,
    });
    auto [indirect_set, indirect_binding] = indirect_lighting_descriptor_set_bindings.at("light.lights.info");
    spot_light_bindings.push_back(ComputeUniformBinding{
        .pipeline = indirect_lighting_compute_pipeline.get(),
        .set_index = indirect_set,
        .binding = indirect_binding,
        .dst_array_element = 0,
    });

    for (uint32_t frame_slot = 0; frame_slot < spot_light_info_ubos->get_context_count(); ++frame_slot) {
        get_frame_uniform_buffer(spot_light_info_ubos, frame_slot, "deferred spot light uniform buffer")
            ->update_buffer(&spot_light_info, sizeof(spot_light_info));
    }

    write_task_uniform_bindings(shadow_light_bindings, spot_light_info_ubos, sizeof(SpotLightInfoVKUBO));
    write_compute_uniform_bindings(spot_light_bindings, spot_light_info_ubos, sizeof(SpotLightInfoVKUBO));
}

void ApplicationVulkanDeferredPBRTest::register_deferred_render_graph(std::string_view p_graph_id,
    bool p_use_summed_area_table,
    Unique<RTRunStaticSetVK>& p_shadow_opaque_task,
    Unique<RTRunStaticSetVK>& p_shadow_masked_task,
    Unique<RTRunStaticSetVK>& p_gbuffer_opaque_task,
    Unique<RTRunStaticSetVK>& p_gbuffer_masked_task) {
    RendGraphDescVK pipeline_desc(render_allocator);
    add_render_graph_resources(pipeline_desc);
    pipeline_desc.add_task(RT_RUN_DEFERRED_SHADOW_OPAQUE, std::move(p_shadow_opaque_task));
    RendTaskDescVK shadow_masked_task_desc(render_allocator, RT_RUN_DEFERRED_SHADOW_MASKED, std::move(p_shadow_masked_task));
    shadow_masked_task_desc.after(RT_RUN_DEFERRED_SHADOW_OPAQUE);
    pipeline_desc.add_task(std::move(shadow_masked_task_desc));
    if (p_use_summed_area_table) {
        pipeline_desc.add_task(create_shadow_moments_summed_area_table_horizontal_task_desc())
            .add_task(create_shadow_moments_summed_area_table_vertical_task_desc());
    } else {
        pipeline_desc.add_task(create_shadow_mipmap_task_desc());
    }
    RendTaskDescVK gbuffer_opaque_task_desc(render_allocator, RT_RUN_DEFERRED_GBUFFER, std::move(p_gbuffer_opaque_task));
    gbuffer_opaque_task_desc.after(RT_RUN_DEFERRED_SHADOW_MASKED);
    pipeline_desc.add_task(std::move(gbuffer_opaque_task_desc));
    RendTaskDescVK gbuffer_masked_task_desc(render_allocator, RT_RUN_DEFERRED_GBUFFER_MASKED, std::move(p_gbuffer_masked_task));
    gbuffer_masked_task_desc.after(RT_RUN_DEFERRED_GBUFFER);
    pipeline_desc.add_task(std::move(gbuffer_masked_task_desc))
        .add_task(create_direct_lighting_task_desc(p_use_summed_area_table))
        .add_task(create_indirect_lighting_task_desc())
        .add_task(create_post_processing_task_desc())
        .add_task(create_present_post_processing_task_desc());
    renderer_vk->register_render_graph(p_graph_id, std::move(pipeline_desc));
}

void ApplicationVulkanDeferredPBRTest::add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const {
    p_pipeline_desc
        .add_resource(RendResVK{
            .res_id = std::string(REND_RES_SWAP_CHAIN_IMAGE), .res_type_id = TypeUUID<ROImageViewVK>::UUID, .container = MEM_NULL})
        .add_resource(REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_COMPUTE_PIPELINE,
            shadow_moments_summed_area_table_horizontal_compute_pipeline)
        .add_resource(REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_VERTICAL_COMPUTE_PIPELINE,
            shadow_moments_summed_area_table_vertical_compute_pipeline)
        .add_resource(REND_RES_TEST_DIRECT_LIGHTING_COMPUTE_PIPELINE, direct_lighting_compute_pipeline)
        .add_resource(REND_RES_TEST_DIRECT_LIGHTING_SUMMED_AREA_TABLE_COMPUTE_PIPELINE, direct_lighting_summed_area_table_compute_pipeline)
        .add_resource(REND_RES_TEST_INDIRECT_LIGHTING_COMPUTE_PIPELINE, indirect_lighting_compute_pipeline)
        .add_resource(REND_RES_TEST_POST_PROCESSING_COMPUTE_PIPELINE, post_processing_compute_pipeline)
        .add_resource(REND_RES_TEST_PRESENT_POST_PROCESSING_GRAPHICS_PIPELINE, present_post_processing_graphics_pipeline)
        .add_resource(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, shadow_color_view)
        .add_resource(REND_RES_TEST_RSM_NORMAL_IMAGE_VIEW, shadow_normal_view)
        .add_resource(REND_RES_TEST_SHADOW_MOMENTS_IMAGE_VIEW, shadow_moments_attachment_view)
        .add_resource(REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE_VIEW, shadow_moments_summed_area_table_horizontal_view)
        .add_resource(REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW, shadow_moments_summed_area_table_view)
        .add_resource(REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW, shadow_depth_view)
        .add_resource(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW, gbuffer_albedo_view)
        .add_resource(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW, gbuffer_normal_view)
        .add_resource(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW, gbuffer_rma_view)
        .add_resource(REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW, gbuffer_depth_view)
        .add_resource(REND_RES_TEST_DIRECT_LIGHTING_IMAGE_VIEW, direct_lighting_view)
        .add_resource(REND_RES_TEST_INDIRECT_LIGHTING_IMAGE_VIEW, indirect_lighting_view)
        .add_resource(REND_RES_TEST_POST_PROCESSED_IMAGE_VIEW, post_processed_view);
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_shadow_moments_summed_area_table_horizontal_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL;
    task_desc.after(RT_RUN_DEFERRED_SHADOW_MASKED)
        .read(REND_RES_TEST_SHADOW_MOMENTS_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_shadow_extent = shadow_extent]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_shadow_extent.height + 63U) / 64U,
            .height = 1,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator,
            task_base_render_data,
            REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_COMPUTE_PIPELINE,
            dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_shadow_mipmap_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_GENERATE_SHADOW_MIPMAPS;
    task_desc.after(RT_RUN_DEFERRED_SHADOW_MASKED);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_shadow_extent = shadow_extent,
                           task_shadow_mipmap_level_count = shadow_mipmap_level_count]() -> Unique<RenderTaskVK> {
        return make_unique<RTGenerateMipmapsVK>(task_render_allocator,
            task_base_render_data,
            REND_RES_TEST_SHADOW_MOMENTS_IMAGE_VIEW,
            task_shadow_extent,
            task_shadow_mipmap_level_count,
            SHADOW_MOMENTS_FORMAT);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_shadow_moments_summed_area_table_vertical_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE_VERTICAL;
    task_desc.after(RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL)
        .read(REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_shadow_extent = shadow_extent]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_shadow_extent.width + 63U) / 64U,
            .height = 1,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator,
            task_base_render_data,
            REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_VERTICAL_COMPUTE_PIPELINE,
            dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_direct_lighting_task_desc(bool p_use_summed_area_table) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_DIRECT_LIGHTING;
    task_desc.after(RT_RUN_DEFERRED_GBUFFER_MASKED)
        .after(p_use_summed_area_table ? RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE_VERTICAL : RT_GENERATE_SHADOW_MIPMAPS)
        .read(REND_RES_TEST_SHADOW_MOMENTS_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_RSM_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(REND_RES_TEST_DIRECT_LIGHTING_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    if (p_use_summed_area_table) {
        task_desc.read(REND_RES_TEST_SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE);
    }
    std::string pipeline_id = std::string(p_use_summed_area_table ? REND_RES_TEST_DIRECT_LIGHTING_SUMMED_AREA_TABLE_COMPUTE_PIPELINE
                                                                  : REND_RES_TEST_DIRECT_LIGHTING_COMPUTE_PIPELINE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline_id = std::move(pipeline_id)]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline_id, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_indirect_lighting_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_INDIRECT_LIGHTING;
    task_desc.after(RT_RUN_DEFERRED_GBUFFER_MASKED)
        .read(REND_RES_TEST_SHADOW_MOMENTS_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_RSM_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(REND_RES_TEST_INDIRECT_LIGHTING_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator, task_base_render_data = base_render_data]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(
            task_render_allocator, task_base_render_data, REND_RES_TEST_INDIRECT_LIGHTING_COMPUTE_PIPELINE, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_post_processing_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_POST_PROCESSING;
    task_desc.after(RT_RUN_DIRECT_LIGHTING)
        .after(RT_RUN_INDIRECT_LIGHTING)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_DIRECT_LIGHTING_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(REND_RES_TEST_INDIRECT_LIGHTING_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(REND_RES_TEST_POST_PROCESSED_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator, task_base_render_data = base_render_data]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(
            task_render_allocator, task_base_render_data, REND_RES_TEST_POST_PROCESSING_COMPUTE_PIPELINE, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDeferredPBRTest::create_present_post_processing_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_PRESENT_POST_PROCESSING;
    task_desc.after(RT_RUN_POST_PROCESSING).read(REND_RES_TEST_POST_PROCESSED_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator, task_base_render_data = base_render_data]() -> Unique<RenderTaskVK> {
        return make_unique<RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
            task_base_render_data->swap_chain_extent,
            REND_RES_SWAP_CHAIN_IMAGE,
            REND_RES_TEST_PRESENT_POST_PROCESSING_GRAPHICS_PIPELINE,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VertexInputDescriptionVK{},
            0);
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
