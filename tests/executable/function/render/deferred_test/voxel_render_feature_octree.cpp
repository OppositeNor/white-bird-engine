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
#include "voxel_render_feature_octree.hh"
#include "../render_test.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "dynamic_mesh_render_feature.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/rt_triggered_vk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "rt_voxel_debug_view_vk.hh"
#include "rt_voxel_frame_begin_vk.hh"
#include "rt_voxel_static_stage_vk.hh"
#include "utils/defs.hh"
#include <array>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace WhiteBirdEngine {
namespace {

/**
 * @brief Clears octree textures to OCTREE_EMPTY in every mip level; neither copyable nor movable.
 * @details The first reported writes must be the textures' full views, so the graph moves every mip level into the unified
 * layout before the first clear. The retained images must outlive the task.
 */
class RTOctreeClearVK final : public IRenderTaskVK {
public:
    RTOctreeClearVK() = delete;
    RTOctreeClearVK(Vector<Ref<ROImageVK>, RenderAllocator> p_images, Vector<std::string, RenderAllocator> p_written_resource_ids)
        : images(std::move(p_images)), written_resource_ids(std::move(p_written_resource_ids)) {
    }
    virtual ~RTOctreeClearVK() override = default;
    RTOctreeClearVK(const RTOctreeClearVK&) = delete;
    RTOctreeClearVK(RTOctreeClearVK&&) = delete;
    RTOctreeClearVK& operator=(const RTOctreeClearVK&) = delete;
    RTOctreeClearVK& operator=(RTOctreeClearVK&&) = delete;

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::COMPUTE_1;
    }

    virtual void append_accesses(
        Vector<ResAccessVK, RenderAllocator>& /*p_reads*/, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        for (const std::string& resource_id : written_resource_ids) {
            p_writes.push_back({.res_id = resource_id, .usage = RendResUsageVK::TRANSFER_DST});
        }
    }

    virtual void record(VkCommandBuffer p_command_buffer, double /*p_delta*/) override {
        // Mirrors OCTREE_EMPTY of octree/octree_common.slang.
        const VkClearColorValue empty{.uint32 = {255, 255, 255, 255}};
        const VkImageSubresourceRange every_level{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        for (const Ref<ROImageVK>& image : images) {
            vkCmdClearColorImage(p_command_buffer, image->get_image(), VK_IMAGE_LAYOUT_GENERAL, &empty, 1, &every_level);
        }
    }

private:
    Vector<Ref<ROImageVK>, RenderAllocator> images;
    Vector<std::string, RenderAllocator> written_resource_ids;
};

/**
 * @brief Reports a texture written through its per-level views as also written through its full view; neither copyable nor
 * movable.
 * @details The traversal reads the full view, so its resource edges order the reads after this task's writes. The full view
 * is reported first, so a first write moves every mip level into the unified layout.
 */
class RTOctreeTextureWriteVK final : public IRenderTaskVK {
public:
    RTOctreeTextureWriteVK() = delete;
    RTOctreeTextureWriteVK(Unique<IRenderTaskVK> p_task, std::string_view p_texture_view_id)
        : task(std::move(p_task)), texture_view_id(p_texture_view_id) {
        WBE_DEBUG_ASSERT(task != MEM_NULL);
    }
    virtual ~RTOctreeTextureWriteVK() override = default;
    RTOctreeTextureWriteVK(const RTOctreeTextureWriteVK&) = delete;
    RTOctreeTextureWriteVK(RTOctreeTextureWriteVK&&) = delete;
    RTOctreeTextureWriteVK& operator=(const RTOctreeTextureWriteVK&) = delete;
    RTOctreeTextureWriteVK& operator=(RTOctreeTextureWriteVK&&) = delete;

    virtual QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }

    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        p_writes.push_back({.res_id = texture_view_id, .usage = RendResUsageVK::STORAGE_IMAGE});
        task->append_accesses(p_reads, p_writes);
    }

    virtual void bind_resources(const RendResViewVK& p_resources) override {
        task->bind_resources(p_resources);
    }

    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override {
        task->record(p_command_buffer, p_delta);
    }

private:
    Unique<IRenderTaskVK> task;
    std::string texture_view_id;
};

} // namespace

