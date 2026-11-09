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
#ifndef WBE_FILE_APPLICATION_VULKAN_DEFERRED_PBR_TEST_HH
#define WBE_FILE_APPLICATION_VULKAN_DEFERRED_PBR_TEST_HH

#include "../render_test.hh"
#include "core/allocator/i_allocator.hh"
#include "core/engine_core.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "dynamic_mesh_render_feature.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/event/event_registration.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/mesh_acceleration_structure_vk.hh"
#include "function/render_vk/mesh_compiler_vk.hh"
#include "function/render_vk/mesh_resources_vk.hh"
#include "function/render_vk/rend_graph_recipe_builder_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/rt_cpu_in_flight_acquire_vk.hh"
#include "deferred_recipe_context.hh"
#include "i_voxel_render_feature.hh"
#include "imgui_overlay_vk.hh"
#include "input_manager_mocked.hh"
#include "pixel_sort.hh"
#include "platform/file_system/path.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/matrix.hpp>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ApplicationVulkanDeferredPBRTest
 * @brief Runs the Vulkan deferred-rendering executable test.
 */
class ApplicationVulkanDeferredPBRTest : public ApplicationGame {
public:
    ApplicationVulkanDeferredPBRTest()
        : ApplicationGame(
              RendererCapabilities{
                  .shader_debug = true,
                  .ray_query = exec_test_requested("WBE_EXEC_TEST_RAY_TRACING"),
                  .timestamp_query = exec_test_requested("WBE_EXEC_TEST_TIMESTAMP_QUERY"),
                  // The Dear ImGui backend of the configuration window allocates classic descriptor sets.
                  .descriptor_set = true,
              },
              make_ref<InputManagerMocked>(global_allocator(), global_allocator())) {
    }

    virtual void logic_update(double p_delta) override;
    virtual void render_update(double p_delta) override;

