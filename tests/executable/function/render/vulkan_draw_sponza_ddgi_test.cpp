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
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/ray_tracing_plugin_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/roc_per_frame_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipelines_sequential_vk.hh"
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
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
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

Ref<ROCPerFrameVK> create_per_frame_uniform_buffers(
    RenderAllocator* p_allocator, const BaseRenderDataVK* p_render_data, VkDeviceSize p_buffer_size) {
    return make_ref<ROCPerFrameVK>(p_allocator,
        p_render_data,
        ROCPerFrameVK::Factory{[p_render_data, p_buffer_size](uint32_t /*p_frame_slot*/) -> Ref<IRenderObjectVK> {
            return ROBufferVK::Builder()
                .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                .set_domain(DeviceDomain::HOST_DEVICE)
                .set_size(p_buffer_size)
                .create_ref(p_render_data);
        }});
}

} // namespace

constexpr VkFormat GBUFFER_ALBEDO_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_NORMAL_FORMAT = VK_FORMAT_R16G16_SNORM;
constexpr VkFormat GBUFFER_RMA_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOWMAP_COLOR_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat LIGHTING_OUTPUT_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat DDGI_RAY_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat DDGI_DISTANCE_FORMAT = VK_FORMAT_R16G16_SFLOAT;

class ApplicationVulkanDdgiPBRTest : public ApplicationGame {
public:
    /**
     * @brief Request ray-query support for probe tracing.
     */
    ApplicationVulkanDdgiPBRTest() : ApplicationGame(RendererCapabilities{.ray_tracing = true, .ray_query = true}) {
    }

    virtual void logic_update(double p_delta) override;
    virtual void render_update(double p_delta) override;

    void setup_draw_task();

private:
    static constexpr std::string_view DDGI_RENDER_GRAPH = "ddgi_pipeline";
    static constexpr std::string_view GBUFFER_ALBEDO_IMAGE_VIEW = "gbuffer_albedo";
    static constexpr std::string_view GBUFFER_NORMAL_IMAGE_VIEW = "gbuffer_normal";
    static constexpr std::string_view GBUFFER_RMA_IMAGE_VIEW = "gbuffer_rma";
    static constexpr std::string_view GBUFFER_DEPTH_IMAGE_VIEW = "gbuffer_depth";
    static constexpr std::string_view SHADOWMAP_COLOR_IMAGE_VIEW = "shadowmap_color";
    static constexpr std::string_view SHADOWMAP_DEPTH_IMAGE_VIEW = "shadowmap_depth";
    static constexpr std::string_view STATIC_OPAQUE_TLAS_RESOURCE = "static_opaque_tlas";
    static constexpr std::string_view DDGI_PROBE_RAYS = "ddgi_probe_rays";
    static constexpr std::string_view DDGI_IRRADIANCE_ATLAS = "ddgi_irradiance_atlas";
    static constexpr std::string_view DDGI_DISTANCE_ATLAS = "ddgi_distance_atlas";
    static constexpr std::string_view DIRECT_LIGHTING_IMAGE_VIEW = "direct_lighting";
    static constexpr std::string_view INDIRECT_LIGHTING_IMAGE_VIEW = "indirect_lighting";
    static constexpr std::string_view RESULT_IMAGE_VIEW = "result_image";
    static constexpr std::string_view BLOOM_HORIZONTAL_IMAGE_VIEW = "bloom_horizontal";
    static constexpr std::string_view BLOOM_IMAGE_VIEW = "bloom";
    static constexpr std::string_view RT_RUN_DDGI_GBUFFER = "ddgi_run_gbuffer";
    static constexpr std::string_view RT_RUN_DDGI_SHADOW_OPAQUE = "ddgi_run_shadow_opaque";
    static constexpr std::string_view RT_TRACE_DDGI_PROBES = "ddgi_trace_probes";
    static constexpr std::string_view RT_UPDATE_DDGI_PROBES = "ddgi_update_probes";
    static constexpr std::string_view RT_RUN_DIRECT_LIGHTING = "run_direct_lighting";
    static constexpr std::string_view RT_RUN_INDIRECT_LIGHTING = "run_indirect_lighting";
    static constexpr std::string_view RT_COMBINE_LIGHTING = "combine_lighting";
    static constexpr std::string_view RT_BLOOM = "bloom";
    static constexpr std::string_view RT_RUN_PRESENT_RESULT_IMAGE = "present_result_image";
    static constexpr std::string_view RT_RUN_LIGHT_BILLBOARD = "run_light_billboard";
    static constexpr uint32_t DDGI_RAYS_PER_PROBE = 8;
    static constexpr uint32_t DDGI_IRRADIANCE_RESOLUTION = 8;
    static constexpr uint32_t DDGI_DISTANCE_RESOLUTION = 16;

    struct CompositionCameraInfo {
        glm::mat4 inverse_view_projection;
        glm::vec4 camera_world_position;
    };

    struct DDGIConfig {
        glm::uvec4 grid_counts_and_probe_count = glm::uvec4(0U);
        glm::vec4 cell_spacing_and_max_ray_distance = glm::vec4(0.0F);
        glm::vec4 grid_origin_and_normal_bias = glm::vec4(0.0F);
        glm::vec4 probe_volume_extent_and_view_bias = glm::vec4(0.0F);
        glm::vec4 indirect_scale_and_hysteresis = glm::vec4(0.0F);
    };

    struct DDGITimeInfo {
        uint32_t frame_index = 0;
        uint32_t ray_rotation_seed = 0;
        float delta_seconds = 0.0F;
        float elapsed_seconds = 0.0F;
    };

    struct TaskUniformBinding {
        RTRunStaticSetVK* task = nullptr;
        std::string field_path;
    };

    struct ComputeUniformBinding {
        ROComputePipelineVK* pipeline = nullptr;
        std::string field_path;
    };

    void control(double p_delta);
    void update_descriptors(double p_delta);
    void write_task_uniform_bindings(std::span<const TaskUniformBinding> p_bindings, Ref<ROCPerFrameVK> p_buffer_container);
    void write_compute_uniform_bindings(std::span<const ComputeUniformBinding> p_bindings, Ref<ROCPerFrameVK> p_buffer_container);

