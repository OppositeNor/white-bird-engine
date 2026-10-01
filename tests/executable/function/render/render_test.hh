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
#ifndef WBE_FILE_RENDER_TEST_HH
#define WBE_FILE_RENDER_TEST_HH
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/ecs/components.hh"
#include "function/render/i_renderer.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/material_texture_contract_resolver.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/dynamic_mesh_resources_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/i_mesh_compiler_vk.hh"
#include "function/render_vk/mesh_draw_batch_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_targets_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/static_mesh_compiler_vk.hh"
#include "function/render_vk/static_mesh_resources_vk.hh"
#include "platform/os/os.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <print>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

inline constexpr std::string_view RT_RUN_PIPELINE = "run_pipeline";
inline constexpr std::string_view RT_RUN_PIPELINE_SHADOWMAP = "run_shadowmap";
inline constexpr std::string_view REND_RES_TEST_SHADOWMAP_COLOR_IMAGE_VIEW = "shadowmap_color";
inline constexpr std::string_view REND_RES_TEST_SHADOWMAP_DEPTH_IMAGE_VIEW = "shadowmap_depth";
inline constexpr std::string_view REND_RES_TEST_RSM_NORMAL_IMAGE_VIEW = "rsm_normal";
inline constexpr glm::vec3 TEST_DEFAULT_CAMERA_POSITION = glm::vec3(2.0F, 2.0F, 2.0F);
inline constexpr glm::vec3 TEST_DEFAULT_CAMERA_TARGET = glm::vec3(0.0F, 0.0F, 0.0F);
inline constexpr float TEST_CAMERA_MOVE_SPEED = 3.0F;
inline constexpr float TEST_CAMERA_TURN_SPEED = glm::radians(90.0F);
inline constexpr float TEST_CAMERA_PITCH_LIMIT = glm::radians(89.0F);

namespace WhiteBirdEngine {

inline std::string make_descriptor_debug_file_stem(std::string_view p_name) {
    std::string result;
    result.reserve(p_name.size());
    for (char character : p_name) {
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')) {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        } else {
            result.push_back('_');
        }
    }
    if (result.empty()) {
        throw std::runtime_error("Descriptor debug output name is empty.");
    }
    return result;
}

inline void write_descriptor_debug_output(std::string_view p_name, std::string_view p_json) {
    std::filesystem::path output_dir = std::filesystem::path("test_env") / "descriptor_debug";
    std::filesystem::create_directories(output_dir);
    std::filesystem::path output_path = output_dir / std::format("{}.json", make_descriptor_debug_file_stem(p_name));
    std::ofstream output(output_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error(std::format("Failed to open descriptor debug output file: {}.", output_path.generic_string()));
    }
    output << nlohmann::json::parse(p_json).dump(2) << '\n';
    if (!output.good()) {
        throw std::runtime_error(std::format("Failed to write descriptor debug output file: {}.", output_path.generic_string()));
    }
}

inline void write_descriptor_debug_output(std::string_view p_name, const DebugData& p_data) {
    std::ostringstream stream;
    stream << p_data;
    write_descriptor_debug_output(p_name, stream.str());
}

inline void on_exec_test_render(IRenderer& p_renderer, uint64_t p_frame) {
    static std::string capture_png_path;
    static uint64_t capture_frame = 5;
    static bool capture_requested = false;
    const char* capture_path = std::getenv("WBE_CAPTURE_PNG");
    if (capture_path != nullptr && *capture_path != '\0') {
        if (*capture_path != '/') {
            throw std::runtime_error("WBE_CAPTURE_PNG must be an absolute output path.");
        }
        capture_png_path = capture_path;
    }
    if (const char* frame = std::getenv("WBE_CAPTURE_FRAME"); frame != nullptr) {
        std::string frame_text(frame);
        const char* frame_end = frame_text.data() + frame_text.size();
        auto result = std::from_chars(frame_text.c_str(), frame_end, capture_frame);
        if (result.ec != std::errc{} || result.ptr != frame_end) {
            throw std::runtime_error("WBE_CAPTURE_FRAME must be a nonnegative frame number.");
        }
    }
    if (!capture_png_path.empty() && !capture_requested && p_frame >= capture_frame) {
        p_renderer.request_frame_capture(capture_png_path);
        capture_requested = true;
    }
}