VoxelRenderFeatureOctree::VoxelRenderFeatureOctree(RendererVK* p_renderer,
    Ref<MeshResourcesVK> p_static_mesh_resources,
    const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
    bool p_rebuild_static_every_frame)
    : renderer_vk(p_renderer), base_render_data(nullptr), render_allocator(nullptr),
      static_mesh_resources(std::move(p_static_mesh_resources)), debug_view_mode(p_debug_view_mode),
      rebuild_static_every_frame(p_rebuild_static_every_frame) {
    if (renderer_vk == nullptr) {
        throw std::runtime_error("VoxelRenderFeatureOctree: renderer is required.");
    }
    if (static_mesh_resources == MEM_NULL) {
        throw std::runtime_error("VoxelRenderFeatureOctree: static scene is required.");
    }
    if (debug_view_mode == nullptr) {
        throw std::runtime_error("VoxelRenderFeatureOctree: debug view state is required.");
    }
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = base_render_data->allocator;
    static_build_completed = make_ref<bool>(render_allocator, false);
    compile_pipelines();
    create_resources();
}

std::string_view VoxelRenderFeatureOctree::get_name() const {
    return "octree (mipmapped 3D texture)";
}

const char* VoxelRenderFeatureOctree::get_debug_view_name(VoxelDebugViewMode p_mode) const {
    switch (p_mode) {
    case VoxelDebugViewMode::DISABLED:
        return "Disabled";
    case VoxelDebugViewMode::POST_SVO_BUILD:
        return "Octree leaves";
    case VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION:
        return "Octree nodes";
    }
    throw std::runtime_error("VoxelRenderFeatureOctree: unknown voxel debug view mode.");
}

std::string VoxelRenderFeatureOctree::get_static_level_resource_id(uint32_t p_level) {
    return std::format("octree_static_levels[{}]", p_level);
}

std::string VoxelRenderFeatureOctree::get_dynamic_level_resource_id(uint32_t p_level) {
    return std::format("octree_dynamic_levels[{}]", p_level);
}

Ref<ROComputePipelineVK> VoxelRenderFeatureOctree::compile_compute_pipeline(std::string_view p_resource_id) const {
    ComputePipelineCompilerVK compiler(ComputePipelineCompilerVKContext{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
    });
    return compiler.compile(p_resource_id, ResourceManager::get_singleton()->get_resource<RUShader>(p_resource_id)).compute_pipeline;
}

Ref<ROGraphicsPipelineVK> VoxelRenderFeatureOctree::compile_debug_draw_pipeline(std::string_view p_resource_id) const {
    GraphicsPipelineCompilerVKContext context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
        .depth_attachment_format = GBUFFER_DEPTH_FORMAT,
        .polygon_mode = VK_POLYGON_MODE_LINE,
    };
    return GraphicsPipelineCompilerVK(context)
        .compile(p_resource_id, ResourceManager::get_singleton()->get_resource<RUGraphicsPipeline>(p_resource_id))
        .graphics_pipeline;
}

void VoxelRenderFeatureOctree::compile_pipelines() {
    auto* resource_manager = ResourceManager::get_singleton();
    GraphicsPipelineCompilerVK build_compiler(GraphicsPipelineCompilerVKContext{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
    });
    constexpr std::string_view BUILD_PIPELINE = "octree_build_static_mesh_pipeline";
    build_pipeline =
        build_compiler.compile(BUILD_PIPELINE, resource_manager->get_resource<RUGraphicsPipeline>(BUILD_PIPELINE)).graphics_pipeline;
    constexpr std::string_view DYNAMIC_BUILD_PIPELINE = "octree_dynamic_build_static_mesh_pipeline";
    dynamic_build_pipeline =
        build_compiler.compile(DYNAMIC_BUILD_PIPELINE, resource_manager->get_resource<RUGraphicsPipeline>(DYNAMIC_BUILD_PIPELINE))
            .graphics_pipeline;
    debug_gather_leaves_pipeline = compile_compute_pipeline("octree_debug_gather_leaves_shader");
    debug_gather_nodes_pipeline = compile_compute_pipeline("octree_debug_gather_nodes_shader");
    debug_draw_pipeline = compile_debug_draw_pipeline("octree_debug_draw_pipeline");
    debug_node_draw_pipeline = compile_debug_draw_pipeline("octree_debug_node_draw_pipeline");
}