    void setup_render_context();
    void load_resources();
    void setup_static_scene_resources();
    void setup_spot_light();
    void create_shadow_map_resources();
    void create_gbuffer_resources();
    void create_lighting_output_resources();
    void create_probe_resources();
    void setup_light_billboard_overlay_resources();
    Ref<ROBufferVK> create_initialized_uniform_buffer(const void* p_data, size_t p_size) const;
    ComputePipelineCompilerVKContext make_compute_pipeline_context() const;
    GraphicsPipelineCompilerVKContext make_present_graphics_pipeline_context() const;
    RenderBindpointDeclarations build_direct_lighting_bindpoints(Ref<ROBufferVK> p_camera_buffer,
        Ref<ROBufferVK> p_light_info_buffer,
        Ref<ROBufferVK> p_light_count_buffer,
        Ref<ROImageViewVK> p_shadow_depth_view) const;
    RenderBindpointDeclarations build_indirect_lighting_bindpoints(
        Ref<ROBufferVK> p_camera_buffer, Ref<ROBufferVK> p_light_info_buffer, Ref<ROBufferVK> p_light_count_buffer) const;
    RenderBindpointDeclarations build_probe_trace_bindpoints(Ref<ROBufferVK> p_light_info_buffer) const;
    RenderBindpointDeclarations build_probe_update_bindpoints() const;
    RenderBindpointDeclarations build_combine_lighting_bindpoints() const;
    RenderBindpointDeclarations build_bloom_horizontal_bindpoints() const;
    RenderBindpointDeclarations build_bloom_vertical_bindpoints() const;
    RenderBindpointDeclarations build_present_post_processing_bindpoints() const;
    Unique<RTRunStaticSetVK> compile_shadow_task(
        RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_pipeline, bool p_load_existing);
    Unique<RTRunStaticSetVK> compile_gbuffer_task(
        RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_pipeline, bool p_load_existing);
    void compile_lighting_pipelines();
    void write_dynamic_camera_descriptors(std::span<RTRunStaticSetVK* const> p_gbuffer_tasks);
    void write_dynamic_light_descriptors(std::span<RTRunStaticSetVK* const> p_shadow_tasks);
    void write_dynamic_ddgi_descriptors();
    void register_ddgi_render_graph(Unique<RTRunStaticSetVK>& p_shadow_opaque_task, Unique<RTRunStaticSetVK>& p_gbuffer_opaque_task);
    void add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const;
    RendTaskDescVK create_direct_lighting_task_desc() const;
    RendTaskDescVK create_indirect_lighting_task_desc() const;
    RendTaskDescVK create_probe_trace_task_desc() const;
    RendTaskDescVK create_probe_update_task_desc() const;
    RendTaskDescVK create_combine_lighting_task_desc() const;
    RendTaskDescVK create_bloom_task_desc() const;
    RendTaskDescVK create_present_result_image_task_desc() const;
    RendTaskDescVK create_light_billboard_task_desc() const;
    CompositionCameraInfo make_composition_camera_info() const;
    DDGIConfig make_ddgi_config() const;
    DDGITimeInfo make_ddgi_time_info(double p_delta_seconds) const;

    RendererVK* renderer_vk = nullptr;
    const BaseRenderDataVK* base_render_data = nullptr;
    RenderAllocator* render_allocator = nullptr;
    RefRes<RUStaticOpaqueSet> sponza_opaque_set;
    RefRes<RUGraphicsPipeline> shadow_opaque_pipeline_resource;
    RefRes<RUGraphicsPipeline> gbuffer_opaque_pipeline_resource;
    RefRes<RUShader> direct_lighting_pcf_shader_resource;
    RefRes<RUShader> indirect_lighting_ddgi_shader_resource;
    RefRes<RUShader> ddgi_trace_probes_shader_resource;
    RefRes<RUShader> ddgi_update_probes_shader_resource;
    RefRes<RUShader> combine_lighting_shader_resource;
    RefRes<RUShader> bloom_horizontal_shader_resource;
    RefRes<RUShader> bloom_vertical_shader_resource;
    RefRes<RUGraphicsPipeline> present_post_processing_pipeline_resource;

    Ref<ROComputePipelineVK> direct_lighting_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> indirect_lighting_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> ddgi_trace_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> ddgi_update_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> combine_lighting_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> bloom_horizontal_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> bloom_vertical_compute_pipeline = MEM_NULL;
    Ref<ROGraphicsPipelineVK> present_post_processing_graphics_pipeline = MEM_NULL;
    Ref<ROGraphicsPipelineVK> light_billboard_pipeline = MEM_NULL;
    Ref<ROImageVK> gbuffer_albedo_image;
    Ref<ROImageVK> gbuffer_normal_image;
    Ref<ROImageVK> gbuffer_rma_image;
    Ref<ROImageVK> gbuffer_depth_image;
    Ref<ROImageVK> direct_lighting_image;
    Ref<ROImageVK> indirect_lighting_image;
    Ref<ROImageVK> ddgi_probe_rays_image;
    Ref<ROImageVK> ddgi_irradiance_image;
    Ref<ROImageVK> ddgi_distance_image;
    Ref<ROImageVK> result_image;
    Ref<ROImageVK> bloom_horizontal_image;
    Ref<ROImageVK> bloom_image;
    Ref<ROImageViewVK> gbuffer_albedo_view;
    Ref<ROImageViewVK> gbuffer_normal_view;
    Ref<ROImageViewVK> gbuffer_rma_view;
    Ref<ROImageViewVK> gbuffer_depth_view;
    Ref<ROImageViewVK> direct_lighting_view;
    Ref<ROImageViewVK> indirect_lighting_view;
    Ref<ROImageViewVK> ddgi_probe_rays_view;
    Ref<ROImageViewVK> ddgi_irradiance_view;
    Ref<ROImageViewVK> ddgi_distance_view;
    Ref<ROImageViewVK> result_image_view;
    Ref<ROImageViewVK> bloom_horizontal_image_view;
    Ref<ROImageViewVK> bloom_image_view;
    Ref<ROImageVK> shadow_depth_image;
    Ref<ROImageVK> shadow_color_image;
    Ref<ROImageViewVK> shadow_depth_view;
    Ref<ROImageViewVK> shadow_color_view;
    Ref<ROSamplerVK> shadow_sampler;
    Ref<ROSamplerVK> gbuffer_sampler;
    Ref<ROCPerFrameVK> gbuffer_camera_info_ubos;
    Ref<ROCPerFrameVK> composition_camera_info_ubos;
    Ref<ROCPerFrameVK> spot_light_info_ubos;
    Ref<ROBufferVK> ddgi_config_ubo;
    Ref<ROCPerFrameVK> ddgi_time_info_ubos;
    Ref<ROBufferVK> light_billboard_ubo;
    Ref<ROSamplerVK> light_billboard_sampler;
    std::vector<TaskUniformBinding> gbuffer_camera_bindings;
    std::vector<TaskUniformBinding> shadow_light_bindings;
    std::vector<ComputeUniformBinding> composition_camera_bindings;
    std::vector<ComputeUniformBinding> spot_light_bindings;
    ComponentSpotLight spot_light_component{};
    float spot_light_yaw = 0.0F;
    float spot_light_pitch = 0.0F;
    VkExtent2D shadow_extent{};
    ComponentCamera camera_component{};
    glm::vec3 camera_position = TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = default_test_camera_yaw();
    float camera_pitch = default_test_camera_pitch();
    double ddgi_elapsed_seconds = 0.0;
    uint32_t ddgi_frame_index = 0;
};

void ApplicationVulkanDdgiPBRTest::logic_update(double p_delta) {
    control(p_delta);
    ddgi_elapsed_seconds += p_delta;
    ++ddgi_frame_index;
    update_descriptors(p_delta);
}

