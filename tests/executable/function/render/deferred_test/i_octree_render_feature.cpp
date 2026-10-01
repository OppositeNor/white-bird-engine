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
#include "i_octree_render_feature.hh"
#include "../render_test.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "dynamic_mesh_render_feature.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/rt_triggered_vk.hh"
#include "function/render_vk/static_set_resources_vk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

/**
 * @brief Shares static-build completion across graph variants; neither copyable nor movable.
 */
class RTOctreeStaticStageVK final : public IRenderTaskVK {
public:
    RTOctreeStaticStageVK() = delete;
    RTOctreeStaticStageVK(Unique<IRenderTaskVK> p_task, Ref<bool> p_completed, bool p_finish)
        : task(std::move(p_task)), completed(std::move(p_completed)), finish(p_finish) {
        WBE_DEBUG_ASSERT(task != MEM_NULL && completed != MEM_NULL);
    }
    ~RTOctreeStaticStageVK() override = default;
    RTOctreeStaticStageVK(const RTOctreeStaticStageVK&) = delete;
    RTOctreeStaticStageVK(RTOctreeStaticStageVK&&) = delete;
    RTOctreeStaticStageVK& operator=(const RTOctreeStaticStageVK&) = delete;
    RTOctreeStaticStageVK& operator=(RTOctreeStaticStageVK&&) = delete;
    QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }
    void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        task->append_accesses(p_reads, p_writes);
    }
    void bind_resources(const RendResViewVK& p_resources) override {
        task->bind_resources(p_resources);
    }
    void record(VkCommandBuffer p_buffer, double p_delta) override {
        if (*completed) {
            return;
        }
        task->record(p_buffer, p_delta);
        if (finish) {
            *completed = true;
            stdout_log(WBE_CHANNEL_DEBUG)->message("Octree static construction recorded once.");
        }
    }

private:
    Unique<IRenderTaskVK> task;
    Ref<bool> completed;
    bool finish;
};

/**
 * @brief Orders mutation of the shared tree after the preceding frame's graphics consumers.
 */
class RTOctreeFrameBeginVK final : public IRenderTaskVK {
public:
    RTOctreeFrameBeginVK() = default;
    ~RTOctreeFrameBeginVK() override = default;
    RTOctreeFrameBeginVK(const RTOctreeFrameBeginVK&) = delete;
    RTOctreeFrameBeginVK(RTOctreeFrameBeginVK&&) = delete;
    RTOctreeFrameBeginVK& operator=(const RTOctreeFrameBeginVK&) = delete;
    RTOctreeFrameBeginVK& operator=(RTOctreeFrameBeginVK&&) = delete;
    QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }
    void append_accesses(Vector<ResAccessVK, RenderAllocator>& /*p_reads*/, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        // The full memory barrier below covers every command, which the octree reset edge synchronizes after.
        p_writes.push_back({.res_id = std::string(IOctreeRenderFeature::OCTREE_INFO), .usage = RendResUsageVK::ALL_COMMANDS});
    }
    void record(VkCommandBuffer p_buffer, double) override {
        VkMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT};
        VkDependencyInfo dependency{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(p_buffer, &dependency);
    }
};

constexpr std::array<uint32_t, 36> OCTREE_DEBUG_CUBE_INDICES{
    0,
    2,
    3,
    0,
    3,
    1,
    4,
    5,
    7,
    4,
    7,
    6,
    0,
    1,
    5,
    0,
    5,
    4,
    2,
    6,
    7,
    2,
    7,
    3,
    0,
    4,
    6,
    0,
    6,
    2,
    1,
    3,
    7,
    1,
    7,
    5,
};

/**
 * @brief Records a retained debug task only for the selected view; neither copyable nor movable.
 */
class RTVoxelDebugView final : public IRenderTaskVK {
public:
    RTVoxelDebugView() = delete;
    /**
     * @brief Retains the task and borrows debug-mode state that outlives graph execution.
     */
    RTVoxelDebugView(Unique<IRenderTaskVK> p_task, const std::atomic<VoxelDebugViewMode>* p_mode, VoxelDebugViewMode p_first_mode, VoxelDebugViewMode p_last_mode)
        : task(std::move(p_task)), mode(p_mode), first_mode(p_first_mode), last_mode(p_last_mode) {
        if (task == MEM_NULL || mode == nullptr) {
            throw std::runtime_error("Failed to create the voxel debug-view task: task or mode state is null.");
        }
        if (first_mode > last_mode) {
            throw std::runtime_error("Failed to create the voxel debug-view task: mode range is invalid.");
        }
    }
    ~RTVoxelDebugView() override = default;
    RTVoxelDebugView(const RTVoxelDebugView&) = delete;
    RTVoxelDebugView(RTVoxelDebugView&&) noexcept = delete;
    RTVoxelDebugView& operator=(const RTVoxelDebugView&) = delete;
    RTVoxelDebugView& operator=(RTVoxelDebugView&&) noexcept = delete;

    QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }

    void bind_resources(const RendResViewVK& p_resources) override {
        task->bind_resources(p_resources);
    }

    void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        task->append_accesses(p_reads, p_writes);
    }

    void record(VkCommandBuffer p_command_buffer, double p_delta) override {
        VoxelDebugViewMode current_mode = mode->load(std::memory_order_relaxed);
        if (current_mode >= first_mode && current_mode <= last_mode) {
            task->record(p_command_buffer, p_delta);
        }
    }

private:
    Unique<IRenderTaskVK> task;
    const std::atomic<VoxelDebugViewMode>* mode;
    VoxelDebugViewMode first_mode;
    VoxelDebugViewMode last_mode;
};

} // namespace

IOctreeRenderFeature::IOctreeRenderFeature(RendererVK* p_renderer,
    Ref<StaticSetResourcesVK> p_static_set_resources,
    const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
    std::string_view p_shader_prefix,
    std::string_view p_dynamic_shader_prefix,
    bool p_rebuild_static_every_frame)
    : renderer_vk(p_renderer), base_render_data(nullptr), render_allocator(nullptr),
      static_set_resources(std::move(p_static_set_resources)), debug_view_mode(p_debug_view_mode),
      rebuild_static_every_frame(p_rebuild_static_every_frame) {
    if (renderer_vk == nullptr || static_set_resources == MEM_NULL || debug_view_mode == nullptr) {
        throw std::runtime_error("IOctreeRenderFeature: renderer, scene and debug state are required.");
    }
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = base_render_data->allocator;
    static_build_completed = make_ref<bool>(render_allocator, false);
    compile_pipelines(p_shader_prefix, p_dynamic_shader_prefix);
    create_resources();
}

Ref<ROComputePipelineVK> IOctreeRenderFeature::compile_compute_pipeline(std::string_view p_resource_id) const {
    ComputePipelineCompilerVK compiler(ComputePipelineCompilerVKContext{
        .allocator = render_allocator, .render_data = base_render_data, .render_database = renderer_vk->get_render_database()});
    return compiler.compile(p_resource_id, ResourceManager::get_singleton()->get_resource<RUShader>(p_resource_id)).compute_pipeline;
}

void IOctreeRenderFeature::compile_pipelines(std::string_view p_shader_prefix, std::string_view p_dynamic_shader_prefix) {
    auto* resource_manager = ResourceManager::get_singleton();
    GraphicsPipelineCompilerVK octree_compiler(GraphicsPipelineCompilerVKContext{.allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk});
    const std::string build_pipeline_id = std::format("{}_build_static_set_pipeline", p_shader_prefix);
    octree_build_graphics_pipeline =
        octree_compiler.compile(build_pipeline_id, resource_manager->get_resource<RUGraphicsPipeline>(build_pipeline_id)).graphics_pipeline;
    const std::string dynamic_build_pipeline_id = std::format("{}_build_static_set_pipeline", p_dynamic_shader_prefix);
    dynamic_build_pipeline =
        octree_compiler
            .compile(dynamic_build_pipeline_id, resource_manager->get_resource<RUGraphicsPipeline>(dynamic_build_pipeline_id))
            .graphics_pipeline;
    dynamic_clear_pipeline = compile_compute_pipeline(std::format("{}_clear_shader", p_dynamic_shader_prefix));
    dynamic_ping_pipeline =
        compile_compute_pipeline(std::format("{}_build_deferred_propagation_pong_write_shader", p_dynamic_shader_prefix));
    dynamic_pong_pipeline =
        compile_compute_pipeline(std::format("{}_build_deferred_propagation_ping_write_shader", p_dynamic_shader_prefix));
    octree_reset_compute_pipeline = compile_compute_pipeline(std::format("{}_reset_shader", p_shader_prefix));
    octree_deferred_count_reset_ping_compute_pipeline = compile_compute_pipeline("octree_deferred_count_reset_ping_shader");
    octree_deferred_count_reset_pong_compute_pipeline = compile_compute_pipeline("octree_deferred_count_reset_pong_shader");
    octree_deferred_propagation_ping_write_compute_pipeline =
        compile_compute_pipeline(std::format("{}_build_deferred_propagation_ping_write_shader", p_shader_prefix));
    octree_deferred_propagation_pong_write_compute_pipeline =
        compile_compute_pipeline(std::format("{}_build_deferred_propagation_pong_write_shader", p_shader_prefix));
    octree_pre_deferred_debug_capture_compute_pipeline = compile_compute_pipeline("octree_pre_deferred_debug_capture_shader");
    octree_debug_draw_setup_compute_pipeline = compile_compute_pipeline("octree_debug_draw_setup_shader");
    GraphicsPipelineCompilerVKContext octree_debug_draw_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
        .depth_attachment_format = GBUFFER_DEPTH_FORMAT,
        .polygon_mode = VK_POLYGON_MODE_LINE,
    };
    octree_debug_draw_graphics_pipeline =
        GraphicsPipelineCompilerVK(octree_debug_draw_context)
            .compile("octree_debug_draw_pipeline", resource_manager->get_resource<RUGraphicsPipeline>("octree_debug_draw_pipeline"))
            .graphics_pipeline;
    octree_pre_deferred_debug_draw_graphics_pipeline =
        GraphicsPipelineCompilerVK(octree_debug_draw_context)
            .compile("octree_pre_deferred_debug_draw_pipeline",
                resource_manager->get_resource<RUGraphicsPipeline>("octree_pre_deferred_debug_draw_pipeline"))
            .graphics_pipeline;
}

