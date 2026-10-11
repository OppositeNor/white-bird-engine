/* Copyright 2026 OppositeNor

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
#ifndef WBE_FILE_VOXEL_RENDER_FEATURE_OCTREE_HH
#define WBE_FILE_VOXEL_RENDER_FEATURE_OCTREE_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/mesh_resources_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "i_voxel_render_feature.hh"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <glm/vec3.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class VoxelRenderFeatureOctree
 * @brief Octree stored in the mip chain of uint8 3D textures, built over the deferred test's scene.
 * @details Mip level 0 holds the leaves and mip level l the nodes of depth max_depth - l, so the octree needs no pointers:
 * every texel is 255 when nothing below it is occupied, and the construction writes 0 into occupied texels. The static scene
 * and the dynamic meshes have separate textures. Each raster construction pass occupies the covered leaves and their
 * ancestors in every mip level directly, so construction needs no allocation, retries, or record of the occupied leaves. The
 * dynamic texture is cleared entirely every frame before its meshes are rasterized again. Shaders read the textures through
 * octree/octree.slang.
 * Neither copyable nor movable. All methods and destruction are confined to the rendering thread. Registered graphs retain
 * their resource handles and task factories; the borrowed renderer and debug state must remain alive.
 */
class VoxelRenderFeatureOctree final : public IVoxelRenderFeature {
public:
    VoxelRenderFeatureOctree() = delete;
    /**
     * @brief Compiles the octree pipelines and creates the textures and buffers on the rendering thread.
     * @details The renderer and debug-mode state must outlive this object and its registered graphs. The feature retains
     * shared ownership of the static scene. Invalid dependencies throw.
     * @param p_rebuild_static_every_frame When true, the full static-scene construction runs every frame instead of once,
     * for observing its per-frame cost. Dynamic construction is unaffected either way.
     */
    VoxelRenderFeatureOctree(RendererVK* p_renderer,
        Ref<MeshResourcesVK> p_static_mesh_resources,
        const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
        bool p_rebuild_static_every_frame = false);
    virtual ~VoxelRenderFeatureOctree() override = default;
    VoxelRenderFeatureOctree(const VoxelRenderFeatureOctree&) = delete;
    VoxelRenderFeatureOctree(VoxelRenderFeatureOctree&&) = delete;
    VoxelRenderFeatureOctree& operator=(const VoxelRenderFeatureOctree&) = delete;
    VoxelRenderFeatureOctree& operator=(VoxelRenderFeatureOctree&&) = delete;

    virtual std::string_view get_name() const override;
    /**
     * @brief Names the leaf view, drawn for POST_SVO_BUILD, and the node view, drawn for PRE_DEFERRED_PROPAGATION.
     * @details The octree has no deferred propagation; its second view adds every occupied node between the leaves and the
     * root, colored by depth, to the leaf view.
     */
    virtual const char* get_debug_view_name(VoxelDebugViewMode p_mode) const override;
    /**
     * @brief Registers the textures, their full and per-level views, and the buffers.
     */
    virtual void register_render_graph_data(RendGraphDescVK& p_graph) const override;
    /**
     * @brief Adds the static construction, the per-frame dynamic construction, and the debug leaf and node gathers.
     */
    virtual void add_build_render_tasks(RendGraphDescVK& p_graph, const DynamicMeshRenderFeature& p_dynamic_mesh) const override;
    /**
     * @brief Adds the leaf overlay followed by the node overlay.
     */
    virtual void add_overlay_render_tasks(RendGraphDescVK& p_graph) const override;
    virtual void connect_render_tasks(RendGraphDescVK& p_graph) const override;
    virtual std::span<const std::string_view> get_overlay_render_task_ids() const override;

    /**
     * @brief Graph id of the single-level storage view of one mip level of the static texture.
     */
    static std::string get_static_level_resource_id(uint32_t p_level);
    /**
     * @brief Graph id of the single-level storage view of one mip level of the dynamic texture.
     */
    static std::string get_dynamic_level_resource_id(uint32_t p_level);

    static constexpr std::string_view RT_RESET_OCTREE = "reset_octree";
    static constexpr std::string_view RT_RUN_BUILD_OCTREE = "run_build_octree";
    static constexpr std::string_view RT_RUN_BUILD_OCTREE_DOUBLE_SIDED = "run_build_octree_double_sided";
    static constexpr std::string_view RT_CLEAR_DYNAMIC_OCTREE = "clear_dynamic_octree";
    static constexpr std::string_view RT_RUN_BUILD_DYNAMIC_OCTREE = "dynamic_octree";
    static constexpr std::string_view RT_RUN_BUILD_DYNAMIC_OCTREE_SECOND = "dynamic_octree_second";
    static constexpr std::string_view RT_GATHER_OCTREE_DEBUG_LEAVES = "gather_octree_debug_leaves";
    static constexpr std::string_view RT_GATHER_OCTREE_DEBUG_NODES = "gather_octree_debug_nodes";
    static constexpr std::string_view RT_RUN_OCTREE_DEBUG_DRAW = "run_octree_debug_draw";
    static constexpr std::string_view RT_RUN_OCTREE_DEBUG_NODE_DRAW = "run_octree_debug_node_draw";