    /**
     * @brief Creates the deferred-rendering resources, pipelines, and render graphs.
     */
    void setup_draw_task();

private:
    static constexpr VkFormat GBUFFER_ALBEDO_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr VkFormat GBUFFER_NORMAL_FORMAT = VK_FORMAT_R16G16_SNORM;
    static constexpr VkFormat GBUFFER_RMA_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr VkFormat GBUFFER_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
    static constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
    static constexpr VkFormat SHADOW_MOMENTS_FORMAT = VK_FORMAT_R32G32_SFLOAT;
    static constexpr VkFormat RSM_FLUX_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
    static constexpr VkFormat RSM_NORMAL_FORMAT = VK_FORMAT_R16G16_SNORM;
    static constexpr VkFormat LIGHTING_OUTPUT_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
    static constexpr uint32_t BLOOM_BLUR_PASS_COUNT = 4;
    static_assert(BLOOM_BLUR_PASS_COUNT % 2U == 0U);
    static constexpr double FRAME_SEQUENCE_FRAMES_PER_SECOND = 30.0;
    // Recorded frames not yet written before the rendering thread waits. The last recorded frame's readback is submitted
    // only by the next tick, so the bound must leave room for it and at least one submitted frame.
    static constexpr uint32_t FRAME_SEQUENCE_MAX_PENDING_FRAMES = 8;
    static_assert(FRAME_SEQUENCE_MAX_PENDING_FRAMES >= 2U);
    // The `script` resource of the graph recipe, res/assets/scripts/teal/render_graphs/deferred.tl.
    static constexpr std::string_view DEFERRED_RECIPE_SCRIPT = "render_graphs.deferred";
    static constexpr std::string_view DEFERRED_MIPMAP_RENDER_GRAPH = "deferred_pipeline_mipmap";
    static constexpr std::string_view DEFERRED_SUMMED_AREA_TABLE_RENDER_GRAPH = "deferred_pipeline_summed_area_table";
    static constexpr std::string_view GBUFFER_ALBEDO_IMAGE_VIEW = "gbuffer_albedo";
    static constexpr std::string_view GBUFFER_NORMAL_IMAGE_VIEW = "gbuffer_normal";
    static constexpr std::string_view GBUFFER_RMA_IMAGE_VIEW = "gbuffer_rma";
    static constexpr std::string_view GBUFFER_DEPTH_IMAGE_VIEW = "gbuffer_depth";
    static constexpr std::string_view SHADOWMAP_COLOR_IMAGE_VIEW = "shadowmap_color";
    static constexpr std::string_view RSM_NORMAL_IMAGE_VIEW = "rsm_normal";
    static constexpr std::string_view SHADOWMAP_DEPTH_IMAGE_VIEW = "shadowmap_depth";
    static constexpr std::string_view SHADOW_MOMENTS_IMAGE_VIEW = "shadow_moments";
    static constexpr std::string_view SHADOW_MOMENTS_SAMPLED_IMAGE_VIEW = "shadow_moments_sampled";
    static constexpr std::string_view SHADOW_MOMENTS_SUMMED_AREA_TABLE_HORIZONTAL_IMAGE_VIEW = "shadow_sat_horizontal";
    static constexpr std::string_view SHADOW_MOMENTS_SUMMED_AREA_TABLE_IMAGE_VIEW = "shadow_sat";
    static constexpr std::string_view DIRECT_LIGHTING_IMAGE_VIEW = "direct_lighting";
    static constexpr std::string_view INDIRECT_LIGHTING_IMAGE_VIEW = "indirect_lighting";
    static constexpr std::string_view RESULT_IMAGE_VIEW = "result_image";
    static constexpr std::string_view BLOOM_PING_IMAGE_VIEW = "bloom_ping";
    static constexpr std::string_view BLOOM_PONG_IMAGE_VIEW = "bloom_pong";
    static constexpr std::string_view FRAME_SEQUENCE_IMAGE_VIEW = "frame_sequence";
    static constexpr std::string_view GBUFFER_CAMERA_INFO_BUFFER = "gbuffer_camera_info";
    static constexpr std::string_view COMPOSITION_CAMERA_INFO_BUFFER = "composition_camera_info";
    // Task-local id of the shadow passes' light camera; each light's passes alias it to that light's buffer.
    static constexpr std::string_view SHADOW_LIGHT_INFO_BUFFER = "shadow_light_info";
    static constexpr std::string_view SPOT_LIGHT_INFO_BUFFER = "spot_light_infos";
    static constexpr std::string_view SHADOW_SAMPLER = "shadow_sampler";
    static constexpr std::string_view GBUFFER_SAMPLER = "gbuffer_sampler";
    static constexpr std::string_view BLOOM_SAMPLER = "bloom_sampler";
    static constexpr std::string_view KAWASE_BLUR_PING = "kawase_blur_ping";
    static constexpr std::string_view KAWASE_BLUR_PONG = "kawase_blur_pong";
    static constexpr std::string_view KAWASE_BLUR_SAMPLER = "kawase_blur_sampler";
    static constexpr std::string_view LIGHT_BILLBOARD_INFO_BUFFER = "light_billboard_info";
    static constexpr std::string_view LIGHT_BILLBOARD_TEXTURE_IMAGE_VIEW = "light_billboard_texture";
    static constexpr std::string_view LIGHT_BILLBOARD_SAMPLER = "light_billboard_sampler";