Ref<ROImageVK> VoxelRenderFeatureOctree::create_octree_image() const {
    return ROImageVK::Builder()
        .image_3d(VkExtent3D{.width = OCTREE_EXTENT, .height = OCTREE_EXTENT, .depth = OCTREE_EXTENT})
        .set_format(OCTREE_FORMAT)
        .set_mipmap_level(OCTREE_LEVEL_COUNT)
        .set_usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .create_ref(base_render_data);
}

Ref<ROImageViewVK> VoxelRenderFeatureOctree::create_octree_image_view(
    const Ref<ROImageVK>& p_image, uint32_t p_base_level, uint32_t p_level_count) const {
    return ROImageViewVK::Builder()
        .set_image(p_image)
        .set_view_type(VK_IMAGE_VIEW_TYPE_3D)
        .set_format(OCTREE_FORMAT)
        .set_subresource_range(VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = p_base_level,
            .levelCount = p_level_count,
            .baseArrayLayer = 0,
            .layerCount = 1,
        })
        .create_ref(base_render_data);
}

Ref<ROBufferVK> VoxelRenderFeatureOctree::create_initialized_buffer(VkBufferUsageFlags p_usage, const void* p_data, size_t p_size) const {
    return ROBufferVK::Builder().set_usage(p_usage).initial_data(p_data, p_size).create_ref(base_render_data);
}

void VoxelRenderFeatureOctree::create_resources() {
    static_image = create_octree_image();
    dynamic_image = create_octree_image();
    static_view = create_octree_image_view(static_image, 0, OCTREE_LEVEL_COUNT);
    dynamic_view = create_octree_image_view(dynamic_image, 0, OCTREE_LEVEL_COUNT);
    for (uint32_t level = 0; level < OCTREE_LEVEL_COUNT; ++level) {
        static_level_views[level] = create_octree_image_view(static_image, level, 1);
        dynamic_level_views[level] = create_octree_image_view(dynamic_image, level, 1);
    }
    const OctreeInfo info{
        .origin = OCTREE_ORIGIN,
        .width = OCTREE_WIDTH,
        .max_depth = OCTREE_MAX_DEPTH,
        .max_debug_node_count = OCTREE_MAX_DEBUG_NODE_COUNT,
        .padding = {},
    };
    octree_info = create_initialized_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &info, sizeof(info));
    debug_draw_index_buffer = create_initialized_buffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VOXEL_DEBUG_CUBE_INDICES.data(), sizeof(VOXEL_DEBUG_CUBE_INDICES));
    const VkDrawIndexedIndirectCommand initial_debug_draw_command{
        .indexCount = VOXEL_DEBUG_CUBE_INDICES.size(),
        .instanceCount = 0,
        .firstIndex = 0,
        .vertexOffset = 0,
        .firstInstance = 0,
    };
    // The leaf gather writes every command before each leaf-view draw.
    debug_draw_indirect_buffer = ROBufferVK::Builder()
                                     .set_properties(sizeof(VkDrawIndexedIndirectCommand),
                                         OCTREE_DEBUG_LEAF_DRAW_COUNT,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
                                     .create_ref(base_render_data);
    debug_leaf_masks = ROBufferVK::Builder()
                           .set_properties(OCTREE_DEBUG_LEAF_MASK_SIZE, OCTREE_DEBUG_SUBGRID_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                           .create_ref(base_render_data);
    debug_node_draw_indirect_buffer = create_initialized_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        &initial_debug_draw_command,
        sizeof(initial_debug_draw_command));
    debug_node_list = ROBufferVK::Builder()
                          .set_properties(sizeof(uint32_t),
                              size_t{OCTREE_DEBUG_NODE_LIST_HEADER_COUNT} + OCTREE_MAX_DEBUG_NODE_COUNT,
                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                          .create_ref(base_render_data);
}

void VoxelRenderFeatureOctree::register_render_graph_data(RendGraphDescVK& p_graph) const {
    p_graph.add_resource(OCTREE_INFO, *octree_info)
        .add_resource(OCTREE_STATIC, *static_view)
        .add_resource(OCTREE_DYNAMIC, *dynamic_view)
        .add_resource(OCTREE_DEBUG_DRAW_INDEX_BUFFER, *debug_draw_index_buffer)
        .add_resource(OCTREE_DEBUG_DRAW_INDIRECT_BUFFER, *debug_draw_indirect_buffer)
        .add_resource(OCTREE_DEBUG_LEAF_MASKS, *debug_leaf_masks)
        .add_resource(OCTREE_DEBUG_NODE_DRAW_INDIRECT_BUFFER, *debug_node_draw_indirect_buffer)
        .add_resource(OCTREE_DEBUG_NODE_LIST, *debug_node_list);
    for (uint32_t level = 0; level < OCTREE_LEVEL_COUNT; ++level) {
        p_graph.add_resource(get_static_level_resource_id(level), *static_level_views[level])
            .add_resource(get_dynamic_level_resource_id(level), *dynamic_level_views[level]);
    }
}

RendTaskDescVK VoxelRenderFeatureOctree::make_static_once(RendTaskDescVK p_task, bool p_finish) const {
    if (rebuild_static_every_frame) {
        // The reset stage empties both textures, so every frame rebuilds from an empty octree.
        return p_task;
    }
    p_task.factory = [this, task_factory = p_task.factory, p_finish](const RendGraphContextVK& p_context) {
        auto stage = make_unique<RTVoxelStaticStageVK>(
            render_allocator, task_factory(p_context), static_build_completed, p_finish, "Octree static construction recorded once.");
        return make_unique<RTTriggeredVK>(render_allocator, std::move(stage), true);
    };
    return p_task;
}

RendTaskDescVK VoxelRenderFeatureOctree::make_texture_write(RendTaskDescVK p_task, std::string_view p_texture_view_id) const {
    p_task.factory = [this, task_factory = p_task.factory, texture_view_id = std::string(p_texture_view_id)](
                         const RendGraphContextVK& p_context) {
        return make_unique<RTOctreeTextureWriteVK>(render_allocator, task_factory(p_context), texture_view_id);
    };
    return p_task;
}

RendTaskDescVK VoxelRenderFeatureOctree::get_clear_render_task_desc(std::string_view p_task_id, bool p_clear_static) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = p_task_id;
    task_desc.factory = [this, p_clear_static](const RendGraphContextVK& /*p_context*/) {
        Vector<Ref<ROImageVK>, RenderAllocator> images(render_allocator);
        Vector<std::string, RenderAllocator> written_resource_ids(render_allocator);
        // The full views come first, so the graph's first-use layout transition covers every mip level.
        if (p_clear_static) {
            images.push_back(static_image);
            written_resource_ids.emplace_back(OCTREE_STATIC);
        }
        images.push_back(dynamic_image);
        written_resource_ids.emplace_back(OCTREE_DYNAMIC);
        for (uint32_t level = 0; level < OCTREE_LEVEL_COUNT; ++level) {
            if (p_clear_static) {
                written_resource_ids.push_back(get_static_level_resource_id(level));
            }
            written_resource_ids.push_back(get_dynamic_level_resource_id(level));
        }
        return make_unique<RTOctreeClearVK>(render_allocator, std::move(images), std::move(written_resource_ids));
    };
    return task_desc;
}

