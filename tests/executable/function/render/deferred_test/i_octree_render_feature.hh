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
#ifndef WBE_FILE_I_OCTREE_RENDER_FEATURE_HH
#define WBE_FILE_I_OCTREE_RENDER_FEATURE_HH

#include "../render_test.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/static_set_resources_vk.hh"
#include "glm/ext/vector_float3.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <glm/vec3.hpp>
#include <string_view>
#include <vulkan/vulkan_core.h>

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
 * @class IOctreeRenderFeature
 * @brief Interface of the deferred test's octree construction and debug rendering.
 * @details Owns the buffers, raster construction, deferred-list debugging, and debug draws shared by every construction
 * algorithm; implementations supply their publication strategy, deferred retries, and implementation-specific resource
 * ordering. Neither copyable nor movable. All methods and destruction are confined to the rendering thread. Registered
 * graphs retain their resource handles and task factories; borrowed renderer and debug state must remain alive.
 */
class IOctreeRenderFeature {
public:
    IOctreeRenderFeature() = delete;
    virtual ~IOctreeRenderFeature() = default;
    IOctreeRenderFeature(const IOctreeRenderFeature&) = delete;
    IOctreeRenderFeature(IOctreeRenderFeature&&) = delete;
    IOctreeRenderFeature& operator=(const IOctreeRenderFeature&) = delete;
    IOctreeRenderFeature& operator=(IOctreeRenderFeature&&) = delete;

    /**
     * @brief Human-readable name of the construction algorithm.
     */
    virtual std::string_view get_name() const = 0;
    /**
     * @brief Registers all implementation-owned buffers with their stable shader resource names.
     */
    void register_render_graph_data(RendGraphDescVK& p_graph) const;
    /**
     * @brief Adds reset, static and dynamic raster draws, publication when required, and pre-deferred capture.
     */
    void add_construction_render_tasks(RendGraphDescVK& p_graph, const DynamicMeshRenderFeature& p_dynamic_mesh) const;
    /**
     * @brief Connects every octree task through resource edges on the octree buffers.
     * @details Call after all construction, deferred, and debug-draw tasks were added to p_graph.
     * @param p_graph Graph descriptor containing the octree tasks.
     */
    void connect_render_tasks(RendGraphDescVK& p_graph) const;
    /**
     * @brief Creates the per-frame reset.
     */
    RendTaskDescVK get_reset_render_task_desc() const;
    /**
     * @brief Creates one raster construction pass over the retained scene.
     */
    RendTaskDescVK get_construction_render_task_desc(
        std::string_view p_task_id, bool p_double_sided = false, const DynamicMeshRenderFeature* p_dynamic_mesh = nullptr) const;
    /**
     * @brief Creates bounded deferred retries with the implementation's allocator.
     * @param p_dynamic Whether the retries continue dynamic construction rather than the static scene.
     */
    RendTaskDescVK get_deferred_construction_render_task_desc(bool p_dynamic = true) const;
    /**
     * @brief Creates indirect debug-draw setup after construction is complete.
     */
    RendTaskDescVK get_debug_draw_setup_render_task_desc() const;
    /**
     * @brief Creates the completed-tree overlay drawn over the presented image.
     */
    RendTaskDescVK get_debug_draw_render_task_desc() const;
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

protected:
    /**
     * @brief Creates the shared pipelines and buffers on the rendering thread.
     * @details The renderer and debug-mode state must outlive this object and its registered graphs. The feature retains
     * shared ownership of the static scene. Invalid dependencies throw.
     * @param p_shader_prefix Resource-key prefix of the static construction shaders.
     * @param p_dynamic_shader_prefix Resource-key prefix of the dynamic construction shaders.
     * @param p_rebuild_static_every_frame When true, the full static-scene construction runs every frame instead of once,
     * for observing its per-frame cost. Dynamic construction is unaffected either way.
     */
    IOctreeRenderFeature(RendererVK* p_renderer,
        Ref<StaticSetResourcesVK> p_static_set_resources,
        const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
        std::string_view p_shader_prefix,
        std::string_view p_dynamic_shader_prefix,
        bool p_rebuild_static_every_frame);

