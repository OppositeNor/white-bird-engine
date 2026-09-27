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
#ifndef WBE_FILE_OCT_TREE_RENDER_FEATURE_HH
#define WBE_FILE_OCT_TREE_RENDER_FEATURE_HH

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

/**
 * @enum VoxelDebugViewMode
 * @brief Selects the oct-tree construction state visualized by the deferred rendering test.
 */
enum class VoxelDebugViewMode : uint32_t {
    DISABLED,
    POST_OCT_TREE_BUILD,
    PRE_DEFERRED_PROPAGATION,
};

/**
 * @class OctTreeRenderFeature
 * @brief Owns the deferred test's interchangeable octree construction and debug rendering resources.
 * @details Neither copyable nor movable. All methods and destruction are confined to the rendering thread.
 * Registered graphs retain their resource handles and task factories; borrowed renderer and debug state must remain alive.
 */
class OctTreeRenderFeature {
public:
    /**
     * @brief Selects the original atomic traversal or pass-published oct_tree_2 shaders.
     */
    enum class Implementation {
        ATOMIC,
        PASS_PUBLISHED
    };

    OctTreeRenderFeature() = delete;
    /**
     * @brief Creates pipelines and buffers on the rendering thread.
     * @details The renderer, allocator, and debug-mode state must outlive this object and its registered graphs.
     * The feature retains shared ownership of the static scene. Invalid dependencies throw.
     */
    OctTreeRenderFeature(RendererVK* p_renderer,
        Ref<StaticSetResourcesVK> p_static_set_resources,
        const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
        Implementation p_implementation);
    ~OctTreeRenderFeature() = default;
    OctTreeRenderFeature(const OctTreeRenderFeature&) = delete;
    OctTreeRenderFeature(OctTreeRenderFeature&&) = delete;
    OctTreeRenderFeature& operator=(const OctTreeRenderFeature&) = delete;
    OctTreeRenderFeature& operator=(OctTreeRenderFeature&&) = delete;

