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
#ifndef WBE_FILE_OCTREE_RENDER_FEATURE_HH
#define WBE_FILE_OCTREE_RENDER_FEATURE_HH

#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/static_set_resources_vk.hh"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <glm/vec3.hpp>
#include <string_view>

namespace WhiteBirdEngine {

class DynamicMeshRenderFeature;

/**
 * @enum VoxelDebugViewMode
 * @brief Selects the octree construction state visualized by the deferred rendering test.
 */
enum class VoxelDebugViewMode : uint32_t {
    DISABLED,
    POST_OCTREE_BUILD,
    PRE_DEFERRED_PROPAGATION,
};

/**
 * @class OctreeRenderFeature
 * @brief Owns the deferred test's interchangeable octree construction and debug rendering resources.
 * @details Neither copyable nor movable. All methods and destruction are confined to the rendering thread.
 * Registered graphs retain their resource handles and task factories; borrowed renderer and debug state must remain alive.
 */
class OctreeRenderFeature {
public:
    /**
     * @brief Selects the original atomic traversal or pass-published octree_2 shaders.
     */
    enum class Implementation {
        ATOMIC,
        PASS_PUBLISHED
    };

    OctreeRenderFeature() = delete;
    /**
     * @brief Creates pipelines and buffers on the rendering thread.
     * @details The renderer, allocator, and debug-mode state must outlive this object and its registered graphs.
     * The feature retains shared ownership of the static scene. Invalid dependencies throw.
     */
    OctreeRenderFeature(RendererVK* p_renderer,
        Ref<StaticSetResourcesVK> p_static_set_resources,
        const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
        Implementation p_implementation);
    ~OctreeRenderFeature() = default;
    OctreeRenderFeature(const OctreeRenderFeature&) = delete;
    OctreeRenderFeature(OctreeRenderFeature&&) = delete;
    OctreeRenderFeature& operator=(const OctreeRenderFeature&) = delete;
    OctreeRenderFeature& operator=(OctreeRenderFeature&&) = delete;

    /**
     * @brief Registers all implementation-owned buffers with their stable shader resource names.
     */
    void register_render_graph_data(RendGraphDescVK& p_graph) const;
    /**
     * @brief Adds reset, static and dynamic raster draws, initial publication when required, and pre-deferred capture.
     */
    void add_construction_render_tasks(
        RendGraphDescVK& p_graph, std::string_view p_previous_task_id, const DynamicMeshRenderFeature& p_dynamic_mesh) const;
    /**
     * @brief Creates the per-frame reset ordered after the supplied graph task.
     */
    RendTaskDescVK get_reset_render_task_desc(std::string_view p_previous_task_id) const;
    /**
     * @brief Creates one raster construction pass over the retained scene.
     */
    RendTaskDescVK get_construction_render_task_desc(std::string_view p_task_id,
        std::string_view p_previous_task_id,
        bool p_double_sided = false,
        const DynamicMeshRenderFeature* p_dynamic_mesh = nullptr) const;
    /**
     * @brief Creates bounded deferred retries, including publication after every iteration for octree_2.
     */
    RendTaskDescVK get_deferred_construction_render_task_desc(bool p_dynamic = true) const;
    /**
     * @brief Creates indirect debug-draw setup after construction is complete.
     */
    RendTaskDescVK get_debug_draw_setup_render_task_desc() const;
    /**
     * @brief Creates the completed-tree overlay after the supplied presentation task.
     */
    RendTaskDescVK get_debug_draw_render_task_desc(std::string_view p_previous_task_id) const;
    /**
     * @brief Creates the pre-deferred overlay after the completed-tree overlay task.
     */
    RendTaskDescVK get_pre_deferred_debug_draw_render_task_desc() const;
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RUN_BUILD_OCTREE_DOUBLE_SIDED = "run_build_octree_double_sided";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RESET_OCTREE = "reset_octree";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RUN_BUILD_OCTREE = "run_build_octree";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RUN_BUILD_OCTREE_DEFERRED = "run_build_octree_deferred";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_CAPTURE_OCTREE_PRE_DEFERRED_DEBUG = "capture_octree_pre_deferred_debug";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_SETUP_OCTREE_DEBUG_DRAW = "setup_octree_debug_draw";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RUN_OCTREE_DEBUG_DRAW = "run_octree_debug_draw";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW = "run_octree_pre_deferred_debug_draw";

private:
    void create_resources();
    void initialize();
    void compile_pipelines();
    Ref<ROBufferVK> create_initialized_buffer(VkBufferUsageFlags p_usage, const void* p_data, size_t p_size) const;
    RendTaskDescVK get_state_update_render_task_desc(std::string_view p_previous_task_id) const;
    RendTaskDescVK get_debug_capture_render_task_desc(
        std::string_view p_task_id, std::string_view p_previous_task_id, VoxelDebugViewMode p_mode) const;
    static constexpr std::string_view RT_UPDATE_OCTREE_STATES = "update_octree_states";
    static constexpr std::string_view OCTREE_VOXEL_CLAIMS = "octree_voxel_claims";
    static constexpr std::string_view OCTREE_PENDING_CHILDREN = "octree_pending_children";
    static constexpr std::string_view GBUFFER_DEPTH_IMAGE_VIEW = "gbuffer_depth";
    static constexpr std::string_view GBUFFER_CAMERA_INFO_BUFFER = "gbuffer_camera_info";
    static constexpr VkFormat GBUFFER_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
    static constexpr size_t OCTREE_MAX_DEPTH = 9;
    static constexpr size_t OCTREE_EXTENT = 1 << OCTREE_MAX_DEPTH;
    static constexpr size_t OCTREE_VOXEL_COUNT = OCTREE_EXTENT * OCTREE_EXTENT * OCTREE_EXTENT;
    static constexpr size_t OCTREE_MAX_LEAVES_COUNT = OCTREE_VOXEL_COUNT / 8;
    static constexpr size_t OCTREE_MAX_NODE_COUNT = OCTREE_VOXEL_COUNT / 64;
    static constexpr size_t OCTREE_MAX_DYNAMIC_NODE_COUNT = OCTREE_MAX_NODE_COUNT;
    static constexpr size_t OCTREE_MAX_DYNAMIC_LEAVES_COUNT = OCTREE_MAX_LEAVES_COUNT;
    static constexpr size_t OCTREE_MAX_DEFERRED_TASK_COUNT = OCTREE_MAX_NODE_COUNT * 5 / 2;
    // Matches deferred propagation and debug-capture shaders; keeps the expanded lists within 65535 workgroups.
    static constexpr uint32_t OCTREE_DEFERRED_THREADS_PER_GROUP = 128;
    static_assert((OCTREE_MAX_DEFERRED_TASK_COUNT + OCTREE_DEFERRED_THREADS_PER_GROUP - 1) / OCTREE_DEFERRED_THREADS_PER_GROUP <= 65535);
    static constexpr size_t OCTREE_CLAIM_WORD_COUNT = (OCTREE_VOXEL_COUNT + 31U) / 32U;
    // A voxel can hold at most one unreturned eight-slot ticket; include transient overflow reservations.
    static_assert(OCTREE_VOXEL_COUNT <= (0xffffffffU - OCTREE_MAX_NODE_COUNT) / 8U);
    static_assert(OCTREE_VOXEL_COUNT <= (0xffffffffU - OCTREE_MAX_LEAVES_COUNT) / 8U);
    static constexpr float OCTREE_WIDTH = 32.0F;
    static constexpr glm::vec3 OCTREE_ORIGIN = glm::vec3(-16.0F, -8.0F, -16.0F);