RendTaskDescVK VoxelRenderFeatureOctree::get_construction_render_task_desc(
    std::string_view p_task_id, bool p_double_sided, const DynamicMeshRenderFeature* p_dynamic_mesh, size_t p_dynamic_entity_index) const {
    constexpr std::array<VkClearValue, 0> CLEAR_VALUES{};
    constexpr std::array<std::string_view, 0> COLOR_ATTACHMENT_IDS{};
    const VkExtent2D extent{.width = OCTREE_EXTENT, .height = OCTREE_EXTENT};
    if (p_dynamic_mesh != nullptr) {
        return make_texture_write(
            p_dynamic_mesh->make_task(p_task_id, p_dynamic_entity_index, dynamic_build_pipeline, make_fixed_extent_factory(extent)),
            OCTREE_DYNAMIC);
    }
    return make_texture_write(make_mesh_task_desc(render_allocator,
        base_render_data,
        p_task_id,
        static_mesh_resources,
        build_pipeline,
        false,
        CLEAR_VALUES,
        make_fixed_extent_factory(extent),
        COLOR_ATTACHMENT_IDS,
        std::nullopt,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        p_double_sided),
        OCTREE_STATIC);
}

RendTaskDescVK VoxelRenderFeatureOctree::get_compute_task_desc(
    std::string_view p_task_id, const Ref<ROComputePipelineVK>& p_pipeline, VkExtent3D p_group_count) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = p_task_id;
    task_desc.factory = [this, p_pipeline, p_group_count](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTRunComputePipelineVK>(
            render_allocator, base_render_data, p_pipeline, p_group_count, QueueFamilyVK::COMPUTE_1);
    };
    return task_desc;
}