    static constexpr std::string_view RT_BUILD_MESH_ACCELERATION_STRUCTURE = "build_mesh_acceleration_structure";
    static constexpr std::string_view MESH_ACCELERATION_STRUCTURE_KEY = "mesh_acceleration_structure";
    static constexpr std::string_view RT_RUN_DEFERRED_GBUFFER_DOUBLE_SIDED = "run_gbuffer_double_sided";
    static constexpr std::string_view RT_RUN_DEFERRED_SHADOW_DOUBLE_SIDED = "run_shadow_double_sided";
    static constexpr std::string_view RT_RUN_DEFERRED_GBUFFER = "run_gbuffer";
    static constexpr std::string_view RT_RUN_DEFERRED_SHADOW_OPAQUE = "run_shadow_opaque";
    static constexpr std::string_view RT_RUN_DYNAMIC_SHADOW = "dynamic_shadow";
    static constexpr std::string_view RT_RUN_DYNAMIC_SHADOW_SECOND = "dynamic_shadow_second";
    static constexpr std::string_view RT_RUN_DYNAMIC_GBUFFER = "dynamic_gbuffer";
    static constexpr std::string_view RT_RUN_DYNAMIC_GBUFFER_SECOND = "dynamic_gbuffer_second";
    static constexpr std::string_view RT_GENERATE_SHADOW_MIPMAPS = "generate_shadow_mipmaps";
    static constexpr std::string_view RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE = "build_shadow_sat";
    static constexpr std::string_view RT_RUN_DIRECT_LIGHTING = "run_direct_lighting";
    static constexpr std::string_view RT_RUN_INDIRECT_LIGHTING = "run_indirect_lighting";
    static constexpr std::string_view RT_COMBINE_LIGHTING = "combine_lighting";
    static constexpr std::string_view RT_EXTRACT_BLOOM = "extract_bloom";
    static constexpr std::string_view RT_BLUR_BLOOM = "blur_bloom";
    static constexpr std::string_view RT_ACQUIRE_BLOOM = "acquire_bloom";
    static constexpr std::string_view RT_RUN_PRESENT_RESULT_IMAGE = "present_result_image";
    static constexpr std::string_view RT_RUN_PRESENT_FRAME_SEQUENCE = "present_frame_sequence";
    static constexpr std::string_view RT_ACQUIRE_FRAME_SEQUENCE = "acquire_frame_sequence";
    // Event ids of the RTTriggeredVK::Command that drives each graph's on-demand tasks, indexed like
    // deferred_render_graphs. Each graph has its own ids so a request never arms the inactive graph.
    static constexpr std::array<std::string_view, 2> BLOOM_ACQUIRE_EVENT_IDS = {
        "deferred_pbr_test.mipmap.bloom_acquire",
        "deferred_pbr_test.summed_area_table.bloom_acquire",
    };
    // The frame-sequence present and acquisition tasks share an id, so one command enables both on the same frames.
    static constexpr std::array<std::string_view, 2> FRAME_SEQUENCE_EVENT_IDS = {
        "deferred_pbr_test.mipmap.frame_sequence",
        "deferred_pbr_test.summed_area_table.frame_sequence",
    };
    static constexpr std::string_view RT_RUN_LIGHT_BILLBOARD = "run_light_billboard";
    // Event ids of the RTToggleVK::Command that gates the tasks of each voxel debug view mode group, shared by both graphs;
    // see is_voxel_debug_view_toggle_enabled(). The recipe receives them as VoxelDebugViewToggles.
    static constexpr std::array<std::string_view, 3> VOXEL_DEBUG_VIEW_TOGGLE_EVENT_IDS = {
        "deferred_pbr_test.voxel_debug_view.enabled",
        "deferred_pbr_test.voxel_debug_view.tree_view",
        "deferred_pbr_test.voxel_debug_view.pre_deferred_propagation",
    };
    // Event id of the RTMultiplexerVK::Command that selects the indirect lighting's branch in both graphs; see
    // get_voxel_debug_view_lighting_branch(). The recipe receives it as VoxelDebugViewToggles::lighting.
    static constexpr std::string_view VOXEL_DEBUG_VIEW_LIGHTING_EVENT_ID = "deferred_pbr_test.voxel_debug_view.lighting";
    // Event id of the RTMultiplexerVK::Command that selects the tree draw's branch in both graphs; see
    // get_voxel_debug_tree_draw_branch(). The recipe receives it as VoxelDebugViewToggles::tree_draw.
    static constexpr std::string_view VOXEL_DEBUG_TREE_DRAW_EVENT_ID = "deferred_pbr_test.voxel_debug_view.tree_draw";
    static constexpr std::string_view RT_DRAW_IMGUI = "draw_imgui";
    // RTTriggeredVK event of the voxel structure's static construction tasks in both graphs; see finish_voxel_static_build().
    static constexpr std::string_view VOXEL_STATIC_BUILD_EVENT_ID = "deferred_pbr_test.voxel_static_build";