void IOctreeRenderFeature::create_resources() {
    octree_info = ROBufferVK::Builder().set_properties(sizeof(OctreeInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    octree_nodes =
        ROBufferVK::Builder().set_properties(sizeof(OctreeNode), OCTREE_MAX_NODE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    dynamic_nodes = ROBufferVK::Builder()
                        .set_properties(sizeof(OctreeNode), OCTREE_MAX_DYNAMIC_NODE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                        .create_ref(base_render_data);
    dynamic_leaves = ROBufferVK::Builder()
                         .set_properties(sizeof(OctreeDynamicLeafBlock), OCTREE_MAX_DYNAMIC_LEAVES_COUNT / 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                         .create_ref(base_render_data);
    octree_leaves =
        ROBufferVK::Builder().set_properties(sizeof(OctreeLeaf), OCTREE_MAX_LEAVES_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    octree_deferred_list_ping =
        ROBufferVK::Builder()
            .set_properties(sizeof(uint32_t), OCTREE_MAX_DEFERRED_TASK_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            .create_ref(base_render_data);
    octree_deferred_list_pong =
        ROBufferVK::Builder()
            .set_properties(sizeof(uint32_t), OCTREE_MAX_DEFERRED_TASK_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            .create_ref(base_render_data);
    octree_debug_draw_index_buffer = create_initialized_buffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, OCTREE_DEBUG_CUBE_INDICES.data(), sizeof(OCTREE_DEBUG_CUBE_INDICES));
    const VkDrawIndexedIndirectCommand initial_debug_draw_command{
        .indexCount = OCTREE_DEBUG_CUBE_INDICES.size(),
        .instanceCount = 0,
        .firstIndex = 0,
        .vertexOffset = 0,
        .firstInstance = 0,
    };
    octree_debug_draw_indirect_buffer = create_initialized_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        &initial_debug_draw_command,
        sizeof(initial_debug_draw_command));
    octree_pre_deferred_debug_info_buffer =
        ROBufferVK::Builder().set_properties(sizeof(OctreeInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    octree_pre_deferred_debug_list_buffer =
        ROBufferVK::Builder()
            .set_properties(sizeof(uint32_t), OCTREE_MAX_DEFERRED_TASK_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            .create_ref(base_render_data);
    octree_pre_deferred_debug_draw_indirect_buffer =
        create_initialized_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            &initial_debug_draw_command,
            sizeof(initial_debug_draw_command));
    voxel_claims =
        ROBufferVK::Builder().set_properties(sizeof(uint32_t), OCTREE_CLAIM_WORD_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    initialize();
}

void IOctreeRenderFeature::initialize() {
    const OctreeInfo initial_octree_info{
        .origin = OCTREE_ORIGIN,
        .width = OCTREE_WIDTH,
        .max_depth = OCTREE_MAX_DEPTH,
        .max_node_count = OCTREE_MAX_NODE_COUNT,
        .max_leaves_count = OCTREE_MAX_LEAVES_COUNT,
        .node_count = 8,
        .leaves_count = 0,
        .data_count = 8,
        .deferred_task_count_ping = 0,
        .deferred_task_count_pong = 0,
        .max_deferred_task_count = OCTREE_MAX_DEFERRED_TASK_COUNT,
        .max_dynamic_node_count = OCTREE_MAX_DYNAMIC_NODE_COUNT,
        .max_dynamic_leaves_count = OCTREE_MAX_DYNAMIC_LEAVES_COUNT,
        .dynamic_node_count = 0,
        .dynamic_leaves_count = 0,
    };
    std::array<OctreeNode, 8> first_sectors{};
    for (uint32_t i = 0; i < first_sectors.size(); ++i) {
        first_sectors[i] = OctreeNode{
            .children_data = static_cast<uint32_t>(OctreeNode::ChildrenState::EMPTY) << 29U,
            .data_index = i,
            .parent_index = -1,
        };
    }
    octree_info->update_buffer(&initial_octree_info, sizeof(initial_octree_info));
    octree_nodes->update_buffer(first_sectors.data(), sizeof(first_sectors));
}

RendTaskDescVK IOctreeRenderFeature::get_reset_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RESET_OCTREE;
    task_desc.task = make_unique<RTRunComputePipelineVK>(render_allocator,
        base_render_data,
        octree_reset_compute_pipeline,
        VkExtent3D{.width = get_reset_group_count(), .height = 1, .depth = 1});
    return task_desc;
}

RendTaskDescVK IOctreeRenderFeature::get_construction_render_task_desc(
    std::string_view p_task_id, bool p_double_sided, const DynamicMeshRenderFeature* p_dynamic_mesh) const {
    constexpr std::array<VkClearValue, 0> CLEAR_VALUES{};
    constexpr std::array<std::string_view, 0> COLOR_ATTACHMENT_IDS{};
    RendTaskDescVK task_desc =
        p_dynamic_mesh != nullptr
            ? p_dynamic_mesh->make_task(p_task_id, dynamic_build_pipeline, VkExtent2D{.width = OCTREE_EXTENT, .height = OCTREE_EXTENT})
            : make_static_set_task_desc(render_allocator,
                  base_render_data,
                  p_task_id,
                  static_set_resources,
                  octree_build_graphics_pipeline,
                  false,
                  CLEAR_VALUES,
                  VkExtent2D{.width = OCTREE_EXTENT, .height = OCTREE_EXTENT},
                  COLOR_ATTACHMENT_IDS,
                  std::nullopt,
                  VK_ATTACHMENT_LOAD_OP_CLEAR,
                  VK_ATTACHMENT_LOAD_OP_CLEAR,
                  p_double_sided);
    return task_desc;
}

RendTaskDescVK IOctreeRenderFeature::get_deferred_construction_render_task_desc(bool p_dynamic) const {
    RendTaskDescVK task_desc = make_deferred_construction_render_task_desc(p_dynamic);
    task_desc.task_id = RT_RUN_BUILD_OCTREE_DEFERRED;
    return task_desc;
}

RendTaskDescVK IOctreeRenderFeature::get_debug_capture_render_task_desc(std::string_view p_task_id, VoxelDebugViewMode p_mode) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = p_task_id;
    Unique<IRenderTaskVK> task = make_unique<RTRunComputePipelineVK>(render_allocator,
        base_render_data,
        octree_pre_deferred_debug_capture_compute_pipeline,
        VkExtent3D{.width = OCTREE_DEFERRED_DISPATCH_GROUP_COUNT, .height = 1, .depth = 1});
    task_desc.task = make_unique<RTVoxelDebugView>(render_allocator, std::move(task), debug_view_mode, p_mode, p_mode);
    return task_desc;
}

RendTaskDescVK IOctreeRenderFeature::get_debug_draw_setup_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_SETUP_OCTREE_DEBUG_DRAW;
    task_desc.task = make_unique<RTRunComputePipelineVK>(
        render_allocator, base_render_data, octree_debug_draw_setup_compute_pipeline, VkExtent3D{.width = 1, .height = 1, .depth = 1});
    return task_desc;
}

RendTaskDescVK IOctreeRenderFeature::get_debug_draw_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_OCTREE_DEBUG_DRAW;
    const std::array<std::string_view, 1> color_targets{REND_RES_SWAP_CHAIN};
    alias_render_targets(task_desc, color_targets, GBUFFER_DEPTH_IMAGE_VIEW);
    RTRunGraphicsPipelineIndirectVK::Builder task_builder(render_allocator);
    task_builder.set_render_extent(base_render_data->swap_chain_extent)
        .set_index_buffer(OCTREE_DEBUG_DRAW_INDEX_BUFFER)
        .set_indirect_buffer(OCTREE_DEBUG_DRAW_INDIRECT_BUFFER)
        .set_pipeline(octree_debug_draw_graphics_pipeline, 1, VertexInputDescriptionVK{})
        .set_color_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
        .set_depth_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    task_desc.task = make_unique<RTVoxelDebugView>(render_allocator,
        task_builder.create_unique(base_render_data),
        debug_view_mode,
        VoxelDebugViewMode::POST_OCTREE_BUILD,
        VoxelDebugViewMode::POST_OCTREE_BUILD);
    return task_desc;
}

RendTaskDescVK IOctreeRenderFeature::get_pre_deferred_debug_draw_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW;
    const std::array<std::string_view, 1> color_targets{REND_RES_SWAP_CHAIN};
    alias_render_targets(task_desc, color_targets, GBUFFER_DEPTH_IMAGE_VIEW);
    RTRunGraphicsPipelineIndirectVK::Builder task_builder(render_allocator);
    task_builder.set_render_extent(base_render_data->swap_chain_extent)
        .set_index_buffer(OCTREE_DEBUG_DRAW_INDEX_BUFFER)
        .set_indirect_buffer(OCTREE_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER)
        .set_pipeline(octree_pre_deferred_debug_draw_graphics_pipeline, 1, VertexInputDescriptionVK{})
        .set_color_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
        .set_depth_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    task_desc.task = make_unique<RTVoxelDebugView>(render_allocator,
        task_builder.create_unique(base_render_data),
        debug_view_mode,
        VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION,
        VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION);
    return task_desc;
}

Ref<ROBufferVK> IOctreeRenderFeature::create_initialized_buffer(VkBufferUsageFlags p_usage, const void* p_data, size_t p_size) const {
    return ROBufferVK::Builder().set_usage(p_usage).initial_data(p_data, p_size).create_ref(base_render_data);
}

void IOctreeRenderFeature::register_render_graph_data(RendGraphDescVK& p_graph) const {
    p_graph.add_resource(OCTREE_DYNAMIC_NODES, dynamic_nodes).add_resource(OCTREE_DYNAMIC_LEAVES, dynamic_leaves);
    p_graph.add_resource(OCTREE_INFO, octree_info)
        .add_resource(OCTREE_NODE_BUFFER, octree_nodes)
        .add_resource(OCTREE_LEAF_BUFFER, octree_leaves)
        .add_resource(OCTREE_DEFERRED_LIST_PING, octree_deferred_list_ping)
        .add_resource(OCTREE_DEFERRED_LIST_PONG, octree_deferred_list_pong)
        .add_resource(OCTREE_DEBUG_DRAW_INDEX_BUFFER, octree_debug_draw_index_buffer)
        .add_resource(OCTREE_DEBUG_DRAW_INDIRECT_BUFFER, octree_debug_draw_indirect_buffer)
        .add_resource(OCTREE_PRE_DEFERRED_DEBUG_INFO_BUFFER, octree_pre_deferred_debug_info_buffer)
        .add_resource(OCTREE_PRE_DEFERRED_DEBUG_LIST_BUFFER, octree_pre_deferred_debug_list_buffer)
        .add_resource(OCTREE_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER, octree_pre_deferred_debug_draw_indirect_buffer);
    p_graph.add_resource(OCTREE_VOXEL_CLAIMS, voxel_claims);
    register_implementation_render_graph_data(p_graph);
}

void IOctreeRenderFeature::add_construction_render_tasks(RendGraphDescVK& p_graph, const DynamicMeshRenderFeature& p_dynamic_mesh) const {
    RendTaskDescVK begin(render_allocator);
    begin.task_id = RT_OCTREE_FRAME_BEGIN;
    begin.task = make_unique<RTOctreeFrameBeginVK>(render_allocator);
    p_graph.add_task(std::move(begin));
    p_graph.add_task(make_static_once(get_reset_render_task_desc()))
        .add_task(make_static_once(get_construction_render_task_desc(RT_RUN_BUILD_OCTREE)))
        .add_task(make_static_once(get_construction_render_task_desc(RT_RUN_BUILD_OCTREE_DOUBLE_SIDED, true)));
    add_static_publication_render_tasks(p_graph);
    auto static_deferred = get_deferred_construction_render_task_desc(false);
    static_deferred.task_id = RT_STATIC_OCTREE_DEFERRED;
    p_graph.add_task(make_static_once(std::move(static_deferred), true));
    p_graph.add_task(get_dynamic_clear_task_desc());
    p_graph.add_task(get_construction_render_task_desc(RT_RUN_BUILD_DYNAMIC_OCTREE, false, &p_dynamic_mesh));
    add_dynamic_publication_render_tasks(p_graph);
    p_graph.add_task(get_debug_capture_render_task_desc(RT_CAPTURE_OCTREE_PRE_DEFERRED_DEBUG, VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION));
}

void IOctreeRenderFeature::connect_render_tasks(RendGraphDescVK& p_graph) const {
    // The frame-begin barrier orders this frame's octree mutation after the previous frame's consumers.
    p_graph.get_task(RT_OCTREE_FRAME_BEGIN).connect(RT_RESET_OCTREE, OCTREE_INFO);
    connect_implementation_render_tasks(p_graph);
    connect_resource_chain(p_graph,
        OCTREE_LEAF_BUFFER,
        {RT_RUN_BUILD_OCTREE,
            RT_RUN_BUILD_OCTREE_DOUBLE_SIDED,
            RT_STATIC_OCTREE_DEFERRED,
            RT_RUN_BUILD_DYNAMIC_OCTREE,
            RT_RUN_BUILD_OCTREE_DEFERRED,
            RT_RUN_OCTREE_DEBUG_DRAW,
            RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(p_graph,
        OCTREE_DEFERRED_LIST_PING,
        {RT_RUN_BUILD_OCTREE, RT_RUN_BUILD_OCTREE_DOUBLE_SIDED, RT_STATIC_OCTREE_DEFERRED, RT_RUN_BUILD_DYNAMIC_OCTREE, RT_CAPTURE_OCTREE_PRE_DEFERRED_DEBUG, RT_RUN_BUILD_OCTREE_DEFERRED});
    connect_resource_chain(p_graph, OCTREE_DEFERRED_LIST_PONG, {RT_STATIC_OCTREE_DEFERRED, RT_RUN_BUILD_OCTREE_DEFERRED});
    connect_resource_chain(
        p_graph, OCTREE_PRE_DEFERRED_DEBUG_INFO_BUFFER, {RT_CAPTURE_OCTREE_PRE_DEFERRED_DEBUG, RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(
        p_graph, OCTREE_PRE_DEFERRED_DEBUG_LIST_BUFFER, {RT_CAPTURE_OCTREE_PRE_DEFERRED_DEBUG, RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(p_graph,
        OCTREE_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER,
        {RT_CAPTURE_OCTREE_PRE_DEFERRED_DEBUG, RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(p_graph, OCTREE_DEBUG_DRAW_INDIRECT_BUFFER, {RT_SETUP_OCTREE_DEBUG_DRAW, RT_RUN_OCTREE_DEBUG_DRAW});
}

RendTaskDescVK IOctreeRenderFeature::make_static_once(RendTaskDescVK p_task, bool p_finish) const {
    if (rebuild_static_every_frame) {
        // The reset stage clears the counters and root nodes, so every frame rebuilds from an empty tree.
        return p_task;
    }
    auto stage = make_unique<RTOctreeStaticStageVK>(render_allocator, std::move(p_task.task), static_build_completed, p_finish);
    p_task.task = make_unique<RTTriggeredVK>(render_allocator, std::move(stage), true);
    return p_task;
}

RendTaskDescVK IOctreeRenderFeature::get_dynamic_clear_task_desc() const {
    RendTaskDescVK task(render_allocator);
    task.task_id = RT_CLEAR_DYNAMIC_OCTREE;
    constexpr uint32_t GROUPS = static_cast<uint32_t>((std::max(OCTREE_MAX_NODE_COUNT, OCTREE_CLAIM_WORD_COUNT) + 127) / 128);
    task.task = make_unique<RTRunComputePipelineVK>(
        render_allocator, base_render_data, dynamic_clear_pipeline, VkExtent3D{.width = GROUPS, .height = 1, .depth = 1});
    return task;
}

} // namespace WhiteBirdEngine