    /**
     * @brief Workgroup count of the per-frame reset dispatch.
     */
    virtual uint32_t get_reset_group_count() const = 0;
    /**
     * @brief Registers resources used only by this implementation.
     */
    virtual void register_implementation_render_graph_data(RendGraphDescVK& p_graph) const = 0;
    /**
     * @brief Adds the tasks that publish the static raster construction before its deferred retries.
     */
    virtual void add_static_publication_render_tasks(RendGraphDescVK& p_graph) const = 0;
    /**
     * @brief Adds the tasks that publish the dynamic raster construction before its deferred retries.
     */
    virtual void add_dynamic_publication_render_tasks(RendGraphDescVK& p_graph) const = 0;
    /**
     * @brief Connects the octree state resources, whose use depends on the publication strategy.
     * @details Must order every access of octree_info, the node buffer, the dynamic buffers, and implementation resources.
     */
    virtual void connect_implementation_render_tasks(RendGraphDescVK& p_graph) const = 0;
    /**
     * @brief Creates the deferred retry task; the caller assigns its task identifier.
     * @param p_dynamic Whether the retries continue dynamic construction rather than the static scene.
     */
    virtual RendTaskDescVK make_deferred_construction_render_task_desc(bool p_dynamic) const = 0;

    /**
     * @brief Compiles a compute pipeline from its shader resource key.
     */
    Ref<ROComputePipelineVK> compile_compute_pipeline(std::string_view p_resource_id) const;
    /**
     * @brief Runs a static construction stage once unless the static scene is rebuilt every frame.
     * @param p_finish Whether recording this stage completes the one-time static construction.
     */
    RendTaskDescVK make_static_once(RendTaskDescVK p_task, bool p_finish = false) const;
    /**
     * @brief Returns p_link as a read of its resource.
     */
    static constexpr ResourceChainLink as_read(ResourceChainLink p_link) {
        p_link.writes = false;
        return p_link;
    }