void ApplicationVulkanDdgiPBRTest::control(double p_delta) { // NOLINT
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

void ApplicationVulkanDdgiPBRTest::update_descriptors(double p_delta) {
    camera_component.view = test_camera_view(camera_position, camera_yaw, camera_pitch);
    CameraInfoVKUBO gbuffer_camera_info = make_camera_info_ubo(camera_component);
    CompositionCameraInfo composition_camera_info = make_composition_camera_info();
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    DDGITimeInfo ddgi_time_info = make_ddgi_time_info(p_delta);
    BillboardInfoVKUBO light_billboard_info = make_light_billboard_info(spot_light_component);
    add_render_job([this, gbuffer_camera_info, composition_camera_info, spot_light_info, ddgi_time_info, light_billboard_info]() {
        if (gbuffer_camera_info_ubos == MEM_NULL || composition_camera_info_ubos == MEM_NULL ||
            spot_light_info_ubos == MEM_NULL || ddgi_time_info_ubos == MEM_NULL || light_billboard_ubo == MEM_NULL) {
            throw std::runtime_error("Failed to update deferred descriptors: per-frame uniform buffers are not initialized.");
        }
        gbuffer_camera_info_ubos->publish_buffer_update(&gbuffer_camera_info, 0, sizeof(gbuffer_camera_info));
        composition_camera_info_ubos->publish_buffer_update(&composition_camera_info, 0, sizeof(composition_camera_info));
        spot_light_info_ubos->publish_buffer_update(&spot_light_info, 0, sizeof(spot_light_info));
        ddgi_time_info_ubos->publish_buffer_update(&ddgi_time_info, 0, sizeof(ddgi_time_info));
        light_billboard_ubo->update_buffer(&light_billboard_info, sizeof(light_billboard_info));
    });
}

void ApplicationVulkanDdgiPBRTest::write_task_uniform_bindings(
    std::span<const TaskUniformBinding> p_bindings, Ref<ROCPerFrameVK> p_buffer_container) {
    if (p_buffer_container == MEM_NULL) {
        throw std::runtime_error("Failed to write deferred task uniform bindings: buffer container is null.");
    }
    for (const TaskUniformBinding& binding : p_bindings) {
        if (binding.task == nullptr) {
            throw std::runtime_error("Failed to write deferred task uniform binding: task is null.");
        }
        binding.task->set_descriptor_binding(binding.field_path, p_buffer_container);
    }
}

void ApplicationVulkanDdgiPBRTest::write_compute_uniform_bindings(
    std::span<const ComputeUniformBinding> p_bindings, Ref<ROCPerFrameVK> p_buffer_container) {
    if (p_buffer_container == MEM_NULL) {
        throw std::runtime_error("Failed to write deferred compute uniform bindings: buffer container is null.");
    }
    for (const ComputeUniformBinding& binding : p_bindings) {
        if (binding.pipeline == nullptr) {
            throw std::runtime_error("Failed to write deferred compute uniform binding: pipeline is null.");
        }
        binding.pipeline->set_descriptor_binding(binding.field_path, p_buffer_container);
    }
}

void ApplicationVulkanDdgiPBRTest::render_update(double /* p_delta */) {
    WBE::on_exec_test_render(*renderer_vk, base_render_data->current_frame);
}

void ApplicationVulkanDdgiPBRTest::setup_draw_task() {
    setup_render_context();
    load_resources();
    setup_static_scene_resources();
    setup_spot_light();
    create_shadow_map_resources();
    create_gbuffer_resources();
    create_lighting_output_resources();
    create_probe_resources();
    compile_lighting_pipelines();
    auto shadow_opaque_task = compile_shadow_task(sponza_opaque_set, shadow_opaque_pipeline_resource, false);
    auto gbuffer_opaque_task = compile_gbuffer_task(sponza_opaque_set, gbuffer_opaque_pipeline_resource, false);
    std::array<RTRunStaticSetVK*, 1> gbuffer_tasks{gbuffer_opaque_task.get()};
    std::array<RTRunStaticSetVK*, 1> shadow_tasks{shadow_opaque_task.get()};
    write_dynamic_camera_descriptors(gbuffer_tasks);
    write_dynamic_light_descriptors(shadow_tasks);
    write_dynamic_ddgi_descriptors();
    setup_light_billboard_overlay_resources();
    register_ddgi_render_graph(shadow_opaque_task, gbuffer_opaque_task);
    renderer_vk->activate_render_graph(DDGI_RENDER_GRAPH);
}

Unique<RTRunStaticSetVK> ApplicationVulkanDdgiPBRTest::compile_gbuffer_task(
    RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_pipeline, bool p_load_existing) {
    RTRunStaticSetVK::Config task_config;
    VkClearValue color_clear{};
    VkClearValue depth_clear{.depthStencil = {.depth = 0.0F, .stencil = 0}};
    task_config.add_color_attachment(GBUFFER_ALBEDO_IMAGE_VIEW, GBUFFER_ALBEDO_FORMAT)
        .add_color_attachment(GBUFFER_NORMAL_IMAGE_VIEW, GBUFFER_NORMAL_FORMAT)
        .add_color_attachment(GBUFFER_RMA_IMAGE_VIEW, GBUFFER_RMA_FORMAT)
        .set_depth_attachment(GBUFFER_DEPTH_IMAGE_VIEW, GBUFFER_DEPTH_FORMAT);
    if (p_load_existing) {
        task_config.set_color_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD).set_depth_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    } else {
        task_config.add_clear_value(color_clear).add_clear_value(color_clear).add_clear_value(color_clear).add_clear_value(depth_clear);
    }
    return make_unique<RTRunStaticSetVK>(render_allocator, renderer_vk, p_set, p_pipeline, task_config);
}

void ApplicationVulkanDdgiPBRTest::setup_render_context() {
    if (!get_renderer_capabilities().ray_query) {
        throw std::runtime_error("DDGI probe tracing requires Vulkan ray-query support.");
    }
    renderer_vk = RendererVK::get_singleton();
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = get_renderer_allocator().get();
    camera_component = make_default_test_camera_component(base_render_data->swap_chain_aspect_ratio, 100.0F);
}

void ApplicationVulkanDdgiPBRTest::load_resources() {
    auto* resource_manager = ResourceManager::get_singleton();
    stdout_log(WBE_CHANNEL_DEBUG)->message("Loading render resources...");
    auto resource_chunk = make_ref<ResourceChunkLoadFile>(
        global_allocator(), "res_chunks/rendering_test_resources.json", resource_manager->get_allocator());
    stdout_log(WBE_CHANNEL_DEBUG)->message("Finished loading render resources.");
    resource_manager->register_chunk(resource_chunk);
    sponza_opaque_set = resource_manager->get_resource<RUStaticOpaqueSet>("intel_sponza.static_opaque_set");
    shadow_opaque_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("shadowmap_spot_static");
    gbuffer_opaque_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("deferred_gbuffer_static");
    direct_lighting_pcf_shader_resource = resource_manager->get_resource<RUShader>("direct_lighting_pcf_shader");
    indirect_lighting_ddgi_shader_resource = resource_manager->get_resource<RUShader>("indirect_lighting_ddgi_shader");
    ddgi_trace_probes_shader_resource = resource_manager->get_resource<RUShader>("ddgi_trace_probes_shader");
    ddgi_update_probes_shader_resource = resource_manager->get_resource<RUShader>("ddgi_update_probes_shader");
    combine_lighting_shader_resource = resource_manager->get_resource<RUShader>("combine_lighting_shader");
    bloom_horizontal_shader_resource = resource_manager->get_resource<RUShader>("bloom_shader_horizontal");
    bloom_vertical_shader_resource = resource_manager->get_resource<RUShader>("bloom_shader_vertical");
    present_post_processing_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("present_post_processing");
}

