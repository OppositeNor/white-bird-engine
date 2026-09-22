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
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/rt_run_static_set_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <atomic>
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
#include <semaphore>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ApplicationVulkanDeferredPBRTest
 * @brief Runs the Vulkan deferred-rendering executable test.
 */
class ApplicationVulkanDeferredPBRTest : public ApplicationGame {
public:
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
    static constexpr uint32_t BLOOM_DOWNSAMPLE_LEVEL = 4;
    static constexpr size_t BLOOM_BYTES_PER_PIXEL = sizeof(uint16_t) * 4;
    static constexpr size_t OCT_TREE_MAX_NODE_COUNT = 1024;
    static constexpr size_t OCT_TREE_MAX_LEAVES_COUNT = 1024;
    static constexpr size_t OCT_TREE_MAX_DEFERRED_TASK_COUNT = 1024;
    static constexpr size_t OCT_TREE_MAX_DEPTH = 16;
    static constexpr float OCT_TREE_WIDTH = 100.0F;
    static constexpr glm::vec3 OCT_TREE_ORIGIN = glm::vec3(0.0);

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
    static constexpr std::string_view BLOOM_EXTRACT_IMAGE_VIEW = "bloom_extract";
    static constexpr std::string_view BLOOM_DOWNSAMPLED_IMAGE_VIEW = "bloom_downsampled";
    static constexpr std::string_view BLOOM_HORIZONTAL_IMAGE_VIEW = "bloom_horizontal";
    static constexpr std::string_view BLOOM_IMAGE_VIEW = "bloom";
    static constexpr std::string_view GBUFFER_CAMERA_INFO_BUFFER = "gbuffer_camera_info";
    static constexpr std::string_view COMPOSITION_CAMERA_INFO_BUFFER = "composition_camera_info";
    static constexpr std::string_view SHADOW_LIGHT_INFO_BUFFER = "shadow_light_info";
    static constexpr std::string_view SPOT_LIGHT_INFO_BUFFER = "spot_light_infos";
    static constexpr std::string_view SPOT_LIGHT_COUNT_BUFFER = "spot_light_count";
    static constexpr std::string_view SHADOW_SAMPLER = "shadow_sampler";
    static constexpr std::string_view GBUFFER_SAMPLER = "gbuffer_sampler";
    static constexpr std::string_view BLOOM_SAMPLER = "bloom_sampler";
    static constexpr std::string_view LIGHT_BILLBOARD_INFO_BUFFER = "light_billboard_info";
    static constexpr std::string_view LIGHT_BILLBOARD_TEXTURE_IMAGE_VIEW = "light_billboard_texture";
    static constexpr std::string_view LIGHT_BILLBOARD_SAMPLER = "light_billboard_sampler";

    static constexpr std::string_view OCT_TREE_INFO = "oct_tree_info";
    static constexpr std::string_view OCT_TREE_NODE_BUFFER = "oct_tree_node_buffer";
    static constexpr std::string_view OCT_TREE_LEAF_BUFFER = "oct_tree_leaves_buffer";
    static constexpr std::string_view OCT_TREE_DEFERRED_LIST_PING = "oct_tree_deferred_list_ping";
    static constexpr std::string_view OCT_TREE_DEFERRED_LIST_PONG = "oct_tree_deferred_list_pong";

    static constexpr std::string_view RT_RUN_DEFERRED_GBUFFER = "run_gbuffer";
    static constexpr std::string_view RT_RUN_DEFERRED_SHADOW_OPAQUE = "run_shadow_opaque";
    static constexpr std::string_view RT_GENERATE_SHADOW_MIPMAPS = "generate_shadow_mipmaps";
    static constexpr std::string_view RT_BUILD_SHADOW_MOMENTS_SUMMED_AREA_TABLE = "build_shadow_sat";
    static constexpr std::string_view RT_RUN_DIRECT_LIGHTING = "run_direct_lighting";
    static constexpr std::string_view RT_RUN_INDIRECT_LIGHTING = "run_indirect_lighting";
    static constexpr std::string_view RT_COMBINE_LIGHTING = "combine_lighting";
    static constexpr std::string_view RT_EXTRACT_BLOOM = "extract_bloom";
    static constexpr std::string_view RT_GENERATE_BLOOM_MIPMAPS = "generate_bloom_mipmaps";
    static constexpr std::string_view RT_BLUR_BLOOM = "blur_bloom";
    static constexpr std::string_view RT_ACQUIRE_BLOOM = "acquire_bloom";
    static constexpr std::string_view RT_RUN_PRESENT_RESULT_IMAGE = "present_result_image";
    static constexpr std::string_view RT_RUN_LIGHT_BILLBOARD = "run_light_billboard";