    /**
     * @brief Registers all implementation-owned buffers with their stable shader resource names.
     */
    void register_render_graph_data(RendGraphDescVK& p_graph) const;
    /**
     * @brief Adds reset, both raster draws, initial publication when required, and pre-deferred capture.
     */
    void add_construction_render_tasks(RendGraphDescVK& p_graph, std::string_view p_previous_task_id) const;
    /**
     * @brief Creates the per-frame reset ordered after the supplied graph task.
     */
    RendTaskDescVK get_reset_render_task_desc(std::string_view p_previous_task_id) const;
    /**
     * @brief Creates one raster construction pass over the retained scene.
     */
    RendTaskDescVK get_construction_render_task_desc(
        std::string_view p_task_id, std::string_view p_previous_task_id, bool p_double_sided = false) const;
    /**
     * @brief Creates bounded deferred retries, including publication after every iteration for oct_tree_2.
     */
    RendTaskDescVK get_deferred_construction_render_task_desc() const;
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
    static constexpr std::string_view RT_RUN_BUILD_OCT_TREE_DOUBLE_SIDED = "run_build_oct_tree_double_sided";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RESET_OCT_TREE = "reset_oct_tree";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RUN_BUILD_OCT_TREE = "run_build_oct_tree";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RUN_BUILD_OCT_TREE_DEFERRED = "run_build_oct_tree_deferred";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_CAPTURE_OCT_TREE_PRE_DEFERRED_DEBUG = "capture_oct_tree_pre_deferred_debug";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_SETUP_OCT_TREE_DEBUG_DRAW = "setup_oct_tree_debug_draw";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RUN_OCT_TREE_DEBUG_DRAW = "run_oct_tree_debug_draw";
    /**
     * @brief Stable octree graph task identifier.
     */
    static constexpr std::string_view RT_RUN_OCT_TREE_PRE_DEFERRED_DEBUG_DRAW = "run_oct_tree_pre_deferred_debug_draw";

private:
    void create_resources();
    void initialize();
    void compile_pipelines();
    Ref<ROBufferVK> create_initialized_buffer(VkBufferUsageFlags p_usage, const void* p_data, size_t p_size) const;
    RendTaskDescVK get_state_update_render_task_desc(std::string_view p_previous_task_id) const;
    RendTaskDescVK get_debug_capture_render_task_desc(
        std::string_view p_task_id, std::string_view p_previous_task_id, VoxelDebugViewMode p_mode) const;
    static constexpr std::string_view RT_UPDATE_OCT_TREE_STATES = "update_oct_tree_states";
    static constexpr std::string_view OCT_TREE_VOXEL_CLAIMS = "oct_tree_voxel_claims";
    static constexpr std::string_view OCT_TREE_PENDING_CHILDREN = "oct_tree_pending_children";
    static constexpr std::string_view GBUFFER_DEPTH_IMAGE_VIEW = "gbuffer_depth";
    static constexpr std::string_view GBUFFER_CAMERA_INFO_BUFFER = "gbuffer_camera_info";
    static constexpr VkFormat GBUFFER_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
    static constexpr size_t OCT_TREE_MAX_DEPTH = 9;
    static constexpr size_t OCT_TREE_EXTENT = 1 << OCT_TREE_MAX_DEPTH;
    static constexpr size_t OCT_TREE_MAX_LEAVES_COUNT = OCT_TREE_EXTENT * OCT_TREE_EXTENT * OCT_TREE_EXTENT / 8;
    static constexpr size_t OCT_TREE_MAX_NODE_COUNT = OCT_TREE_MAX_LEAVES_COUNT / 4;
    static constexpr size_t OCT_TREE_MAX_DEFERRED_TASK_COUNT = OCT_TREE_MAX_NODE_COUNT;
    static constexpr size_t OCT_TREE_VOXEL_COUNT = OCT_TREE_EXTENT * OCT_TREE_EXTENT * OCT_TREE_EXTENT;
    static constexpr size_t OCT_TREE_CLAIM_WORD_COUNT = (OCT_TREE_VOXEL_COUNT + 31U) / 32U;
    // A voxel can hold at most one unreturned eight-slot ticket; include transient overflow reservations.
    static_assert(OCT_TREE_VOXEL_COUNT <= (0xffffffffU - OCT_TREE_MAX_NODE_COUNT) / 8U);
    static_assert(OCT_TREE_VOXEL_COUNT <= (0xffffffffU - OCT_TREE_MAX_LEAVES_COUNT) / 8U);
    static constexpr float OCT_TREE_WIDTH = 32.0F;
    static constexpr glm::vec3 OCT_TREE_ORIGIN = glm::vec3(-16.0F, -8.0F, -16.0F);

    static constexpr std::string_view OCT_TREE_INFO = "oct_tree_info";
    static constexpr std::string_view OCT_TREE_NODE_BUFFER = "oct_tree_node_buffer";
    static constexpr std::string_view OCT_TREE_LEAF_BUFFER = "oct_tree_leaves_buffer";
    static constexpr std::string_view OCT_TREE_DEFERRED_LIST_PING = "oct_tree_deferred_list_ping";
    static constexpr std::string_view OCT_TREE_DEFERRED_LIST_PONG = "oct_tree_deferred_list_pong";
    static constexpr std::string_view OCT_TREE_DEBUG_DRAW_INDEX_BUFFER = "oct_tree_debug_draw_index_buffer";
    static constexpr std::string_view OCT_TREE_DEBUG_DRAW_INDIRECT_BUFFER = "oct_tree_debug_draw_indirect_buffer";
    static constexpr std::string_view OCT_TREE_PRE_DEFERRED_DEBUG_INFO_BUFFER = "oct_tree_pre_deferred_debug_info_buffer";
    static constexpr std::string_view OCT_TREE_PRE_DEFERRED_DEBUG_LIST_BUFFER = "oct_tree_pre_deferred_debug_list_buffer";
    static constexpr std::string_view OCT_TREE_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER = "oct_tree_pre_deferred_debug_draw_indirect_buffer";