    // Number of spot lights in the scene; T switches which one the light controls move.
    static constexpr uint32_t SPOT_LIGHT_COUNT = 2;
    static_assert(SPOT_LIGHT_COUNT >= 1U && SPOT_LIGHT_COUNT <= MAX_SPOT_LIGHT_INFO_COUNT);

    // Graph resource and task ids owned by one spot light. Every id is the shared base name suffixed with the light index.
    struct SpotLightGraphIds {
        std::string shadowmap_depth_image;
        std::string shadowmap_color_image;
        std::string rsm_normal_image;
        std::string shadow_moments_image;
        std::string shadow_moments_summed_area_table_horizontal_image;
        std::string shadow_moments_summed_area_table_image;
        std::string shadowmap_depth;
        std::string shadowmap_color;
        std::string rsm_normal;
        std::string shadow_moments;
        std::string shadow_moments_sampled;
        std::string shadow_moments_summed_area_table_horizontal;
        std::string shadow_moments_summed_area_table;
        std::string shadow_light_info;
        std::string light_billboard_info;
        std::string run_shadow_opaque;
        std::string run_shadow_double_sided;
        std::string run_dynamic_shadow;
        std::string run_dynamic_shadow_second;
        std::string generate_shadow_mipmaps;
        std::string build_shadow_moments_summed_area_table;
        std::string run_light_billboard;
    };

    struct CompositionCameraInfo {
        glm::mat4 inverse_view_projection;
        glm::vec4 camera_world_position;
    };

    void control(double p_delta);
    void update_descriptors();
    // Draw the configuration window, whose controls mirror the toggle shortcut keys. Runs on the rendering thread.
    void draw_configuration_window();
    // Draw the pixel sort options and apply their changes to both deferred graphs. Runs on the rendering thread.
    void draw_pixel_sort_options();