    static constexpr std::string_view OCTREE_INFO = "octree_info";
    /** Static texture viewed with every mip level, read by traversal. */
    static constexpr std::string_view OCTREE_STATIC = "octree_static";
    /** Dynamic texture viewed with every mip level, read by traversal. */
    static constexpr std::string_view OCTREE_DYNAMIC = "octree_dynamic";
    static constexpr std::string_view OCTREE_DEBUG_DRAW_INDEX_BUFFER = "octree_debug_draw_index_buffer";
    static constexpr std::string_view OCTREE_DEBUG_DRAW_INDIRECT_BUFFER = "octree_debug_draw_indirect_buffer";
    static constexpr std::string_view OCTREE_DEBUG_LEAF_MASKS = "octree_debug_leaf_masks";
    static constexpr std::string_view OCTREE_DEBUG_NODE_DRAW_INDIRECT_BUFFER = "octree_debug_node_draw_indirect_buffer";
    static constexpr std::string_view OCTREE_DEBUG_NODE_LIST = "octree_debug_node_list";
    static constexpr std::array<std::string_view, 2> OVERLAY_RENDER_TASK_IDS{RT_RUN_OCTREE_DEBUG_DRAW, RT_RUN_OCTREE_DEBUG_NODE_DRAW};

private:
    static constexpr std::string_view RT_OCTREE_FRAME_BEGIN = "octree_frame_begin";
    static constexpr std::string_view GBUFFER_DEPTH_IMAGE_VIEW = "gbuffer_depth";
    static constexpr VkFormat GBUFFER_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
    static constexpr VkFormat OCTREE_FORMAT = VK_FORMAT_R8_UINT;
    static constexpr uint32_t OCTREE_MAX_DEPTH = 9;
    // Mirrors OCTREE_LEVEL_DESCRIPTOR_COUNT of octree/octree_common.slang, which binds one storage view per level.
    static constexpr uint32_t OCTREE_LEVEL_COUNT = OCTREE_MAX_DEPTH + 1;
    static_assert(OCTREE_LEVEL_COUNT == 10);
    static constexpr uint32_t OCTREE_EXTENT = uint32_t{1} << OCTREE_MAX_DEPTH;
    static constexpr size_t OCTREE_VOXEL_COUNT = size_t{OCTREE_EXTENT} * OCTREE_EXTENT * OCTREE_EXTENT;
    static constexpr uint32_t OCTREE_MAX_DEBUG_NODE_COUNT = static_cast<uint32_t>(OCTREE_VOXEL_COUNT / 64);
    // Mirrors OCTREE_DEBUG_SUBGRID_EXTENT of octree/octree_debug.slang; the leaf gather runs one 4x4x8 workgroup per 8x8x16
    // subgrid, each invocation testing a 2x2x2 block.
    static constexpr VkExtent3D OCTREE_DEBUG_SUBGRID_GRID_EXTENT{
        .width = OCTREE_EXTENT / 8, .height = OCTREE_EXTENT / 8, .depth = OCTREE_EXTENT / 16};
    static constexpr uint32_t OCTREE_DEBUG_SUBGRID_COUNT =
        OCTREE_DEBUG_SUBGRID_GRID_EXTENT.width * OCTREE_DEBUG_SUBGRID_GRID_EXTENT.height * OCTREE_DEBUG_SUBGRID_GRID_EXTENT.depth;
    // One leaf-view draw command per subgrid, followed by the bounds command.
    static constexpr uint32_t OCTREE_DEBUG_LEAF_DRAW_COUNT = OCTREE_DEBUG_SUBGRID_COUNT + 1;
    // Mirrors OctreeDebugSubgridMask of octree/octree_debug.slang: one bit per leaf of an 8x8x16 subgrid.
    static constexpr size_t OCTREE_DEBUG_LEAF_MASK_SIZE = 8 * 8 * 16 / 8;
    // Mirrors the debug node list of octree/octree_debug.slang: the entry count, then entries.
    static constexpr uint32_t OCTREE_DEBUG_NODE_LIST_HEADER_COUNT = 1;
    // Mirrors OCTREE_DEBUG_GATHER_LEVEL of octree/octree_debug.slang and the gather's 4x4x4 workgroups.
    static constexpr uint32_t OCTREE_DEBUG_GATHER_LEVEL = 4;
    static constexpr uint32_t OCTREE_DEBUG_GATHER_GROUP_COUNT = (OCTREE_EXTENT >> OCTREE_DEBUG_GATHER_LEVEL) / 4;
    static_assert(OCTREE_DEBUG_GATHER_GROUP_COUNT > 0);
    static constexpr float OCTREE_WIDTH = 32.0F;
    static constexpr glm::vec3 OCTREE_ORIGIN = glm::vec3(-16.0F, -8.0F, -16.0F);