void ApplicationVulkanDdgiPBRTest::setup_static_scene_resources() {
    Ref<RenderDatabase> render_database = renderer_vk->get_render_database();
    render_database->set_static_opaque_set(sponza_opaque_set);
    renderer_vk->update_static_geometry_sets();
}

void ApplicationVulkanDdgiPBRTest::setup_spot_light() {
    spot_light_component = make_default_test_spot_light_component(128.0F);
    spot_light_yaw = std::atan2(spot_light_component.direction.z, spot_light_component.direction.x);
    spot_light_pitch = std::asin(std::clamp(spot_light_component.direction.y, -1.0F, 1.0F));
    shadow_extent = VkExtent2D{
        .width = spot_light_component.shadow_map_resolution,
        .height = spot_light_component.shadow_map_resolution,
    };
}

void ApplicationVulkanDdgiPBRTest::setup_light_billboard_overlay_resources() {
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

    light_billboard_ubo = ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .set_domain(DeviceDomain::HOST_DEVICE)
                              .set_size(sizeof(BillboardInfoVKUBO))
                              .create_ref(base_render_data);
    BillboardInfoVKUBO light_billboard_info = make_light_billboard_info(spot_light_component);
    light_billboard_ubo->update_buffer(&light_billboard_info, sizeof(light_billboard_info));
    light_billboard_sampler = ROSamplerVK::Builder()
                                  .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                                  .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                                  .create_ref(base_render_data);

    RenderBindpointDeclarationsBuilder bindpoint_builder;
    bindpoint_builder.add_storage_buffer_decleration("camera_info", gbuffer_camera_info_ubos)
        .add_storage_buffer_decleration("billboard_info", light_billboard_ubo)
        .add_image_decleration("texture_image", texture_resource)
        .add_sampler_decleration("texture_sampler", light_billboard_sampler);
    GraphicsPipelineCompilerVKContext compiler_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
        .depth_attachment_format = GBUFFER_DEPTH_FORMAT,
    };
    auto compiled = GraphicsPipelineCompilerVK(compiler_context)
                        .compile("deferred_light_billboard_pipeline", pipeline_resource, bindpoint_builder.build());
    light_billboard_pipeline = compiled.graphics_pipeline;
}

void ApplicationVulkanDdgiPBRTest::create_shadow_map_resources() {
    shadow_depth_image = ROImageVK::Builder().shadow_map_depth(spot_light_component).create_ref(base_render_data);
    shadow_color_image = ROImageVK::Builder()
                             .set_format(SHADOWMAP_COLOR_FORMAT)
                             .image_2d(shadow_extent)
                             .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
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
                            .set_format(SHADOWMAP_COLOR_FORMAT)
                            .create_ref(base_render_data);
    shadow_sampler = ROSamplerVK::Builder()
                         .set_filter(VK_FILTER_NEAREST, VK_FILTER_NEAREST)
                         .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                         .create_ref(base_render_data);
}

void ApplicationVulkanDdgiPBRTest::create_gbuffer_resources() {
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

void ApplicationVulkanDdgiPBRTest::create_lighting_output_resources() {
    VkExtent2D extent = base_render_data->swap_chain_extent;
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
    result_image = ROImageVK::Builder()
                       .set_format(LIGHTING_OUTPUT_FORMAT)
                       .image_2d(extent)
                       .set_usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                       .create_ref(base_render_data);
    bloom_horizontal_image = ROImageVK::Builder()
                                 .set_format(LIGHTING_OUTPUT_FORMAT)
                                 .image_2d(extent)
                                 .set_usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                 .create_ref(base_render_data);
    bloom_image = ROImageVK::Builder()
                      .set_format(LIGHTING_OUTPUT_FORMAT)
                      .image_2d(extent)
                      .set_usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
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
    bloom_horizontal_image_view = ROImageViewVK::Builder()
                                      .set_image(bloom_horizontal_image)
                                      .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                      .set_format(LIGHTING_OUTPUT_FORMAT)
                                      .create_ref(base_render_data);
    bloom_image_view = ROImageViewVK::Builder()
                           .set_image(bloom_image)
                           .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                           .set_format(LIGHTING_OUTPUT_FORMAT)
                           .create_ref(base_render_data);
}

void ApplicationVulkanDdgiPBRTest::create_probe_resources() {
    DDGIConfig config = make_ddgi_config();
    uint32_t probe_count = config.grid_counts_and_probe_count.w;
    uint32_t probes_per_row = config.grid_counts_and_probe_count.x * config.grid_counts_and_probe_count.y;
    uint32_t probe_rows = config.grid_counts_and_probe_count.z;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ddgi_probe_rays_image = ROImageVK::Builder()
                                .set_format(DDGI_RAY_FORMAT)
                                .image_2d(VkExtent2D{DDGI_RAYS_PER_PROBE, probe_count})
                                .set_usage(usage)
                                .create_ref(base_render_data);
    ddgi_irradiance_image = ROImageVK::Builder()
                                .set_format(LIGHTING_OUTPUT_FORMAT)
                                .image_2d(VkExtent2D{probes_per_row * DDGI_IRRADIANCE_RESOLUTION, probe_rows * DDGI_IRRADIANCE_RESOLUTION})
                                .set_usage(usage)
                                .create_ref(base_render_data);
    ddgi_distance_image = ROImageVK::Builder()
                              .set_format(DDGI_DISTANCE_FORMAT)
                              .image_2d(VkExtent2D{probes_per_row * DDGI_DISTANCE_RESOLUTION, probe_rows * DDGI_DISTANCE_RESOLUTION})
                              .set_usage(usage)
                              .create_ref(base_render_data);
    ddgi_probe_rays_view = ROImageViewVK::Builder()
                               .set_image(ddgi_probe_rays_image)
                               .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                               .set_format(DDGI_RAY_FORMAT)
                               .create_ref(base_render_data);
    ddgi_irradiance_view = ROImageViewVK::Builder()
                               .set_image(ddgi_irradiance_image)
                               .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                               .set_format(LIGHTING_OUTPUT_FORMAT)
                               .create_ref(base_render_data);
    ddgi_distance_view = ROImageViewVK::Builder()
                             .set_image(ddgi_distance_image)
                             .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                             .set_format(DDGI_DISTANCE_FORMAT)
                             .create_ref(base_render_data);
}

Unique<RTRunStaticSetVK> ApplicationVulkanDdgiPBRTest::compile_shadow_task(
    RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_pipeline, bool p_load_existing) {
    RTRunStaticSetVK::Config task_config;
    VkClearValue shadow_color_clear{};
    VkClearValue shadow_depth_clear{.depthStencil = {.depth = 0.0F, .stencil = 0}};
    task_config.set_render_extent(shadow_extent)
        .add_color_attachment(SHADOWMAP_COLOR_IMAGE_VIEW, SHADOWMAP_COLOR_FORMAT)
        .set_depth_attachment(SHADOWMAP_DEPTH_IMAGE_VIEW, SHADOWMAP_DEPTH_FORMAT);
    if (p_load_existing) {
        task_config.set_color_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD).set_depth_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    } else {
        task_config.add_clear_value(shadow_color_clear).add_clear_value(shadow_depth_clear);
    }
    auto task = make_unique<RTRunStaticSetVK>(render_allocator, renderer_vk, p_set, p_pipeline, task_config);
    return task;
}