    static constexpr std::string_view RT_RUN_BUILD_OCT_TREE = "run_build_oct_tree";

    struct CompositionCameraInfo {
        glm::mat4 inverse_view_projection;
        glm::vec4 camera_world_position;
    };

    struct OctTreeInfo {
        glm::mat4 matrices[3];
        glm::vec3 origin;
        float width;
        uint32_t max_depth;
        uint32_t node_count;
        uint32_t leaves_count;
        uint32_t data_count;
        uint32_t deferred_task_count;
    };

    struct OctTreeNode {
        enum class ChildrenState {
            NODE,
            LEAF,
            EMPTY,
            ALLOCATING
        };
        enum Subregion {
            XYZ_000,
            XYZ_001,
            XYZ_010,
            XYZ_011,
            XYZ_100,
            XYZ_101,
            XYZ_110,
            XYZ_111,
            MAX_SUBREGION
        };
        uint32_t children_state;
        uint32_t children;
        uint32_t data_index;
        int32_t parent_index;
    };

    struct OctTreeLeaf {
        uint32_t data_index;
        int32_t parent_index;
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
    void create_oct_tree_resources();
    void setup_light_billboard_overlay_resources();
    Ref<ROBufferVK> create_initialized_buffer(VkBufferUsageFlags p_buffer_usage, const void* p_data, size_t p_size) const;
    ComputePipelineCompilerVKContext make_compute_pipeline_context() const;
    GraphicsPipelineCompilerVKContext make_present_graphics_pipeline_context() const;
    Unique<RTRunStaticSetVK> compile_shadow_task(
        RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_pipeline, bool p_load_existing);
    Unique<RTRunStaticSetVK> compile_gbuffer_task(
        RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_pipeline, bool p_load_existing);
    void compile_shadow_moments_summed_area_table_pipelines();
    void compile_lighting_pipelines();
    void write_dynamic_camera_descriptors();
    void write_dynamic_light_descriptors();
    void register_deferred_render_graph(std::string_view p_graph_id,
        bool p_use_summed_area_table,
        Unique<RTRunStaticSetVK>& p_shadow_opaque_task,
        Unique<RTRunStaticSetVK>& p_gbuffer_opaque_task);
    void add_render_graph_resources(RendGraphDescVK& p_pipeline_desc) const;
    RendTaskDescVK create_shadow_mipmap_task_desc() const;
    RendTaskDescVK create_shadow_moments_summed_area_table_task_desc() const;
    RendTaskDescVK create_direct_lighting_task_desc(bool p_use_summed_area_table) const;
    RendTaskDescVK create_indirect_lighting_task_desc(bool p_use_summed_area_table) const;
    RendTaskDescVK create_combine_lighting_task_desc() const;
    RendTaskDescVK create_bloom_extract_task_desc() const;
    RendTaskDescVK create_bloom_mipmap_task_desc() const;
    RendTaskDescVK create_bloom_blur_task_desc() const;
    RendTaskDescVK create_bloom_acquire_task_desc();
    RendTaskDescVK create_present_result_image_task_desc() const;
    RendTaskDescVK create_light_billboard_task_desc() const;
    RendTaskDescVK create_oct_tree_build_task_desc() const;
    CompositionCameraInfo make_composition_camera_info() const;
    void write_completed_bloom_acquire();