    struct OctTreeInfo {
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
    };

    struct OctTreeNode {
        enum class ChildrenState {
            NODE = 0,
            EMPTY = 1,
            ALLOCATING = 2,
            LEAF = 3
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
        // Mirrors the packed Slang description: state 31:30; leaf occupancy 29:22/block 21:0;
        // nodes use block 21:0 with bits 29:22 unused. Block indices address eight entries.
        uint32_t children_data;
        uint32_t data_index;
        int32_t parent_index;
    };

    static_assert(sizeof(OctTreeNode) == 12);
    static_assert(alignof(OctTreeNode) == alignof(uint32_t));
    static_assert(offsetof(OctTreeNode, data_index) == 4);
    static_assert(offsetof(OctTreeNode, parent_index) == 8);
    static_assert(OCT_TREE_MAX_DEPTH >= 2);
    static_assert(OCT_TREE_MAX_NODE_COUNT % 8 == 0 && OCT_TREE_MAX_NODE_COUNT <= (1U << 22U) * 8U);
    static_assert(OCT_TREE_MAX_LEAVES_COUNT % 8 == 0 && OCT_TREE_MAX_LEAVES_COUNT <= (1U << 22U) * 8U);

    struct OctTreeLeaf {
        uint32_t data_index;
        int32_t parent_index;
    };

    struct OctTreeDeferredListEntry {
        uint32_t node_index;
        uint32_t current_depth;
        int32_t remaining_depth;
        alignas(16) glm::vec3 oct_tree_position;
    };

    static_assert(offsetof(OctTreeDeferredListEntry, oct_tree_position) == 16);
    static_assert(sizeof(OctTreeDeferredListEntry) == 32);

    RendererVK* renderer_vk;
    const BaseRenderDataVK* base_render_data;
    RenderAllocator* render_allocator;
    Ref<StaticSetResourcesVK> static_set_resources;
    const std::atomic<VoxelDebugViewMode>* debug_view_mode;
    Implementation implementation;
    Ref<ROBufferVK> pending_children;
    Ref<ROBufferVK> voxel_claims;
    Ref<ROComputePipelineVK> update_states_pipeline;
    Ref<ROGraphicsPipelineVK> oct_tree_build_graphics_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> oct_tree_reset_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> oct_tree_deferred_count_reset_ping_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> oct_tree_deferred_count_reset_pong_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> oct_tree_deferred_propagation_ping_write_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> oct_tree_deferred_propagation_pong_write_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> oct_tree_pre_deferred_debug_capture_compute_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> oct_tree_debug_draw_setup_compute_pipeline = MEM_NULL;
    Ref<ROGraphicsPipelineVK> oct_tree_debug_draw_graphics_pipeline = MEM_NULL;
    Ref<ROGraphicsPipelineVK> oct_tree_pre_deferred_debug_draw_graphics_pipeline = MEM_NULL;
    Ref<ROBufferVK> oct_tree_info;
    Ref<ROBufferVK> oct_tree_nodes;
    Ref<ROBufferVK> oct_tree_leaves;
    Ref<ROBufferVK> oct_tree_deferred_list_ping;
    Ref<ROBufferVK> oct_tree_deferred_list_pong;
    Ref<ROBufferVK> oct_tree_debug_draw_index_buffer;
    Ref<ROBufferVK> oct_tree_debug_draw_indirect_buffer;
    Ref<ROBufferVK> oct_tree_pre_deferred_debug_info_buffer;
    Ref<ROBufferVK> oct_tree_pre_deferred_debug_list_buffer;
    Ref<ROBufferVK> oct_tree_pre_deferred_debug_draw_indirect_buffer;
};

} // namespace WhiteBirdEngine

#endif