    /**
     * @brief Graph task identifier of the static state-publication pass.
     */
    static constexpr std::string_view RT_UPDATE_OCTREE_STATES = "update_octree_states";
    /**
     * @brief Graph task identifier of the one-time static deferred retries.
     */
    static constexpr std::string_view RT_STATIC_OCTREE_DEFERRED = "static_octree_deferred";
    /**
     * @brief Graph task identifier of the per-frame dynamic reset.
     */
    static constexpr std::string_view RT_CLEAR_DYNAMIC_OCTREE = "clear_dynamic_octree";
    /**
     * @brief Graph task identifier of the dynamic raster construction.
     */
    static constexpr std::string_view RT_RUN_BUILD_DYNAMIC_OCTREE = "dynamic_octree";
    /**
     * @brief Graph task identifier of the dynamic state-publication pass.
     */
    static constexpr std::string_view RT_PUBLISH_DYNAMIC_OCTREE = "publish_dynamic_octree";
    /**
     * @brief Graph resource of the per-voxel claim bitset.
     */
    static constexpr std::string_view OCTREE_VOXEL_CLAIMS = "octree_voxel_claims";
    /**
     * @brief Graph resource of the pending child descriptions used by pass publication.
     */
    static constexpr std::string_view OCTREE_PENDING_CHILDREN = "octree_pending_children";
    /**
     * @brief Graph resource of the octree counters and configuration.
     */
    static constexpr std::string_view OCTREE_INFO = "octree_info";
    /**
     * @brief Graph resource of the static node buffer.
     */
    static constexpr std::string_view OCTREE_NODE_BUFFER = "octree_node_buffer";
    /**
     * @brief Graph resource of the dynamic node buffer.
     */
    static constexpr std::string_view OCTREE_DYNAMIC_NODES = "octree_dynamic_nodes";
    /**
     * @brief Graph resource of the copy-on-write dynamic leaf blocks.
     */
    static constexpr std::string_view OCTREE_DYNAMIC_LEAVES = "octree_dynamic_leaves";
    /**
     * @brief Depth of the leaf level; root sectors have depth one.
     */
    static constexpr size_t OCTREE_MAX_DEPTH = 9;
    /**
     * @brief Number of finest-level voxels.
     */
    static constexpr size_t OCTREE_VOXEL_COUNT = (size_t{1} << OCTREE_MAX_DEPTH) * (size_t{1} << OCTREE_MAX_DEPTH) * (size_t{1} << OCTREE_MAX_DEPTH);
    /**
     * @brief Capacity of the static node buffer.
     */
    static constexpr size_t OCTREE_MAX_NODE_COUNT = OCTREE_VOXEL_COUNT / 64;
    /**
     * @brief Capacity of the dynamic node buffer.
     */
    static constexpr size_t OCTREE_MAX_DYNAMIC_NODE_COUNT = OCTREE_MAX_NODE_COUNT;
    /**
     * @brief Number of 32-bit words in the voxel claim bitset.
     */
    static constexpr size_t OCTREE_CLAIM_WORD_COUNT = (OCTREE_VOXEL_COUNT + 31U) / 32U;
    /**
     * @brief Capacity of each deferred list.
     */
    static constexpr size_t OCTREE_MAX_DEFERRED_TASK_COUNT = OCTREE_MAX_NODE_COUNT * 5 / 2;
    /**
     * @brief Workgroup size of the deferred propagation and debug-capture shaders.
     */
    static constexpr uint32_t OCTREE_DEFERRED_THREADS_PER_GROUP = 128;
    /**
     * @brief Workgroup count covering a full deferred list.
     */
    static constexpr uint32_t OCTREE_DEFERRED_DISPATCH_GROUP_COUNT = static_cast<uint32_t>(
        (OCTREE_MAX_DEFERRED_TASK_COUNT + OCTREE_DEFERRED_THREADS_PER_GROUP - 1) / OCTREE_DEFERRED_THREADS_PER_GROUP);
    /**
     * @brief Pipeline stages of compute octree tasks.
     */
    static constexpr VkPipelineStageFlags2 STAGE_COMPUTE = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    /**
     * @brief Pipeline stages of raster octree tasks.
     */
    static constexpr VkPipelineStageFlags2 STAGE_RASTER = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    /**
     * @brief Resource-chain use by the per-frame reset.
     */
    static constexpr ResourceChainLink LINK_RESET{.task_id = RT_RESET_OCTREE, .stage = STAGE_COMPUTE, .writes = true};
    /**
     * @brief Resource-chain use by the single-sided static raster construction.
     */
    static constexpr ResourceChainLink LINK_BUILD{.task_id = RT_RUN_BUILD_OCTREE, .stage = STAGE_RASTER, .writes = true};
    /**
     * @brief Resource-chain use by the double-sided static raster construction.
     */
    static constexpr ResourceChainLink LINK_BUILD_DOUBLE_SIDED{
        .task_id = RT_RUN_BUILD_OCTREE_DOUBLE_SIDED, .stage = STAGE_RASTER, .writes = true};
    /**
     * @brief Resource-chain use by the static state publication.
     */
    static constexpr ResourceChainLink LINK_STATE_UPDATE{.task_id = RT_UPDATE_OCTREE_STATES, .stage = STAGE_COMPUTE, .writes = true};
    /**
     * @brief Resource-chain use by the static deferred retries.
     */
    static constexpr ResourceChainLink LINK_STATIC_DEFERRED{.task_id = RT_STATIC_OCTREE_DEFERRED, .stage = STAGE_COMPUTE, .writes = true};
    /**
     * @brief Resource-chain use by the per-frame dynamic reset.
     */
    static constexpr ResourceChainLink LINK_CLEAR{.task_id = RT_CLEAR_DYNAMIC_OCTREE, .stage = STAGE_COMPUTE, .writes = true};
    /**
     * @brief Resource-chain use by the dynamic raster construction.
     */
    static constexpr ResourceChainLink LINK_DYNAMIC_BUILD{.task_id = RT_RUN_BUILD_DYNAMIC_OCTREE, .stage = STAGE_RASTER, .writes = true};
    /**
     * @brief Resource-chain use by the dynamic state publication.
     */
    static constexpr ResourceChainLink LINK_PUBLISH{.task_id = RT_PUBLISH_DYNAMIC_OCTREE, .stage = STAGE_COMPUTE, .writes = true};
    /**
     * @brief Resource-chain use by the pre-deferred debug capture.
     */
    static constexpr ResourceChainLink LINK_CAPTURE{
        .task_id = RT_CAPTURE_OCTREE_PRE_DEFERRED_DEBUG, .stage = STAGE_COMPUTE, .writes = true};
    /**
     * @brief Resource-chain use by the per-frame deferred retries.
     */
    static constexpr ResourceChainLink LINK_DEFERRED{.task_id = RT_RUN_BUILD_OCTREE_DEFERRED, .stage = STAGE_COMPUTE, .writes = true};
    /**
     * @brief Resource-chain use by the debug-draw setup.
     */
    static constexpr ResourceChainLink LINK_SETUP{.task_id = RT_SETUP_OCTREE_DEBUG_DRAW, .stage = STAGE_COMPUTE, .writes = true};
    /**
     * @brief Resource-chain use by the completed-tree debug draw.
     */
    static constexpr ResourceChainLink LINK_DEBUG_DRAW{.task_id = RT_RUN_OCTREE_DEBUG_DRAW, .stage = STAGE_RASTER};
    /**
     * @brief Resource-chain use by the pre-deferred debug draw.
     */
    static constexpr ResourceChainLink LINK_PRE_DEFERRED_DRAW{.task_id = RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW, .stage = STAGE_RASTER};
    /**
     * @brief Borrowed renderer that owns the pipelines and graphs.
     */
    RendererVK* renderer_vk;
    /**
     * @brief Borrowed renderer data used to create buffers and tasks.
     */
    const BaseRenderDataVK* base_render_data;
    /**
     * @brief Borrowed allocator of render objects and tasks.
     */
    RenderAllocator* render_allocator;
    /**
     * @brief Resets the ping deferred count before an iteration that writes the ping list.
     */
    Ref<ROComputePipelineVK> octree_deferred_count_reset_ping_compute_pipeline = MEM_NULL;
    /**
     * @brief Resets the pong deferred count before an iteration that writes the pong list.
     */
    Ref<ROComputePipelineVK> octree_deferred_count_reset_pong_compute_pipeline = MEM_NULL;
    /**
     * @brief Static deferred retries that read pong and write ping.
     */
    Ref<ROComputePipelineVK> octree_deferred_propagation_ping_write_compute_pipeline = MEM_NULL;
    /**
     * @brief Static deferred retries that read ping and write pong.
     */
    Ref<ROComputePipelineVK> octree_deferred_propagation_pong_write_compute_pipeline = MEM_NULL;
    /**
     * @brief Dynamic deferred retries that read ping and write pong.
     */
    Ref<ROComputePipelineVK> dynamic_ping_pipeline;
    /**
     * @brief Dynamic deferred retries that read pong and write ping.
     */
    Ref<ROComputePipelineVK> dynamic_pong_pipeline;

private:
    static constexpr std::string_view RT_OCTREE_FRAME_BEGIN = "octree_frame_begin";
    static constexpr std::string_view GBUFFER_DEPTH_IMAGE_VIEW = "gbuffer_depth";
    static constexpr VkFormat GBUFFER_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
    static constexpr size_t OCTREE_EXTENT = size_t{1} << OCTREE_MAX_DEPTH;
    static constexpr size_t OCTREE_MAX_LEAVES_COUNT = OCTREE_VOXEL_COUNT / 8;
    static constexpr size_t OCTREE_MAX_DYNAMIC_LEAVES_COUNT = OCTREE_MAX_LEAVES_COUNT;
    // Keeps the expanded lists within 65535 workgroups.
    static_assert(OCTREE_DEFERRED_DISPATCH_GROUP_COUNT <= 65535);
    // A voxel can hold at most one unreturned eight-slot ticket; include transient overflow reservations.
    static_assert(OCTREE_VOXEL_COUNT <= (0xffffffffU - OCTREE_MAX_NODE_COUNT) / 8U);
    static_assert(OCTREE_VOXEL_COUNT <= (0xffffffffU - OCTREE_MAX_LEAVES_COUNT) / 8U);
    static constexpr float OCTREE_WIDTH = 32.0F;
    static constexpr glm::vec3 OCTREE_ORIGIN = glm::vec3(-16.0F, -8.0F, -16.0F);
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