    /**
     * @brief Mirrors OctreeInfo of octree/octree_common.slang.
     */
    struct OctreeInfo {
        glm::vec3 origin;
        float width;
        uint32_t max_depth;
        uint32_t max_debug_node_count;
        // Pads to the structure's 16-byte storage-buffer stride.
        std::array<uint32_t, 2> padding;
    };
    static_assert(sizeof(OctreeInfo) == 32);
    static_assert(offsetof(OctreeInfo, max_depth) == 16);

    void compile_pipelines();
    void create_resources();
    Ref<ROComputePipelineVK> compile_compute_pipeline(std::string_view p_resource_id) const;
    Ref<ROGraphicsPipelineVK> compile_debug_draw_pipeline(std::string_view p_resource_id) const;
    Ref<ROImageVK> create_octree_image() const;
    Ref<ROImageViewVK> create_octree_image_view(const Ref<ROImageVK>& p_image, uint32_t p_base_level, uint32_t p_level_count) const;
    Ref<ROBufferVK> create_initialized_buffer(VkBufferUsageFlags p_usage, const void* p_data, size_t p_size) const;
    RendTaskDescVK make_static_once(RendTaskDescVK p_task, bool p_finish = false) const;
    RendTaskDescVK make_texture_write(RendTaskDescVK p_task, std::string_view p_texture_view_id) const;
    RendTaskDescVK get_clear_render_task_desc(std::string_view p_task_id, bool p_clear_static) const;
    RendTaskDescVK get_construction_render_task_desc(
        std::string_view p_task_id, bool p_double_sided, const DynamicMeshRenderFeature* p_dynamic_mesh, size_t p_dynamic_entity_index) const;
    RendTaskDescVK get_compute_task_desc(
        std::string_view p_task_id, const Ref<ROComputePipelineVK>& p_pipeline, VkExtent3D p_group_count) const;
    RendTaskDescVK get_debug_draw_render_task_desc(std::string_view p_task_id,
        const Ref<ROGraphicsPipelineVK>& p_pipeline,
        std::string_view p_indirect_buffer_id,
        uint32_t p_draw_count,
        VoxelDebugViewMode p_first_mode) const;

    RendererVK* renderer_vk;
    const BaseRenderDataVK* base_render_data;
    RenderAllocator* render_allocator;
    Ref<MeshResourcesVK> static_mesh_resources;
    const std::atomic<VoxelDebugViewMode>* debug_view_mode;
    // Skips the run-once wrappers so the static scene is reset and rebuilt every frame.
    bool rebuild_static_every_frame;
    // Render-thread completion shared by all graph variants using these textures.
    Ref<bool> static_build_completed;

    Ref<ROGraphicsPipelineVK> build_pipeline;
    Ref<ROGraphicsPipelineVK> dynamic_build_pipeline;
    Ref<ROComputePipelineVK> debug_gather_leaves_pipeline;
    Ref<ROComputePipelineVK> debug_gather_nodes_pipeline;
    Ref<ROGraphicsPipelineVK> debug_draw_pipeline;
    Ref<ROGraphicsPipelineVK> debug_node_draw_pipeline;

    Ref<ROImageVK> static_image;
    Ref<ROImageVK> dynamic_image;
    Ref<ROImageViewVK> static_view;
    Ref<ROImageViewVK> dynamic_view;
    std::array<Ref<ROImageViewVK>, OCTREE_LEVEL_COUNT> static_level_views;
    std::array<Ref<ROImageViewVK>, OCTREE_LEVEL_COUNT> dynamic_level_views;
    Ref<ROBufferVK> octree_info;
    Ref<ROBufferVK> debug_draw_index_buffer;
    Ref<ROBufferVK> debug_draw_indirect_buffer;
    Ref<ROBufferVK> debug_leaf_masks;
    Ref<ROBufferVK> debug_node_draw_indirect_buffer;
    Ref<ROBufferVK> debug_node_list;
};

} // namespace WhiteBirdEngine

#endif