Ref<ROBufferVK> ApplicationVulkanDdgiPBRTest::create_initialized_uniform_buffer(const void* p_data, size_t p_size) const {
    Ref<ROBufferVK> buffer = ROBufferVK::Builder()
                                 .set_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                 .set_domain(DeviceDomain::HOST_DEVICE)
                                 .set_size(p_size)
                                 .create_ref(base_render_data);
    buffer->update_buffer(p_data, p_size);
    return buffer;
}

ComputePipelineCompilerVKContext ApplicationVulkanDdgiPBRTest::make_compute_pipeline_context() const {
    return ComputePipelineCompilerVKContext{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
    };
}

GraphicsPipelineCompilerVKContext ApplicationVulkanDdgiPBRTest::make_present_graphics_pipeline_context() const {
    return GraphicsPipelineCompilerVKContext{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
    };
}

RenderBindpointDeclarations ApplicationVulkanDdgiPBRTest::build_direct_lighting_bindpoints(Ref<ROBufferVK> p_camera_buffer,
    Ref<ROBufferVK> p_light_info_buffer,
    Ref<ROBufferVK> p_light_count_buffer,
    Ref<ROImageViewVK> p_shadow_depth_view) const {
    RenderBindpointDeclarationsBuilder bindpoints;
    bindpoints.add_storage_buffer_decleration("camera_info", p_camera_buffer)
        .add_storage_buffer_decleration("light.lights[0].info", p_light_info_buffer)
        .add_image_decleration("light.lights[0].shadowmap", p_shadow_depth_view)
        .add_sampler_decleration("light.lights[0].shadowmap_sampler", shadow_sampler)
        .add_storage_buffer_decleration("light.light_count", p_light_count_buffer)
        .add_image_decleration("gbuffer_albedo", gbuffer_albedo_view)
        .add_image_decleration("gbuffer_octa_normal", gbuffer_normal_view)
        .add_image_decleration("gbuffer_rma", gbuffer_rma_view)
        .add_image_decleration("gbuffer_depth", gbuffer_depth_view)
        .add_sampler_decleration("gbuffer_sampler", gbuffer_sampler)
        .add_image_decleration("direct_lighting_output", direct_lighting_view);
    return bindpoints.build();
}

RenderBindpointDeclarations ApplicationVulkanDdgiPBRTest::build_indirect_lighting_bindpoints(
    Ref<ROBufferVK> p_camera_buffer, Ref<ROBufferVK> p_light_info_buffer, Ref<ROBufferVK> p_light_count_buffer) const {
    static_cast<void>(p_light_info_buffer);
    static_cast<void>(p_light_count_buffer);
    if (ddgi_config_ubo == MEM_NULL) {
        throw std::runtime_error("Failed to build DDGI indirect-lighting bindpoints: DDGI config buffer is null.");
    }
    if (ddgi_time_info_ubos == MEM_NULL) {
        throw std::runtime_error("Failed to build DDGI indirect-lighting bindpoints: DDGI time buffer container is null.");
    }
    RenderBindpointDeclarationsBuilder bindpoints;
    bindpoints.add_storage_buffer_decleration("camera_info", p_camera_buffer)
        .add_storage_buffer_decleration("ddgi_config", ddgi_config_ubo)
        .add_image_decleration("irradiance_atlas", ddgi_irradiance_view)
        .add_image_decleration("distance_atlas", ddgi_distance_view)
        .add_image_decleration("gbuffer_albedo", gbuffer_albedo_view)
        .add_image_decleration("gbuffer_octa_normal", gbuffer_normal_view)
        .add_image_decleration("gbuffer_rma", gbuffer_rma_view)
        .add_image_decleration("gbuffer_depth", gbuffer_depth_view)
        .add_sampler_decleration("gbuffer_sampler", gbuffer_sampler)
        .add_image_decleration("indirect_lighting_output", indirect_lighting_view);
    return bindpoints.build();
}

RenderBindpointDeclarations ApplicationVulkanDdgiPBRTest::build_probe_trace_bindpoints(Ref<ROBufferVK> p_light_info_buffer) const {
    RayTracingPluginVK* ray_tracing_plugin = RayTracingPluginVK::get_singleton();
    if (ray_tracing_plugin == nullptr) {
        throw std::runtime_error("Failed to build DDGI probe trace bindpoints: ray tracing plugin is unavailable.");
    }
    Ref<ROCSingleVK> static_opaque_tlas = ray_tracing_plugin->get_static_opaque_top_level_acceleration_structure_container();
    if (static_opaque_tlas == MEM_NULL) {
        throw std::runtime_error("Failed to build DDGI probe trace bindpoints: static-opaque TLAS is unavailable.");
    }
    Ref<IRenderObjectContainer> acceleration_structure_container = static_opaque_tlas;
    RenderBindpointDeclarationsBuilder bindpoints;
    bindpoints.add_storage_buffer_decleration("ddgi_config", ddgi_config_ubo)
        .add_storage_buffer_decleration("ddgi_time_info", ddgi_time_info_ubos)
        .add_storage_buffer_decleration("spot_light_info", p_light_info_buffer)
        .add_acceleration_structure_decleration("static_opaque_tlas", acceleration_structure_container)
        .add_image_decleration("probe_rays", ddgi_probe_rays_view);
    return bindpoints.build();
}

RenderBindpointDeclarations ApplicationVulkanDdgiPBRTest::build_probe_update_bindpoints() const {
    RenderBindpointDeclarationsBuilder bindpoints;
    bindpoints.add_storage_buffer_decleration("ddgi_config", ddgi_config_ubo)
        .add_storage_buffer_decleration("ddgi_time_info", ddgi_time_info_ubos)
        .add_image_decleration("probe_rays", ddgi_probe_rays_view)
        .add_image_decleration("irradiance_atlas", ddgi_irradiance_view)
        .add_image_decleration("distance_atlas", ddgi_distance_view);
    return bindpoints.build();
}

RenderBindpointDeclarations ApplicationVulkanDdgiPBRTest::build_combine_lighting_bindpoints() const {
    RenderBindpointDeclarationsBuilder bindpoints;
    bindpoints.add_image_decleration("gbuffer_rma", gbuffer_rma_view)
        .add_image_decleration("direct_lighting", direct_lighting_view)
        .add_image_decleration("indirect_lighting", indirect_lighting_view)
        .add_image_decleration("result_image", result_image_view);
    return bindpoints.build();
}