    void setup_render_context();
    void load_resources();
    void setup_static_scene_resources();
    void setup_spot_light();
    void setup_bloom_acquisition();
    void setup_light_billboard_overlay_resources();
    Ref<ROBufferVK> create_initialized_buffer(VkBufferUsageFlags p_buffer_usage, const void* p_data, size_t p_size) const;
    static SpotLightGraphIds make_spot_light_graph_ids(uint32_t p_light_index);
    // Advance the spot light the light controls move, as the T key does.
    void select_next_controlled_spot_light();
    void add_shadow_moments_summed_area_table_pipelines();
    void add_lighting_pipelines();
    // Register the meshes, pipelines, and images the application and its features draw with on p_graph, and compile the
    // static mesh's acceleration structure once. Called by every rebuild; repeated registrations are free.
    void register_graph_resources(RenderGraphVK& p_graph);
    void write_dynamic_camera_descriptors();
    void write_dynamic_light_descriptors();
    void register_deferred_render_graph(std::string_view p_graph_id, bool p_use_summed_area_table);
    // Run the deferred recipe for a graph context, then add the mesh and voxel parts that stay in C++.
    RendGraphDescVK make_deferred_render_graph_desc(
        bool p_use_summed_area_table, const RendGraphContextVK& p_context, RenderGraphVK& p_graph);
    // The recipe context, without the voxel structure and post-processing parameters.
    DeferredRecipeContext make_deferred_recipe_context(bool p_use_summed_area_table, const RendGraphContextVK& p_context) const;
    // The application-owned resources the recipe imports, and the tasks built in C++ that it imports.
    RendGraphRecipeImportsVK make_deferred_recipe_imports(bool p_use_summed_area_table);
    RendTaskDescVK create_mesh_acceleration_structure_build_task_desc();
    // Whether a voxel debug view toggle group records in a mode: 0 while the debug view is enabled, 1 in the
    // post-SVO-build and occupancy views, which draw the post-construction tree, and 2 in the pre-deferred-propagation view.
    static bool is_voxel_debug_view_toggle_enabled(size_t p_group, VoxelDebugViewMode p_mode);
    // The indirect lighting's RTMultiplexerVK branch in a mode: 0, the lit result, while the debug view is disabled, and
    // 1, the debug view, otherwise.
    static uint32_t get_voxel_debug_view_lighting_branch(VoxelDebugViewMode p_mode);
    // The tree draw's RTMultiplexerVK branch: the style's index, plus two in the occupancy view.
    static uint32_t get_voxel_debug_tree_draw_branch(VoxelDebugViewMode p_mode, VoxelDebugDrawStyle p_style);
    // The supported debug view mode after p_mode, in the order V cycles through them.
    VoxelDebugViewMode get_next_voxel_debug_view_mode(VoxelDebugViewMode p_mode) const;
    // Emit the toggle events of a changed voxel debug view mode. Runs on the rendering thread, between frames.
    void apply_voxel_debug_view_mode();
    // Disable the voxel static construction once a graph has recorded it, so neither the other graph nor a rebuild records
    // it again. Runs on the rendering thread, between frames.
    void finish_voxel_static_build();
    RendTaskDescVK create_frame_sequence_acquire_task_desc(bool p_use_summed_area_table);
    RendTaskDescVK create_imgui_task_desc();
    CompositionCameraInfo make_composition_camera_info() const;
    // Write a completed bloom acquisition. Runs on a worker thread.
    void write_completed_bloom_acquire(const RTCPUInFlightAcquireVK::AcquiredImage& p_image);
    // Start capturing frame_sequence_frame_count_input frames at a fixed delta. Runs on the rendering thread.
    void start_frame_sequence();
    // Bound the frames waiting to be written, and end the fixed delta after the last frame is recorded. Runs on the
    // rendering thread.
    void poll_frame_sequence();
    // Write one acquired sequence frame. Runs on a worker thread; takes a copy because it converts the pixels in place.
    void write_frame_sequence_frame(RTCPUInFlightAcquireVK::AcquiredImage p_image);
    bool is_frame_sequence_in_progress() const;

    Unique<DynamicMeshRenderFeature> dynamic_mesh;
    double dynamic_mesh_time = 0.0;
    // The SVO or octree selected by WBE_SVO_IMPLEMENTATION.
    Unique<IVoxelRenderFeature> voxel_structure;
    // Applied in place to the result image after lighting composition; toggled with the 1 key.
    Unique<PixelSort> post_processing;
    // The mipmap and the summed-area table graph, indexed by whether they filter shadows with the summed-area table.
    std::array<Ref<RenderGraphVK>, 2> deferred_render_graphs;
    RendererVK* renderer_vk = nullptr;
    const RenderContextVK* render_context = nullptr;
    RenderAllocator* render_allocator = nullptr;
    RefRes<RUMesh3D> sponza_mesh;
    // The static scene mesh, registered on both graphs under STATIC_MESH_KEY; the voxel features and the acceleration
    // structure share the same compiled mesh, which MeshCompilerVK caches for this description.
    RDParamsMeshVK static_mesh_params;
    Ref<MeshResourcesVK> static_mesh_resources = MEM_NULL;
    Ref<MeshAccelerationStructureVK> mesh_acceleration_structure = MEM_NULL;
    // The pipelines the application's own passes use, registered on both graphs.
    RenderTestRegistrations scene_registrations;
    Ref<ROCMultislotsVK> gbuffer_camera_info_ubos;
    Ref<ROCMultislotsVK> composition_camera_info_ubos;
    Ref<ROCMultislotsVK> spot_light_info_buffers;
    // One light camera per spot light, read by that light's shadow passes as shadow_light_info.
    std::array<Ref<ROCMultislotsVK>, SPOT_LIGHT_COUNT> shadow_light_info_buffers;
    std::array<Ref<ROBufferVK>, SPOT_LIGHT_COUNT> light_billboard_ubos;