    struct OctreeDeferredListEntry {
        uint32_t node_index;
        uint32_t packed_depth;
        uint32_t local_voxel_index;
    };

    static_assert(sizeof(OctreeDeferredListEntry) == 12);
    // local_voxel_index packs max_depth bits per axis into 32 bits.
    static_assert(OCTREE_MAX_DEPTH <= 10);

    void create_resources();
    void initialize();
    void compile_pipelines(std::string_view p_shader_prefix, std::string_view p_dynamic_shader_prefix);
    Ref<ROBufferVK> create_initialized_buffer(VkBufferUsageFlags p_usage, const void* p_data, size_t p_size) const;
    RendTaskDescVK get_debug_capture_render_task_desc(std::string_view p_task_id, VoxelDebugViewMode p_mode) const;
    RendTaskDescVK get_dynamic_clear_task_desc() const;

    Ref<StaticSetResourcesVK> static_set_resources;
    const std::atomic<VoxelDebugViewMode>* debug_view_mode;
    // Skips the run-once wrappers so the static scene is reset and rebuilt every frame.
    bool rebuild_static_every_frame;
    // Render-thread completion shared by all graph variants using these buffers.
    Ref<bool> static_build_completed;
    Ref<ROBufferVK> dynamic_nodes;
    Ref<ROBufferVK> dynamic_leaves;
    Ref<ROGraphicsPipelineVK> dynamic_build_pipeline;
    Ref<ROComputePipelineVK> dynamic_clear_pipeline;
    Ref<ROBufferVK> voxel_claims;
    Ref<ROGraphicsPipelineVK> octree_build_graphics_pipeline = MEM_NULL;
    Ref<ROComputePipelineVK> octree_reset_compute_pipeline = MEM_NULL;
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