RendTaskDescVK VoxelRenderFeatureOctree::get_debug_draw_render_task_desc(std::string_view p_task_id,
    const Ref<ROGraphicsPipelineVK>& p_pipeline,
    std::string_view p_indirect_buffer_id,
    uint32_t p_draw_count,
    VoxelDebugViewMode p_first_mode) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = p_task_id;
    const std::array<std::string_view, 1> color_targets{REND_RES_SWAP_CHAIN};
    alias_render_targets(task_desc, color_targets, GBUFFER_DEPTH_IMAGE_VIEW);
    RTRunGraphicsPipelineIndirectVK::Builder task_builder(render_allocator);
    task_builder.set_index_buffer(OCTREE_DEBUG_DRAW_INDEX_BUFFER)
        .set_indirect_buffer(p_indirect_buffer_id)
        .set_pipeline(p_pipeline, p_draw_count, VertexInputDescriptionVK{})
        .set_color_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
        .set_depth_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    task_desc.factory = [this, task_builder, p_first_mode](const RendGraphContextVK& p_context) {
        RTRunGraphicsPipelineIndirectVK::Builder builder = task_builder;
        builder.set_render_extent(p_context.swap_chain_extent);
        return make_unique<RTVoxelDebugViewVK>(render_allocator,
            builder.create_unique(base_render_data),
            debug_view_mode,
            p_first_mode,
            VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION);
    };
    return task_desc;
}

void VoxelRenderFeatureOctree::add_build_render_tasks(RendGraphDescVK& p_graph, const DynamicMeshRenderFeature& p_dynamic_mesh) const {
    RendTaskDescVK begin(render_allocator);
    begin.task_id = RT_OCTREE_FRAME_BEGIN;
    begin.factory = [this](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTVoxelFrameBeginVK>(render_allocator, OCTREE_DYNAMIC);
    };
    p_graph.add_task(std::move(begin));
    // Static scene: empty both textures, then occupy the rasterized leaves and their ancestors.
    p_graph.add_task(make_static_once(get_clear_render_task_desc(RT_RESET_OCTREE, true)))
        .add_task(make_static_once(get_construction_render_task_desc(RT_RUN_BUILD_OCTREE, false, nullptr, 0)))
        .add_task(make_static_once(get_construction_render_task_desc(RT_RUN_BUILD_OCTREE_DOUBLE_SIDED, true, nullptr, 0), true));
    // Dynamic meshes: empty the dynamic texture, then rebuild it like the static scene.
    p_graph.add_task(get_clear_render_task_desc(RT_CLEAR_DYNAMIC_OCTREE, false))
        .add_task(get_construction_render_task_desc(RT_RUN_BUILD_DYNAMIC_OCTREE, false, &p_dynamic_mesh, 0))
        .add_task(get_construction_render_task_desc(RT_RUN_BUILD_DYNAMIC_OCTREE_SECOND, false, &p_dynamic_mesh, 1));
    // Debug views: the leaf gather serves both views, and the node gather only the node view.
    RendTaskDescVK gather_leaves = get_compute_task_desc(RT_GATHER_OCTREE_DEBUG_LEAVES, debug_gather_leaves_pipeline, OCTREE_DEBUG_SUBGRID_GRID_EXTENT);
    gather_leaves.factory = [this, task_factory = gather_leaves.factory](const RendGraphContextVK& p_context) {
        return make_unique<RTVoxelDebugViewVK>(render_allocator,
            task_factory(p_context),
            debug_view_mode,
            VoxelDebugViewMode::POST_SVO_BUILD,
            VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION);
    };
    p_graph.add_task(std::move(gather_leaves));
    RendTaskDescVK gather_nodes = get_compute_task_desc(RT_GATHER_OCTREE_DEBUG_NODES,
        debug_gather_nodes_pipeline,
        VkExtent3D{
            .width = OCTREE_DEBUG_GATHER_GROUP_COUNT, .height = OCTREE_DEBUG_GATHER_GROUP_COUNT, .depth = OCTREE_DEBUG_GATHER_GROUP_COUNT});
    gather_nodes.factory = [this, task_factory = gather_nodes.factory](const RendGraphContextVK& p_context) {
        return make_unique<RTVoxelDebugViewVK>(render_allocator,
            task_factory(p_context),
            debug_view_mode,
            VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION,
            VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION);
    };
    p_graph.add_task(std::move(gather_nodes));
}