    std::array<ComponentSpotLight, SPOT_LIGHT_COUNT> spot_light_components{};
    std::array<float, SPOT_LIGHT_COUNT> spot_light_yaws{};
    std::array<float, SPOT_LIGHT_COUNT> spot_light_pitches{};
    // Index of the spot light moved by the light controls. Written by the input thread and the configuration window.
    std::atomic<uint32_t> controlled_spot_light = 0;
    VkExtent2D shadow_extent{};
    uint32_t shadow_mipmap_level_count = 1;
    VkExtent2D bloom_extent{};
    // Input-thread mailbox; render_update forwards requests to the active graph's wrapper.
    std::atomic_bool bloom_acquire_requested = false;
    // Rendering-thread mailbox of the configuration window's bloom button; control() handles it like a B key press.
    std::atomic_bool bloom_acquire_button_pressed = false;
    bool bloom_acquire_key_was_down = false;
    // Set by control() when it requests an acquisition; cleared by the worker after the image is written.
    std::atomic_bool bloom_acquire_in_flight = false;
    // Frame sequence capture. The rendering thread owns everything except the written count, which workers advance.
    Path frame_sequence_directory;
    DeltaSettings frame_sequence_previous_delta_settings{};
    uint64_t frame_sequence_first_frame = 0;
    uint32_t frame_sequence_frame_count = 0;
    std::atomic<uint32_t> frame_sequence_written_count = 0;
    int frame_sequence_frame_count_input = 60;
    bool frame_sequence_recording = false;
    bool frame_sequence_requested = false;
    bool render_graph_switch_key_was_down = false;
    bool spot_light_switch_key_was_down = false;
    bool voxel_debug_view_key_was_down = false;
    bool frame_capture_key_was_down = false;
    bool post_processing_key_was_down = false;
    // Input-thread mailbox; render_update applies it to the post-processing between frames.
    std::atomic_bool post_processing_requested = false;
    std::atomic_bool frame_capture_requested = false;
    std::atomic<bool> summed_area_table_requested = false;
    std::atomic<VoxelDebugViewMode> voxel_debug_view_mode = VoxelDebugViewMode::DISABLED;
    // The mode the toggled tasks follow; owned by the rendering thread.
    VoxelDebugViewMode applied_voxel_debug_view_mode = VoxelDebugViewMode::DISABLED;
    // The draw style the configuration window requests, and the one the rendering thread applied last; applied together with
    // the mode, so the tree draw's branch and the voxel structure's settings change on the same frame.
    std::atomic<VoxelDebugDrawStyle> voxel_debug_draw_style = VoxelDebugDrawStyle::WIREFRAME;
    VoxelDebugDrawStyle applied_voxel_debug_draw_style = VoxelDebugDrawStyle::WIREFRAME;
    // Whether the voxel static construction has yet to record, and the frame the deferred graphs were registered on; owned
    // by the rendering thread.
    bool voxel_static_build_pending = true;
    uint64_t voxel_static_build_registration_frame = 0;
    bool summed_area_table_active = false;
    // Draws the configuration window into the swap chain after every other pass. Destroyed before the renderer, after
    // the rendering thread has stopped.
    Unique<ImGuiOverlayVK> imgui_overlay;
    ComponentCamera camera_component{};
    glm::vec3 camera_position = TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = default_test_camera_yaw();
    float camera_pitch = default_test_camera_pitch();
    // Deliver completed bloom and frame-sequence acquisitions. Declared last, so they are destroyed first and no handler
    // runs against members that are already destroyed.
    EventRegistration bloom_acquire_registration;
    EventRegistration frame_sequence_acquire_registration;
};

} // namespace WhiteBirdEngine

#endif