RenderBindpointDeclarations ApplicationVulkanDdgiPBRTest::build_bloom_horizontal_bindpoints() const {
    RenderBindpointDeclarationsBuilder bindpoints;
    bindpoints.add_image_decleration("bloom_input", result_image_view)
        .add_image_decleration("bloom_output", bloom_horizontal_image_view)
        .add_sampler_decleration("gbuffer_sampler", gbuffer_sampler);
    return bindpoints.build();
}

RenderBindpointDeclarations ApplicationVulkanDdgiPBRTest::build_bloom_vertical_bindpoints() const {
    RenderBindpointDeclarationsBuilder bindpoints;
    bindpoints.add_image_decleration("bloom_input", bloom_horizontal_image_view)
        .add_image_decleration("bloom_output", bloom_image_view)
        .add_sampler_decleration("gbuffer_sampler", gbuffer_sampler);
    return bindpoints.build();
}

RenderBindpointDeclarations ApplicationVulkanDdgiPBRTest::build_present_post_processing_bindpoints() const {
    RenderBindpointDeclarationsBuilder bindpoints;
    bindpoints.add_image_decleration("result_image", result_image_view)
        .add_image_decleration("bloom_image", bloom_image_view)
        .add_sampler_decleration("result_sampler", gbuffer_sampler);
    return bindpoints.build();
}

void ApplicationVulkanDdgiPBRTest::compile_lighting_pipelines() {
    CompositionCameraInfo camera_info = make_composition_camera_info();
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    DDGIConfig ddgi_config = make_ddgi_config();
    DDGITimeInfo ddgi_time_info = make_ddgi_time_info(0.0);
    uint32_t spot_light_count = 1;
    Ref<ROBufferVK> camera_buffer = create_initialized_uniform_buffer(&camera_info, sizeof(camera_info));
    Ref<ROBufferVK> light_info_buffer = create_initialized_uniform_buffer(&spot_light_info, sizeof(spot_light_info));
    Ref<ROBufferVK> light_count_buffer = create_initialized_uniform_buffer(&spot_light_count, sizeof(spot_light_count));
    ddgi_config_ubo = create_initialized_uniform_buffer(&ddgi_config, sizeof(ddgi_config));
    ddgi_time_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(DDGITimeInfo));
    ddgi_time_info_ubos->publish_buffer_update(&ddgi_time_info, 0, sizeof(ddgi_time_info));
    ComputePipelineCompilerVKContext compute_context = make_compute_pipeline_context();
    auto direct_compiled = ComputePipelineCompilerVK(compute_context)
                               .compile("direct_lighting_pcf_shader",
                                   direct_lighting_pcf_shader_resource,
                                   build_direct_lighting_bindpoints(camera_buffer, light_info_buffer, light_count_buffer, shadow_depth_view));
    direct_lighting_compute_pipeline = direct_compiled.compute_pipeline;
    auto indirect_compiled = ComputePipelineCompilerVK(compute_context)
                                 .compile("indirect_lighting_ddgi_shader",
                                     indirect_lighting_ddgi_shader_resource,
                                     build_indirect_lighting_bindpoints(camera_buffer, light_info_buffer, light_count_buffer));
    indirect_lighting_compute_pipeline = indirect_compiled.compute_pipeline;
    auto trace_compiled = ComputePipelineCompilerVK(compute_context)
                              .compile("ddgi_trace_probes_shader", ddgi_trace_probes_shader_resource, build_probe_trace_bindpoints(light_info_buffer));
    ddgi_trace_compute_pipeline = trace_compiled.compute_pipeline;
    auto update_compiled = ComputePipelineCompilerVK(compute_context)
                               .compile("ddgi_update_probes_shader", ddgi_update_probes_shader_resource, build_probe_update_bindpoints());
    ddgi_update_compute_pipeline = update_compiled.compute_pipeline;
    auto combine_compiled =
        ComputePipelineCompilerVK(compute_context).compile("combine_lighting_shader", combine_lighting_shader_resource, build_combine_lighting_bindpoints());
    combine_lighting_compute_pipeline = combine_compiled.compute_pipeline;
    auto bloom_horizontal_compiled =
        ComputePipelineCompilerVK(compute_context).compile("bloom_shader_horizontal", bloom_horizontal_shader_resource, build_bloom_horizontal_bindpoints());
    bloom_horizontal_compute_pipeline = bloom_horizontal_compiled.compute_pipeline;
    auto bloom_vertical_compiled =
        ComputePipelineCompilerVK(compute_context).compile("bloom_shader_vertical", bloom_vertical_shader_resource, build_bloom_vertical_bindpoints());
    bloom_vertical_compute_pipeline = bloom_vertical_compiled.compute_pipeline;
    auto present_compiled =
        GraphicsPipelineCompilerVK(make_present_graphics_pipeline_context())
            .compile("present_post_processing", present_post_processing_pipeline_resource, build_present_post_processing_bindpoints());
    present_post_processing_graphics_pipeline = present_compiled.graphics_pipeline;
}

void ApplicationVulkanDdgiPBRTest::write_dynamic_camera_descriptors(std::span<RTRunStaticSetVK* const> p_gbuffer_tasks) {
    if (p_gbuffer_tasks.empty()) {
        throw std::runtime_error("Failed to setup deferred test: compiled pipelines are unavailable.");
    }
    gbuffer_camera_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(CameraInfoVKUBO));
    CameraInfoVKUBO gbuffer_camera_info = make_camera_info_ubo(camera_component);
    gbuffer_camera_bindings.clear();
    gbuffer_camera_bindings.reserve(p_gbuffer_tasks.size());
    for (RTRunStaticSetVK* gbuffer_task : p_gbuffer_tasks) {
        if (gbuffer_task == nullptr) {
            throw std::runtime_error("Failed to setup deferred test: a G-buffer task is null.");
        }
        gbuffer_camera_bindings.push_back(TaskUniformBinding{.task = gbuffer_task, .field_path = "camera_info"});
    }

    composition_camera_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(CompositionCameraInfo));
    CompositionCameraInfo composition_camera_info = make_composition_camera_info();
    composition_camera_bindings.clear();
    composition_camera_bindings.push_back(ComputeUniformBinding{
        .pipeline = direct_lighting_compute_pipeline.get(),
        .field_path = "camera_info",
    });
    composition_camera_bindings.push_back(ComputeUniformBinding{
        .pipeline = indirect_lighting_compute_pipeline.get(),
        .field_path = "camera_info",
    });

    gbuffer_camera_info_ubos->publish_buffer_update(&gbuffer_camera_info, 0, sizeof(gbuffer_camera_info));
    composition_camera_info_ubos->publish_buffer_update(&composition_camera_info, 0, sizeof(composition_camera_info));
    log_uniform_buffer_descriptors(gbuffer_camera_info_ubos, "G-buffer camera buffer");
    log_uniform_buffer_descriptors(composition_camera_info_ubos, "Composition camera buffer");

    write_task_uniform_bindings(gbuffer_camera_bindings, gbuffer_camera_info_ubos);
    write_compute_uniform_bindings(composition_camera_bindings, composition_camera_info_ubos);
}