inline void on_exec_test_start() {

    const char* wait_process_inject = std::getenv("WBE_WAIT_PROCESS_INJECT");
    if (wait_process_inject != nullptr && std::string_view(wait_process_inject) == "1") {
        std::println("WBE_WAIT_PROCESS_INJECT is set. Running on PID: {}, Press Enter to continue...", OS::get_pid());
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

inline std::string_view mesh_vertex_entry(RefRes<RUGraphicsPipeline> p_pipeline) {
    if (p_pipeline == MEM_NULL || p_pipeline->vertex_stage == MEM_NULL) {
        throw std::runtime_error("Failed to resolve mesh vertex entry: graphics pipeline or vertex stage is null.");
    }
    if (p_pipeline->vertex_stage_entry != MEM_NULL && p_pipeline->vertex_stage_entry.get()[0] != '\0') {
        return p_pipeline->vertex_stage_entry.get();
    }
    // An empty entry resolves to the vertex shader's only reflected vertex entry point.
    return {};
}

inline std::optional<MaterialTextureContract> load_mesh_material_contract(RefRes<RUGraphicsPipeline> p_pipeline) {
    if (p_pipeline == MEM_NULL || p_pipeline->vertex_stage == MEM_NULL || p_pipeline->fragment_stage == MEM_NULL) {
        throw std::runtime_error("Failed to load mesh material contract: graphics pipeline stages are missing.");
    }
    ShaderMetadata vertex_metadata = ShaderMetadataLoader().load(p_pipeline->vertex_stage);
    ShaderMetadata fragment_metadata = ShaderMetadataLoader().load(p_pipeline->fragment_stage);
    std::optional<ShaderMetadata> geometry_metadata;
    if (p_pipeline->geometry_stage != MEM_NULL) {
        geometry_metadata = ShaderMetadataLoader().load(p_pipeline->geometry_stage);
    }
    return MaterialTextureContractResolver().resolve(
        vertex_metadata, geometry_metadata.has_value() ? &*geometry_metadata : nullptr, fragment_metadata, "material_textures", REND_RES_MESH_MATERIAL_PARAMS);
}

inline Ref<StaticMeshResourcesVK> create_static_mesh_resources(RendererVK* p_renderer,
    RefRes<RUStaticOpaqueMesh> p_opaque_mesh,
    RefRes<RUStaticMaskedMesh> p_masked_mesh,
    RefRes<RUGraphicsPipeline> p_opaque_pipeline,
    RefRes<RUGraphicsPipeline> p_masked_pipeline,
    RefRes<RUGraphicsPipeline> p_material_pipeline,
    RefRes<RUStaticOpaqueDoubleSidedMesh> p_opaque_double_sided_mesh = MEM_NULL) {
    ShaderMetadata opaque_metadata;
    ShaderMetadata masked_metadata;
    if (p_opaque_mesh != MEM_NULL || p_opaque_double_sided_mesh != MEM_NULL) {
        opaque_metadata = ShaderMetadataLoader().load(p_opaque_pipeline->vertex_stage);
    }
    if (p_masked_mesh != MEM_NULL) {
        masked_metadata = ShaderMetadataLoader().load(p_masked_pipeline->vertex_stage);
    }
    const std::optional<MaterialTextureContract> material_contract =
        p_material_pipeline != MEM_NULL ? load_mesh_material_contract(p_material_pipeline) : std::nullopt;
    return StaticMeshCompilerVK(p_renderer)
        .compile(p_opaque_mesh,
            p_masked_mesh,
            opaque_metadata,
            (p_opaque_mesh != MEM_NULL || p_opaque_double_sided_mesh != MEM_NULL) ? mesh_vertex_entry(p_opaque_pipeline)
                                                                                : std::string_view{},
            masked_metadata,
            p_masked_mesh != MEM_NULL ? mesh_vertex_entry(p_masked_pipeline) : std::string_view{},
            material_contract,
            StaticMeshCompilerVK::Config{},
            p_opaque_double_sided_mesh);
}

/**
 * @brief Copy a trivially copyable value into bytes used as initial graph-buffer data.
 * @param p_value The value to copy.
 * @return The object representation of p_value.
 */
template <typename T>
std::vector<uint8_t> to_initial_data(const T& p_value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* bytes = reinterpret_cast<const uint8_t*>(&p_value);
    return std::vector<uint8_t>(bytes, bytes + sizeof(T));
}

/**
 * @brief Connect consecutive uses of one resource with resource edges.
 *
 * Every task is connected to the next one; the graph derives each edge's synchronization from the tasks' reported usages.
 *
 * @param p_graph Graph descriptor that already contains every linked task.
 * @param p_resource_id Graph resource carried by the edges.
 * @param p_task_ids Tasks that use the resource, in execution order.
 */
inline void connect_resource_chain(RendGraphDescVK& p_graph, std::string_view p_resource_id, std::span<const std::string_view> p_task_ids) {
    for (size_t i = 1; i < p_task_ids.size(); ++i) {
        p_graph.get_task(p_task_ids[i - 1]).connect(p_task_ids[i], p_resource_id);
    }
}

/**
 * @brief Graph resource keys of one compiled mesh that back the shader-side mesh resource ids.
 */
struct MeshResourceKeys {
    /** Graph key bound to REND_RES_MESH_SUBMESHES. */
    std::string_view submeshes;
    /** Graph key bound to REND_RES_MESH_NODES. */
    std::string_view nodes;
    /** Graph key bound to REND_RES_MESH_INSTANCES. */
    std::string_view instances;
    /** Graph key bound to REND_RES_MESH_MATERIAL_PARAMS. */
    std::string_view material_params;
    /** Graph key bound to REND_RES_MESH_MATERIAL_SAMPLER. */
    std::string_view material_sampler;
};

/**
 * @brief Keys under which RendGraphDescVK::add_static_mesh() imports a static mesh.
 */
inline constexpr MeshResourceKeys STATIC_MESH_RESOURCE_KEYS{
    .submeshes = REND_RES_STATIC_MESH_SUBMESHES,
    .nodes = REND_RES_STATIC_MESH_NODES,
    .instances = REND_RES_STATIC_MESH_INSTANCES,
    .material_params = REND_RES_MESH_MATERIAL_PARAMS,
    .material_sampler = REND_RES_MESH_MATERIAL_SAMPLER,
};

/**
 * @brief Keys under which RendGraphDescVK::add_dynamic_mesh() imports a dynamic mesh.
 */
inline constexpr MeshResourceKeys DYNAMIC_MESH_RESOURCE_KEYS{
    .submeshes = REND_RES_DYNAMIC_MESH_SUBMESHES,
    .nodes = REND_RES_DYNAMIC_MESH_NODES,
    .instances = REND_RES_DYNAMIC_MESH_INSTANCES,
    .material_params = REND_RES_DYNAMIC_MESH_MATERIAL_PARAMS,
    .material_sampler = REND_RES_DYNAMIC_MESH_MATERIAL_SAMPLER,
};

/**
 * @brief Map the shader-side mesh resource ids a pipeline reflects to one mesh's graph resources.
 * @details Ids the pipeline does not declare are skipped because every alias must be used by the task; ids whose key
 * already equals the shader id need no alias.
 * @param p_task Task drawing the mesh with p_pipeline.
 * @param p_pipeline Pipeline whose reflected descriptor layout selects the aliased ids.
 * @param p_keys Graph keys of the drawn mesh.
 * @return p_task.
 */
inline RendTaskDescVK& alias_mesh_resources(RendTaskDescVK& p_task, const ROGraphicsPipelineVK& p_pipeline, const MeshResourceKeys& p_keys) {
    const std::array<std::pair<std::string_view, std::string_view>, 5> mappings{{
        {REND_RES_MESH_SUBMESHES, p_keys.submeshes},
        {REND_RES_MESH_NODES, p_keys.nodes},
        {REND_RES_MESH_INSTANCES, p_keys.instances},
        {REND_RES_MESH_MATERIAL_PARAMS, p_keys.material_params},
        {REND_RES_MESH_MATERIAL_SAMPLER, p_keys.material_sampler},
    }};
    const auto& fields = p_pipeline.get_descriptor_layout().fields;
    for (const auto& [shader_id, graph_id] : mappings) {
        if (shader_id == graph_id) {
            continue;
        }
        if (std::ranges::any_of(fields, [shader_id](const auto& p_field) {
                return p_field.resource_id == shader_id;
            })) {
            p_task.alias(shader_id, graph_id);
        }
    }
    return p_task;
}

/**
 * @brief Convert one compiled mesh draw batch into a pipeline batch drawn with p_pipeline.
 */
inline RTRunGraphicsPipelineIndirectVK::PipelineBatch to_pipeline_batch(const MeshDrawBatchVK& p_source, Ref<ROGraphicsPipelineVK> p_pipeline) {
    RTRunGraphicsPipelineIndirectVK::PipelineBatch batch;
    batch.graphics_pipeline = std::move(p_pipeline);
    batch.first_draw = p_source.first_draw;
    batch.draw_count = p_source.draw_count;
    batch.vertex_input = p_source.vertex_input;
    batch.vertex_buffer_offset = p_source.vertex_buffer_offset;
    return batch;
}

/**
 * @brief Convert every draw batch of a dynamic mesh into pipeline batches drawn with p_pipeline.
 */
inline Vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch, RenderAllocator> make_dynamic_mesh_pipeline_batches(
    RenderAllocator* p_allocator, const DynamicMeshResourcesVK& p_resources, Ref<ROGraphicsPipelineVK> p_pipeline) {
    Vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch, RenderAllocator> result(p_allocator);
    result.reserve(p_resources.get_draw_batch_count());
    for (size_t i = 0; i < p_resources.get_draw_batch_count(); ++i) {
        result.push_back(to_pipeline_batch(p_resources.get_draw_batch(i), p_pipeline));
    }
    return result;
}

inline Vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch, RenderAllocator> make_static_mesh_pipeline_batches(
    RenderAllocator* p_allocator, Ref<StaticMeshResourcesVK> p_resources, Ref<ROGraphicsPipelineVK> p_pipeline, bool p_masked, bool p_double_sided = false) {
    Vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch, RenderAllocator> result(p_allocator);
    size_t batch_count = [&p_resources, p_double_sided, p_masked]() {
        if (p_double_sided) {
            return p_resources->get_opaque_double_sided_draw_batch_count();
        }
        return p_masked ? p_resources->get_masked_draw_batch_count() : p_resources->get_opaque_draw_batch_count();
    }();
    result.reserve(batch_count);
    for (size_t i = 0; i < batch_count; ++i) {
        const MeshDrawBatchVK& source = [i, p_double_sided, p_masked, &p_resources]() {
            if (p_double_sided) {
                return p_resources->get_opaque_double_sided_draw_batch(i);
            }
            return p_masked ? p_resources->get_masked_draw_batch(i) : p_resources->get_opaque_draw_batch(i);
        }();
        result.push_back(to_pipeline_batch(source, p_pipeline));
    }
    return result;
}

/**
 * @brief Connect consecutive uses of one resource with resource edges.
 * @param p_graph Graph descriptor that already contains every linked task.
 * @param p_resource_id Graph resource carried by the edges.
 * @param p_task_ids Tasks that use the resource, in execution order.
 */
inline void connect_resource_chain(
    RendGraphDescVK& p_graph, std::string_view p_resource_id, std::initializer_list<std::string_view> p_task_ids) {
    connect_resource_chain(p_graph, p_resource_id, std::span<const std::string_view>(p_task_ids.begin(), p_task_ids.size()));
}

/**
 * @brief Map a graphics task's render targets to graph resources.
 * @param p_task Descriptor of a task that renders into `render_target_colors[<index>]` and `render_target_depth`.
 * @param p_color_resource_ids Graph resources of the color targets in pipeline order.
 * @param p_depth_resource_id Graph resource of the depth target, when the pipeline uses one.
 * @return p_task.
 */
inline RendTaskDescVK& alias_render_targets(RendTaskDescVK& p_task,
    std::span<const std::string_view> p_color_resource_ids,
    std::optional<std::string_view> p_depth_resource_id = std::nullopt) {
    for (size_t i = 0; i < p_color_resource_ids.size(); ++i) {
        p_task.alias(RenderTargetsVK::color_id(static_cast<uint32_t>(i)), p_color_resource_ids[i]);
    }
    if (p_depth_resource_id.has_value()) {
        p_task.alias(RenderTargetsVK::DEPTH_ID, *p_depth_resource_id);
    }
    return p_task;
}

inline Ref<ROGraphicsPipelineVK> compile_render_test_graphics_pipeline(RendererVK* p_renderer,
    std::string_view p_key,
    RefRes<RUGraphicsPipeline> p_pipeline,
    std::span<const VkFormat> p_color_formats,
    VkFormat p_depth_format = VK_FORMAT_UNDEFINED) {
    const BaseRenderDataVK* render_data = p_renderer->get_base_render_data();
    GraphicsPipelineCompilerVKContext context{
        .allocator = render_data->allocator,
        .render_data = render_data,
        .render_database = p_renderer->get_render_database(),
        .renderer = p_renderer,
        .color_attachment_formats = std::vector<VkFormat>(p_color_formats.begin(), p_color_formats.end()),
        .depth_attachment_format = p_depth_format,
    };
    return GraphicsPipelineCompilerVK(context).compile(p_key, p_pipeline).graphics_pipeline;
}

inline RendTaskDescVK make_static_mesh_task_desc(RenderAllocator* p_allocator, // NOLINT
    const BaseRenderDataVK* p_render_data,
    std::string_view p_task_id,
    Ref<StaticMeshResourcesVK> p_resources,
    Ref<ROGraphicsPipelineVK> p_pipeline,
    bool p_masked,
    std::span<const VkClearValue> p_clear_values,
    VkExtent2D p_render_extent,
    std::span<const std::string_view> p_color_attachment_ids,
    std::optional<std::string_view> p_depth_attachment_id = std::nullopt,
    VkAttachmentLoadOp p_color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
    VkAttachmentLoadOp p_depth_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
    bool p_double_sided = false) {
    auto batches = make_static_mesh_pipeline_batches(p_allocator, p_resources, p_pipeline, p_masked, p_double_sided);
    std::string_view indirect_buffer_id = [p_double_sided, p_masked]() {
        if (p_double_sided) {
            return REND_RES_STATIC_MESH_OPAQUE_DOUBLE_SIDED_DRAW_BATCHES;
        }
        return p_masked ? REND_RES_STATIC_MESH_MASKED_DRAW_BATCHES : REND_RES_STATIC_MESH_OPAQUE_DRAW_BATCHES;
    }();
    RTRunGraphicsPipelineIndirectVK::Builder task_builder(p_allocator);
    task_builder.set_render_extent(p_render_extent)
        .set_vertex_buffer(REND_RES_STATIC_MESH_VERTICES)
        .set_index_buffer(REND_RES_STATIC_MESH_INDICES)
        .set_indirect_buffer(indirect_buffer_id)
        .set_clear_values(p_clear_values)
        .set_color_load_op(p_color_load_op)
        .set_depth_load_op(p_depth_load_op);
    for (auto& batch : batches) {
        task_builder.add_pipeline_batch(std::move(batch));
    }
    RendTaskDescVK result(p_allocator, p_task_id, task_builder.create_unique(p_render_data));
    alias_render_targets(result, p_color_attachment_ids, p_depth_attachment_id);
    alias_mesh_resources(result, *p_pipeline, STATIC_MESH_RESOURCE_KEYS);
    return result;
}
} // namespace WhiteBirdEngine

inline glm::vec3 default_test_camera_direction() {
    return glm::normalize(TEST_DEFAULT_CAMERA_TARGET - TEST_DEFAULT_CAMERA_POSITION);
}

inline float default_test_camera_yaw() {
    glm::vec3 direction = default_test_camera_direction();
    return std::atan2(direction.z, direction.x);
}

inline float default_test_camera_pitch() {
    return std::asin(default_test_camera_direction().y);
}

inline glm::vec3 test_camera_forward(float p_yaw, float p_pitch) {
    return glm::normalize(glm::vec3(std::cos(p_pitch) * std::cos(p_yaw), std::sin(p_pitch), std::cos(p_pitch) * std::sin(p_yaw)));
}

inline glm::mat4 test_camera_view(const glm::vec3& p_position, float p_yaw, float p_pitch) {
    glm::vec3 forward = test_camera_forward(p_yaw, p_pitch);
    glm::vec3 up = std::abs(forward.y) > 0.99F ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
    return glm::lookAt(p_position, p_position + forward, up);
}

inline WBE::ComponentCamera make_default_test_camera_component(float p_aspect_ratio, float p_near_plane = 10.0F) {
    WBE::ComponentCamera camera_component{
        .view = test_camera_view(TEST_DEFAULT_CAMERA_POSITION, default_test_camera_yaw(), default_test_camera_pitch()),
        .proj = glm::perspectiveRH_ZO(glm::radians(45.0F), p_aspect_ratio, p_near_plane, 0.1F),
        .is_dirty = false,
    };
    camera_component.proj[1][1] *= -1;
    return camera_component;
}

inline WBE::CameraInfoVKUBO make_camera_info_ubo(const WBE::ComponentCamera& p_camera_component) {
    return WBE::CameraInfoVKUBO{
        .view = p_camera_component.view,
        .proj = p_camera_component.proj,
        .world_position = glm::inverse(p_camera_component.view)[3],
    };
}

inline WBE::ComponentSpotLight make_default_test_spot_light_component(float p_intensity) {
    WBE::ComponentSpotLight spot_light_component{
        .position = glm::vec3(2.5F, 3.5F, 2.5F),
        .direction = glm::normalize(glm::vec3(-1.0F, -1.4F, -1.0F)),
        .color = glm::vec3(1.0F, 0.95F, 0.85F),
        .intensity = p_intensity,
        .inner_cone_cos = std::cos(glm::radians(18.0F)),
        .outer_cone_cos = std::cos(glm::radians(28.0F)),
        .range = 12.0F,
        .emitter_width = 0.05F,
    };

    glm::vec3 up = std::abs(spot_light_component.direction.y) > 0.99F ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
    spot_light_component.view =
        glm::lookAt(spot_light_component.position, spot_light_component.position + spot_light_component.direction, up);
    float outer_half_angle = std::acos(std::clamp(spot_light_component.outer_cone_cos, -1.0F, 1.0F));
    float far_plane = std::max(spot_light_component.range, 0.2F);
    spot_light_component.proj = glm::perspectiveRH_ZO(2.0F * outer_half_angle, 1.0F, far_plane, 0.1F);
    spot_light_component.proj[1][1] *= -1.0F;
    return spot_light_component;
}

inline WBE::SpotLightInfoVKUBO make_spot_light_info_ubo(const WBE::ComponentSpotLight& p_spot_light_component) {
    return WBE::SpotLightInfoVKUBO{.view = p_spot_light_component.view,
        .proj = p_spot_light_component.proj,
        .inv_view_proj = glm::inverse(p_spot_light_component.proj * p_spot_light_component.view),
        .position = p_spot_light_component.position,
        .intensity = p_spot_light_component.intensity,
        .direction = p_spot_light_component.direction,
        .inner_cone_cos = p_spot_light_component.inner_cone_cos,
        .color = p_spot_light_component.color,
        .outer_cone_cos = p_spot_light_component.outer_cone_cos,
        .range = p_spot_light_component.range,
        .emitter_width = p_spot_light_component.emitter_width};
}

inline void log_uniform_buffer_descriptors(WBE::Ref<WBE::ROCMultislotsVK> p_buffers, std::string_view p_label) {
    WBE::write_descriptor_debug_output(std::format("{}_descriptors", p_label), p_buffers->to_string());
}

#endif