    static constexpr std::string_view OCTREE_DYNAMIC_NODES = "octree_dynamic_nodes";
    static constexpr std::string_view OCTREE_DYNAMIC_LEAVES = "octree_dynamic_leaves";
    static constexpr std::string_view OCTREE_INFO = "octree_info";
    static constexpr std::string_view OCTREE_NODE_BUFFER = "octree_node_buffer";
    static constexpr std::string_view OCTREE_LEAF_BUFFER = "octree_leaves_buffer";
    static constexpr std::string_view OCTREE_DEFERRED_LIST_PING = "octree_deferred_list_ping";
    static constexpr std::string_view OCTREE_DEFERRED_LIST_PONG = "octree_deferred_list_pong";
    static constexpr std::string_view OCTREE_DEBUG_DRAW_INDEX_BUFFER = "octree_debug_draw_index_buffer";
    static constexpr std::string_view OCTREE_DEBUG_DRAW_INDIRECT_BUFFER = "octree_debug_draw_indirect_buffer";
    static constexpr std::string_view OCTREE_PRE_DEFERRED_DEBUG_INFO_BUFFER = "octree_pre_deferred_debug_info_buffer";
    static constexpr std::string_view OCTREE_PRE_DEFERRED_DEBUG_LIST_BUFFER = "octree_pre_deferred_debug_list_buffer";
    static constexpr std::string_view OCTREE_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER = "octree_pre_deferred_debug_draw_indirect_buffer";

    struct OctreeInfo {
        glm::vec3 origin;
        float width;
        uint32_t max_depth;
        uint32_t max_node_count;
        uint32_t max_leaves_count;
        uint32_t node_count;
        uint32_t leaves_count;
        uint32_t data_count;
        uint32_t deferred_task_count_ping;
        uint32_t deferred_task_count_pong;
        uint32_t max_deferred_task_count;
        uint32_t max_dynamic_node_count;
        uint32_t max_dynamic_leaves_count;
        uint32_t dynamic_node_count;
        uint32_t dynamic_leaves_count;
    };

    static_assert(sizeof(OctreeInfo) == 68);
    static_assert(offsetof(OctreeInfo, dynamic_node_count) == 60);