    RendererVK* renderer_vk = nullptr;
    const BaseRenderDataVK* base_render_data = nullptr;
    RenderAllocator* render_allocator = nullptr;
    RefRes<RUStaticOpaqueSet> sponza_opaque_set;
    RefRes<RUGraphicsPipeline> shadow_opaque_pipeline_resource;
    RefRes<RUGraphicsPipeline> gbuffer_opaque_pipeline_resource;
    RefRes<RUShader> shadow_moments_summed_area_table_horizontal_shader_resource;
    RefRes<RUShader> shadow_moments_summed_area_table_vertical_shader_resource;
    RefRes<RUShader> direct_lighting_shader_resource;
    RefRes<RUShader> direct_lighting_summed_area_table_shader_resource;
    RefRes<RUShader> indirect_lighting_rsm_shader_resource;
    RefRes<RUShader> combine_lighting_shader_resource;
    RefRes<RUShader> bloom_extract_shader_resource;
    RefRes<RUShader> bloom_horizontal_shader_resource;
    RefRes<RUShader> bloom_vertical_shader_resource;
    RefRes<RUShader> oct_tree_build_x_shader_resource;
    RefRes<RUShader> oct_tree_build_y_shader_resource;
    RefRes<RUShader> oct_tree_build_z_shader_resource;
    RefRes<RUGraphicsPipeline> present_post_processing_pipeline_resource;

    Ref<ROComputePipelineVK> shadow_moments_summed_area_table_horizontal_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> shadow_moments_summed_area_table_vertical_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> direct_lighting_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> direct_lighting_summed_area_table_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> indirect_lighting_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> combine_lighting_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> bloom_extract_compute_pipeline = MEM_NULL;
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
    Ref<ROImageVK> result_image;
    Ref<ROImageVK> bloom_extract_image;
    Ref<ROImageVK> bloom_horizontal_image;
    Ref<ROImageVK> bloom_image;
    Ref<ROImageViewVK> gbuffer_albedo_view;
    Ref<ROImageViewVK> gbuffer_normal_view;
    Ref<ROImageViewVK> gbuffer_rma_view;
    Ref<ROImageViewVK> gbuffer_depth_view;
    Ref<ROImageViewVK> direct_lighting_view;
    Ref<ROImageViewVK> indirect_lighting_view;
    Ref<ROImageViewVK> result_image_view;
    Ref<ROImageViewVK> bloom_extract_image_view;
    Ref<ROImageViewVK> bloom_downsampled_image_view;
    Ref<ROImageViewVK> bloom_horizontal_image_view;
    Ref<ROImageViewVK> bloom_image_view;
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
    Ref<ROSamplerVK> bloom_sampler;
    Ref<ROCMultislotsVK> gbuffer_camera_info_ubos;
    Ref<ROCMultislotsVK> composition_camera_info_ubos;
    Ref<ROCMultislotsVK> spot_light_info_buffers;
    Ref<ROBufferVK> spot_light_count_buffer;
    Ref<ROBufferVK> light_billboard_ubo;
    Ref<ROImageViewVK> light_billboard_texture_view;
    Ref<ROSamplerVK> light_billboard_sampler;

    Ref<ROBufferVK> oct_tree_info;
    Ref<ROBufferVK> oct_tree_nodes;
    Ref<ROBufferVK> oct_tree_leaves;
    Ref<ROBufferVK> oct_tree_deferred_list_ping;
    Ref<ROBufferVK> oct_tree_deferred_list_pong;
    ComponentSpotLight spot_light_component{};
    float spot_light_yaw = 0.0F;
    float spot_light_pitch = 0.0F;
    VkExtent2D shadow_extent{};
    uint32_t shadow_mipmap_level_count = 1;
    VkExtent2D bloom_extent{};
    uint32_t bloom_mipmap_level_count = 1;
    std::vector<uint8_t> bloom_acquire_buffer;
    std::atomic_bool bloom_acquire_requested = false;
    std::binary_semaphore bloom_acquire_completed{0};
    bool bloom_acquire_key_was_down = false;
    bool bloom_acquire_in_flight = false;
    bool render_graph_switch_key_was_down = false;
    std::atomic<bool> summed_area_table_requested = false;
    bool summed_area_table_active = false;
    ComponentCamera camera_component{};
    glm::vec3 camera_position = TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = default_test_camera_yaw();
    float camera_pitch = default_test_camera_pitch();
};

} // namespace WhiteBirdEngine

#endif