void VoxelRenderFeatureOctree::add_overlay_render_tasks(RendGraphDescVK& p_graph) const {
    p_graph
        .add_task(get_debug_draw_render_task_desc(
            RT_RUN_OCTREE_DEBUG_DRAW,
            debug_draw_pipeline,
            OCTREE_DEBUG_DRAW_INDIRECT_BUFFER,
            OCTREE_DEBUG_LEAF_DRAW_COUNT,
            VoxelDebugViewMode::POST_SVO_BUILD))
        .add_task(get_debug_draw_render_task_desc(RT_RUN_OCTREE_DEBUG_NODE_DRAW,
            debug_node_draw_pipeline,
            OCTREE_DEBUG_NODE_DRAW_INDIRECT_BUFFER,
            1,
            VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION));
}

void VoxelRenderFeatureOctree::connect_render_tasks(RendGraphDescVK& p_graph) const {
    // The frame-begin barrier orders this frame's mutation after the previous frame's consumers. The debug gathers read the
    // full views after every pass writing the textures.
    connect_resource_chain(p_graph,
        OCTREE_DYNAMIC,
        {RT_OCTREE_FRAME_BEGIN,
            RT_RESET_OCTREE,
            RT_CLEAR_DYNAMIC_OCTREE,
            RT_RUN_BUILD_DYNAMIC_OCTREE,
            RT_RUN_BUILD_DYNAMIC_OCTREE_SECOND,
            RT_GATHER_OCTREE_DEBUG_LEAVES,
            RT_GATHER_OCTREE_DEBUG_NODES});
    connect_resource_chain(p_graph,
        OCTREE_STATIC,
        {RT_RESET_OCTREE, RT_RUN_BUILD_OCTREE, RT_RUN_BUILD_OCTREE_DOUBLE_SIDED, RT_GATHER_OCTREE_DEBUG_LEAVES, RT_GATHER_OCTREE_DEBUG_NODES});
    // The raster passes write every mip level through the per-level views.
    for (uint32_t level = 0; level < OCTREE_LEVEL_COUNT; ++level) {
        connect_resource_chain(
            p_graph, get_static_level_resource_id(level), {RT_RESET_OCTREE, RT_RUN_BUILD_OCTREE, RT_RUN_BUILD_OCTREE_DOUBLE_SIDED});
        connect_resource_chain(p_graph,
            get_dynamic_level_resource_id(level),
            {RT_RESET_OCTREE, RT_CLEAR_DYNAMIC_OCTREE, RT_RUN_BUILD_DYNAMIC_OCTREE, RT_RUN_BUILD_DYNAMIC_OCTREE_SECOND});
    }
    connect_resource_chain(p_graph, OCTREE_DEBUG_DRAW_INDIRECT_BUFFER, {RT_GATHER_OCTREE_DEBUG_LEAVES, RT_RUN_OCTREE_DEBUG_DRAW});
    connect_resource_chain(p_graph, OCTREE_DEBUG_LEAF_MASKS, {RT_GATHER_OCTREE_DEBUG_LEAVES, RT_RUN_OCTREE_DEBUG_DRAW});
    // The leaf gather empties the node view before the node gather fills it.
    connect_resource_chain(p_graph,
        OCTREE_DEBUG_NODE_DRAW_INDIRECT_BUFFER,
        {RT_GATHER_OCTREE_DEBUG_LEAVES, RT_GATHER_OCTREE_DEBUG_NODES, RT_RUN_OCTREE_DEBUG_NODE_DRAW});
    connect_resource_chain(
        p_graph, OCTREE_DEBUG_NODE_LIST, {RT_GATHER_OCTREE_DEBUG_LEAVES, RT_GATHER_OCTREE_DEBUG_NODES, RT_RUN_OCTREE_DEBUG_NODE_DRAW});
}

std::span<const std::string_view> VoxelRenderFeatureOctree::get_overlay_render_task_ids() const {
    return OVERLAY_RENDER_TASK_IDS;
}

} // namespace WhiteBirdEngine