    struct OctreeNode {
        enum class ChildrenState {
            NODE = 0,
            EMPTY = 1,
            ALLOCATING = 2,
            LEAF = 3,
            ALLOCATING_DYNAMIC = 4,
            NODE_DYNAMIC = 5,
            LEAF_DYNAMIC = 6
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
        // Mirrors the packed Slang description: state 31:29; leaf occupancy 28:21/block 20:0;
        // nodes use block 20:0 with bits 28:21 unused. Block indices address eight entries.
        uint32_t children_data;
        uint32_t data_index;
        int32_t parent_index;
        uint32_t original_children_data;
    };

    static_assert(sizeof(OctreeNode) == 16);
    static_assert(alignof(OctreeNode) == alignof(uint32_t));
    static_assert(offsetof(OctreeNode, data_index) == 4);
    static_assert(offsetof(OctreeNode, parent_index) == 8);
    static_assert(OCTREE_MAX_DEPTH >= 2);
    static_assert(OCTREE_MAX_NODE_COUNT % 8 == 0 && OCTREE_MAX_NODE_COUNT <= (1U << 21U) * 8U);
    static_assert(OCTREE_MAX_LEAVES_COUNT % 8 == 0 && OCTREE_MAX_LEAVES_COUNT <= (1U << 21U) * 8U);

    struct OctreeLeaf {
        uint32_t data_index;
        int32_t parent_index;
    };

    struct OctreeDynamicLeafBlock {
        uint32_t original_children_data;
        OctreeLeaf leaves[8];
    };
    static_assert(OCTREE_MAX_DYNAMIC_LEAVES_COUNT % 8 == 0 && OCTREE_MAX_DYNAMIC_LEAVES_COUNT <= (1U << 21U) * 8U);
    static_assert(sizeof(OctreeDynamicLeafBlock) == 68);
    static_assert(offsetof(OctreeDynamicLeafBlock, leaves) == 4);
    static_assert(offsetof(OctreeNode, original_children_data) == 12);
    static_assert(OCTREE_MAX_NODE_COUNT + OCTREE_MAX_DYNAMIC_NODE_COUNT <= (1U << 21U) * 8U);

    RendTaskDescVK get_dynamic_clear_task_desc(std::string_view p_previous_task_id) const;
    RendTaskDescVK get_dynamic_publication_task_desc(std::string_view p_previous_task_id) const;
    RendTaskDescVK make_static_once(RendTaskDescVK p_task, bool p_finish = false) const;

    struct OctreeDeferredListEntry {
        uint32_t node_index;
        uint32_t current_depth;
        int32_t remaining_depth;
        alignas(16) glm::vec3 octree_position;
    };

    static_assert(offsetof(OctreeDeferredListEntry, octree_position) == 16);
    static_assert(sizeof(OctreeDeferredListEntry) == 32);

    RendererVK* renderer_vk;
    const BaseRenderDataVK* base_render_data;
    RenderAllocator* render_allocator;
    Ref<StaticSetResourcesVK> static_set_resources;
    const std::atomic<VoxelDebugViewMode>* debug_view_mode;
    Implementation implementation;
    // Render-thread completion shared by all graph variants using these buffers.
    Ref<bool> static_build_completed;
    Ref<ROBufferVK> dynamic_nodes;
    Ref<ROBufferVK> dynamic_leaves;
    Ref<ROGraphicsPipelineVK> dynamic_build_pipeline;
    Ref<ROComputePipelineVK> dynamic_clear_pipeline;
    Ref<ROComputePipelineVK> dynamic_publication_pipeline;
    Ref<ROComputePipelineVK> dynamic_ping_pipeline;
    Ref<ROComputePipelineVK> dynamic_pong_pipeline;
    Ref<ROBufferVK> pending_children;
    Ref<ROBufferVK> voxel_claims;
    Ref<ROComputePipelineVK> update_states_pipeline;
    Ref<ROGraphicsPipelineVK> octree_build_graphics_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> octree_reset_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> octree_deferred_count_reset_ping_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> octree_deferred_count_reset_pong_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> octree_deferred_propagation_ping_write_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> octree_deferred_propagation_pong_write_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> octree_pre_deferred_debug_capture_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> octree_debug_draw_setup_compute_pipeline = MEM_NULL;
    Ref<ROGraphicsPipelineVK> octree_debug_draw_graphics_pipeline = MEM_NULL;
    Ref<ROGraphicsPipelineVK> octree_pre_deferred_debug_draw_graphics_pipeline = MEM_NULL;
    Ref<ROBufferVK> octree_info;
    Ref<ROBufferVK> octree_nodes;
    Ref<ROBufferVK> octree_leaves;
    Ref<ROBufferVK> octree_deferred_list_ping;
    Ref<ROBufferVK> octree_deferred_list_pong;
    Ref<ROBufferVK> octree_debug_draw_index_buffer;
    Ref<ROBufferVK> octree_debug_draw_indirect_buffer;
    Ref<ROBufferVK> octree_pre_deferred_debug_info_buffer;
    Ref<ROBufferVK> octree_pre_deferred_debug_list_buffer;
    Ref<ROBufferVK> octree_pre_deferred_debug_draw_indirect_buffer;
};

} // namespace WhiteBirdEngine

#endif