void ApplicationVulkanDdgiPBRTest::write_dynamic_light_descriptors(std::span<RTRunStaticSetVK* const> p_shadow_tasks) {
    if (p_shadow_tasks.empty()) {
        throw std::runtime_error("Failed to setup deferred test: compiled light pipelines are unavailable.");
    }
    spot_light_info_ubos = create_per_frame_uniform_buffers(render_allocator, base_render_data, sizeof(SpotLightInfoVKUBO));
    SpotLightInfoVKUBO spot_light_info = make_spot_light_info_ubo(spot_light_component);
    shadow_light_bindings.clear();

    for (RTRunStaticSetVK* shadow_task : p_shadow_tasks) {
        if (shadow_task == nullptr) {
            throw std::runtime_error("Failed to setup deferred test: a shadow task is null.");
        }
        shadow_light_bindings.push_back(TaskUniformBinding{.task = shadow_task, .field_path = "light_camera_info"});
    }

    spot_light_bindings.clear();
    spot_light_bindings.push_back(ComputeUniformBinding{
        .pipeline = direct_lighting_compute_pipeline.get(),
        .field_path = "light.lights[0].info",
    });

    spot_light_info_ubos->publish_buffer_update(&spot_light_info, 0, sizeof(spot_light_info));
    log_uniform_buffer_descriptors(spot_light_info_ubos, "Spot light buffer");

    write_task_uniform_bindings(shadow_light_bindings, spot_light_info_ubos);
    write_compute_uniform_bindings(spot_light_bindings, spot_light_info_ubos);
    ddgi_trace_compute_pipeline->set_descriptor_binding("spot_light_info", spot_light_info_ubos);
}

void ApplicationVulkanDdgiPBRTest::write_dynamic_ddgi_descriptors() {
    if (ddgi_trace_compute_pipeline == MEM_NULL || ddgi_update_compute_pipeline == MEM_NULL) {
        throw std::runtime_error("Failed to write DDGI descriptors: probe pipelines are unavailable.");
    }
    if (ddgi_time_info_ubos == MEM_NULL) {
        throw std::runtime_error("Failed to write DDGI descriptors: DDGI time buffer container is unavailable.");
    }
    ddgi_trace_compute_pipeline->set_descriptor_binding("ddgi_time_info", ddgi_time_info_ubos);
    ddgi_update_compute_pipeline->set_descriptor_binding("ddgi_time_info", ddgi_time_info_ubos);
}

void ApplicationVulkanDdgiPBRTest::register_ddgi_render_graph(
    Unique<RTRunStaticSetVK>& p_shadow_opaque_task, Unique<RTRunStaticSetVK>& p_gbuffer_opaque_task) {
    RendGraphDescVK pipeline_desc(render_allocator);
    add_render_graph_resources(pipeline_desc);
    RendTaskDescVK shadow_opaque_task_desc(render_allocator, RT_RUN_DDGI_SHADOW_OPAQUE, std::move(p_shadow_opaque_task));
    pipeline_desc.add_task(std::move(shadow_opaque_task_desc));
    RendTaskDescVK gbuffer_opaque_task_desc(render_allocator, RT_RUN_DDGI_GBUFFER, std::move(p_gbuffer_opaque_task));
    gbuffer_opaque_task_desc.after(RT_RUN_DDGI_SHADOW_OPAQUE);
    pipeline_desc.add_task(std::move(gbuffer_opaque_task_desc));
    pipeline_desc.add_task(create_direct_lighting_task_desc())
        .add_task(create_probe_trace_task_desc())
        .add_task(create_probe_update_task_desc())
        .add_task(create_indirect_lighting_task_desc())
        .add_task(create_combine_lighting_task_desc())
        .add_task(create_bloom_task_desc())
        .add_task(create_present_result_image_task_desc())
        .add_task(create_light_billboard_task_desc());
    Ref<RenderGraphVK> graph = renderer_vk->register_render_graph(DDGI_RENDER_GRAPH, std::move(pipeline_desc));
    write_descriptor_debug_output("ddgi_graph_descriptors", graph->list_descriptors());
}

void ApplicationVulkanDdgiPBRTest::add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const {
    RayTracingPluginVK* ray_tracing_plugin = RayTracingPluginVK::get_singleton();
    if (ray_tracing_plugin == nullptr) {
        throw std::runtime_error("Failed to build DDGI render graph: ray tracing plugin is unavailable.");
    }
    Ref<ROCSingleVK> static_opaque_tlas_container = ray_tracing_plugin->get_static_opaque_top_level_acceleration_structure_container();
    if (static_opaque_tlas_container == MEM_NULL) {
        throw std::runtime_error("Failed to build DDGI render graph: static-opaque TLAS container is unavailable.");
    }
    Ref<RenderObjectContainerVK> static_opaque_tlas_graph_container = static_opaque_tlas_container;
    p_pipeline_desc.add_resource(REND_RES_SWAP_CHAIN_IMAGE, TypeUUID<ROImageViewVK>::UUID)
        .add_resource(STATIC_OPAQUE_TLAS_RESOURCE, static_opaque_tlas_graph_container)
        .add_resource(DDGI_PROBE_RAYS, ddgi_probe_rays_view)
        .add_resource(DDGI_IRRADIANCE_ATLAS, ddgi_irradiance_view)
        .add_resource(DDGI_DISTANCE_ATLAS, ddgi_distance_view)
        .add_resource(SHADOWMAP_COLOR_IMAGE_VIEW, shadow_color_view)
        .add_resource(SHADOWMAP_DEPTH_IMAGE_VIEW, shadow_depth_view)
        .add_resource(GBUFFER_ALBEDO_IMAGE_VIEW, gbuffer_albedo_view)
        .add_resource(GBUFFER_NORMAL_IMAGE_VIEW, gbuffer_normal_view)
        .add_resource(GBUFFER_RMA_IMAGE_VIEW, gbuffer_rma_view)
        .add_resource(GBUFFER_DEPTH_IMAGE_VIEW, gbuffer_depth_view)
        .add_resource(DIRECT_LIGHTING_IMAGE_VIEW, direct_lighting_view)
        .add_resource(INDIRECT_LIGHTING_IMAGE_VIEW, indirect_lighting_view)
        .add_resource(RESULT_IMAGE_VIEW, result_image_view)
        .add_resource(BLOOM_HORIZONTAL_IMAGE_VIEW, bloom_horizontal_image_view)
        .add_resource(BLOOM_IMAGE_VIEW, bloom_image_view);
}

RendTaskDescVK ApplicationVulkanDdgiPBRTest::create_direct_lighting_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_DIRECT_LIGHTING;
    task_desc.after(RT_RUN_DDGI_GBUFFER)
        .read(SHADOWMAP_DEPTH_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_ALBEDO_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_RMA_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_DEPTH_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(DIRECT_LIGHTING_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = direct_lighting_compute_pipeline]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDdgiPBRTest::create_probe_trace_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    DDGIConfig config = make_ddgi_config();
    task_desc.task_id = RT_TRACE_DDGI_PROBES;
    task_desc.after(RT_RUN_DDGI_GBUFFER)
        .read(STATIC_OPAQUE_TLAS_RESOURCE, RendResUsageVK::ACCELERATION_STRUCTURE)
        .write(DDGI_PROBE_RAYS, RendResUsageVK::STORAGE_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = ddgi_trace_compute_pipeline,
                           config]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (DDGI_RAYS_PER_PROBE + 7U) / 8U,
            .height = (config.grid_counts_and_probe_count.w + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDdgiPBRTest::create_probe_update_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    DDGIConfig config = make_ddgi_config();
    task_desc.task_id = RT_UPDATE_DDGI_PROBES;
    task_desc.after(RT_TRACE_DDGI_PROBES)
        .read(DDGI_PROBE_RAYS, RendResUsageVK::SAMPLED_IMAGE)
        .write(DDGI_IRRADIANCE_ATLAS, RendResUsageVK::STORAGE_IMAGE)
        .write(DDGI_DISTANCE_ATLAS, RendResUsageVK::STORAGE_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = ddgi_update_compute_pipeline,
                           config]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (config.grid_counts_and_probe_count.x * config.grid_counts_and_probe_count.y * DDGI_DISTANCE_RESOLUTION + 7U) / 8U,
            .height = (config.grid_counts_and_probe_count.z * DDGI_DISTANCE_RESOLUTION + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDdgiPBRTest::create_indirect_lighting_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_INDIRECT_LIGHTING;
    task_desc.after(RT_UPDATE_DDGI_PROBES)
        .read(GBUFFER_ALBEDO_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_NORMAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_RMA_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(GBUFFER_DEPTH_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(DDGI_IRRADIANCE_ATLAS, RendResUsageVK::SAMPLED_IMAGE)
        .read(DDGI_DISTANCE_ATLAS, RendResUsageVK::SAMPLED_IMAGE)
        .write(INDIRECT_LIGHTING_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = indirect_lighting_compute_pipeline]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDdgiPBRTest::create_combine_lighting_task_desc() const {
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
                           task_pipeline = combine_lighting_compute_pipeline]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        return make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline, dispatch_group_count);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDdgiPBRTest::create_bloom_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_BLOOM;
    task_desc.after(RT_COMBINE_LIGHTING)
        .read(RESULT_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .read(BLOOM_HORIZONTAL_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE)
        .write(BLOOM_HORIZONTAL_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE)
        .write(BLOOM_IMAGE_VIEW, RendResUsageVK::STORAGE_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           horizontal_pipeline = bloom_horizontal_compute_pipeline,
                           vertical_pipeline = bloom_vertical_compute_pipeline]() -> Unique<RenderTaskVK> {
        VkExtent3D dispatch_group_count{
            .width = (task_base_render_data->swap_chain_extent.width + 7U) / 8U,
            .height = (task_base_render_data->swap_chain_extent.height + 7U) / 8U,
            .depth = 1,
        };
        std::array<RTRunComputePipelinesSequentialVK::Dispatch, 2> dispatches{{
            {.pipeline = horizontal_pipeline, .group_count = dispatch_group_count},
            {.pipeline = vertical_pipeline, .group_count = dispatch_group_count},
        }};
        return make_unique<RTRunComputePipelinesSequentialVK>(task_render_allocator, task_base_render_data, dispatches);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDdgiPBRTest::create_present_result_image_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_PRESENT_RESULT_IMAGE;
    task_desc.after(RT_BLOOM).read(RESULT_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE).read(BLOOM_IMAGE_VIEW, RendResUsageVK::SAMPLED_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = present_post_processing_graphics_pipeline]() -> Unique<RenderTaskVK> {
        return make_unique<RTRunGraphicsPipelineVK>(task_render_allocator,
            task_base_render_data,
            glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
            task_base_render_data->swap_chain_extent,
            REND_RES_SWAP_CHAIN_IMAGE,
            task_pipeline,
            VK_ATTACHMENT_LOAD_OP_CLEAR);
    };
    return task_desc;
}

RendTaskDescVK ApplicationVulkanDdgiPBRTest::create_light_billboard_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_LIGHT_BILLBOARD;
    task_desc.after(RT_RUN_PRESENT_RESULT_IMAGE);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = light_billboard_pipeline]() -> Unique<RenderTaskVK> {
        std::array<VkClearValue, 2> clear_values{};
        std::array<std::string_view, 1> color_attachment_ids{REND_RES_SWAP_CHAIN_IMAGE};
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

ApplicationVulkanDdgiPBRTest::CompositionCameraInfo ApplicationVulkanDdgiPBRTest::make_composition_camera_info() const {
    return CompositionCameraInfo{
        .inverse_view_projection = glm::inverse(camera_component.proj * camera_component.view),
        .camera_world_position = glm::vec4(camera_position, 1.0F),
    };
}

ApplicationVulkanDdgiPBRTest::DDGIConfig ApplicationVulkanDdgiPBRTest::make_ddgi_config() const {
    glm::uvec3 grid_counts = glm::uvec3(4U, 2U, 4U);
    glm::vec3 cell_spacing = glm::vec3(7.0F, 7.5F, 7.0F);
    glm::vec3 grid_origin = glm::vec3(-10.5F, -1.0F, -10.5F);
    glm::vec3 probe_volume_extent = glm::vec3(grid_counts - glm::uvec3(1U)) * cell_spacing;
    uint32_t probe_count = grid_counts.x * grid_counts.y * grid_counts.z;
    return DDGIConfig{
        .grid_counts_and_probe_count = glm::uvec4(grid_counts, probe_count),
        .cell_spacing_and_max_ray_distance = glm::vec4(cell_spacing, 12.0F),
        .grid_origin_and_normal_bias = glm::vec4(grid_origin, 0.15F),
        .probe_volume_extent_and_view_bias = glm::vec4(probe_volume_extent, 0.05F),
        .indirect_scale_and_hysteresis = glm::vec4(2.5F, 0.97F, 0.0F, 0.0F),
    };
}

ApplicationVulkanDdgiPBRTest::DDGITimeInfo ApplicationVulkanDdgiPBRTest::make_ddgi_time_info(double p_delta_seconds) const {
    uint32_t ray_rotation_seed = ddgi_frame_index * 747796405U + 2891336453U;
    return DDGITimeInfo{
        .frame_index = ddgi_frame_index,
        .ray_rotation_seed = ray_rotation_seed,
        .delta_seconds = static_cast<float>(p_delta_seconds),
        .elapsed_seconds = static_cast<float>(ddgi_elapsed_seconds),
    };
}

} // namespace WhiteBirdEngine

namespace WBE = WhiteBirdEngine;

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Directory({"test_env"}));
    {
        WBE::ApplicationVulkanDdgiPBRTest game_application;
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
