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
#include "i_svo_render_feature.hh"
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
#include "function/render_vk/mesh_resources_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/rt_triggered_vk.hh"
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
class RTSVOStaticStageVK final : public IRenderTaskVK {
public:
    RTSVOStaticStageVK() = delete;
    RTSVOStaticStageVK(Unique<IRenderTaskVK> p_task, Ref<bool> p_completed, bool p_finish)
        : task(std::move(p_task)), completed(std::move(p_completed)), finish(p_finish) {
        WBE_DEBUG_ASSERT(task != MEM_NULL && completed != MEM_NULL);
    }
    virtual ~RTSVOStaticStageVK() override = default;
    RTSVOStaticStageVK(const RTSVOStaticStageVK&) = delete;
    RTSVOStaticStageVK(RTSVOStaticStageVK&&) = delete;
    RTSVOStaticStageVK& operator=(const RTSVOStaticStageVK&) = delete;
    RTSVOStaticStageVK& operator=(RTSVOStaticStageVK&&) = delete;
    virtual QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }
    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        task->append_accesses(p_reads, p_writes);
    }
    virtual void bind_resources(const RendResViewVK& p_resources) override {
        task->bind_resources(p_resources);
    }
    virtual void record(VkCommandBuffer p_buffer, double p_delta) override {
        if (*completed) {
            return;
        }
        task->record(p_buffer, p_delta);
        if (finish) {
            *completed = true;
            stdout_log(WBE_CHANNEL_DEBUG)->message("SVO static construction recorded once.");
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
class RTSVOFrameBeginVK final : public IRenderTaskVK {
public:
    RTSVOFrameBeginVK() = default;
    virtual ~RTSVOFrameBeginVK() override = default;
    RTSVOFrameBeginVK(const RTSVOFrameBeginVK&) = delete;
    RTSVOFrameBeginVK(RTSVOFrameBeginVK&&) = delete;
    RTSVOFrameBeginVK& operator=(const RTSVOFrameBeginVK&) = delete;
    RTSVOFrameBeginVK& operator=(RTSVOFrameBeginVK&&) = delete;
    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }
    virtual void append_accesses(
        Vector<ResAccessVK, RenderAllocator>& /*p_reads*/, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        // The full memory barrier below covers every command, which the SVO reset edge synchronizes after.
        p_writes.push_back({.res_id = std::string(ISVORenderFeature::SVO_INFO), .usage = RendResUsageVK::ALL_COMMANDS});
    }
    virtual void record(VkCommandBuffer p_buffer, double) override {
        VkMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
        };
        VkDependencyInfo dependency{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(p_buffer, &dependency);
    }
};

constexpr std::array<uint32_t, 36> SVO_DEBUG_CUBE_INDICES{
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
    virtual ~RTVoxelDebugView() override = default;
    RTVoxelDebugView(const RTVoxelDebugView&) = delete;
    RTVoxelDebugView(RTVoxelDebugView&&) noexcept = delete;
    RTVoxelDebugView& operator=(const RTVoxelDebugView&) = delete;
    RTVoxelDebugView& operator=(RTVoxelDebugView&&) noexcept = delete;

    virtual QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }

    virtual void bind_resources(const RendResViewVK& p_resources) override {
        task->bind_resources(p_resources);
    }

    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        task->append_accesses(p_reads, p_writes);
    }

    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override {
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

ISVORenderFeature::ISVORenderFeature(RendererVK* p_renderer,
    Ref<MeshResourcesVK> p_static_mesh_resources,
    const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
    std::string_view p_shader_prefix,
    std::string_view p_dynamic_shader_prefix,
    bool p_rebuild_static_every_frame)
    : renderer_vk(p_renderer), base_render_data(nullptr), render_allocator(nullptr),
      static_mesh_resources(std::move(p_static_mesh_resources)), debug_view_mode(p_debug_view_mode),
      rebuild_static_every_frame(p_rebuild_static_every_frame) {
    if (renderer_vk == nullptr || static_mesh_resources == MEM_NULL || debug_view_mode == nullptr) {
        throw std::runtime_error("ISVORenderFeature: renderer, scene and debug state are required.");
    }
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = base_render_data->allocator;
    static_build_completed = make_ref<bool>(render_allocator, false);
    compile_pipelines(p_shader_prefix, p_dynamic_shader_prefix);
    create_resources();
}

Ref<ROComputePipelineVK> ISVORenderFeature::compile_compute_pipeline(std::string_view p_resource_id) const {
    ComputePipelineCompilerVK compiler(ComputePipelineCompilerVKContext{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
    });
    return compiler.compile(p_resource_id, ResourceManager::get_singleton()->get_resource<RUShader>(p_resource_id)).compute_pipeline;
}

void ISVORenderFeature::compile_pipelines(std::string_view p_shader_prefix, std::string_view p_dynamic_shader_prefix) {
    auto* resource_manager = ResourceManager::get_singleton();
    GraphicsPipelineCompilerVK svo_compiler(GraphicsPipelineCompilerVKContext{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
    });
    const std::string build_pipeline_id = std::format("{}_build_static_mesh_pipeline", p_shader_prefix);
    svo_build_graphics_pipeline =
        svo_compiler.compile(build_pipeline_id, resource_manager->get_resource<RUGraphicsPipeline>(build_pipeline_id)).graphics_pipeline;
    const std::string dynamic_build_pipeline_id = std::format("{}_build_static_mesh_pipeline", p_dynamic_shader_prefix);
    dynamic_build_pipeline =
        svo_compiler
            .compile(dynamic_build_pipeline_id, resource_manager->get_resource<RUGraphicsPipeline>(dynamic_build_pipeline_id))
            .graphics_pipeline;
    dynamic_clear_pipeline = compile_compute_pipeline(std::format("{}_clear_shader", p_dynamic_shader_prefix));
    dynamic_ping_pipeline =
        compile_compute_pipeline(std::format("{}_build_deferred_propagation_pong_write_shader", p_dynamic_shader_prefix));
    dynamic_pong_pipeline =
        compile_compute_pipeline(std::format("{}_build_deferred_propagation_ping_write_shader", p_dynamic_shader_prefix));
    svo_reset_compute_pipeline = compile_compute_pipeline(std::format("{}_reset_shader", p_shader_prefix));
    svo_deferred_count_reset_ping_compute_pipeline = compile_compute_pipeline("svo_deferred_count_reset_ping_shader");
    svo_deferred_count_reset_pong_compute_pipeline = compile_compute_pipeline("svo_deferred_count_reset_pong_shader");
    svo_deferred_propagation_ping_write_compute_pipeline =
        compile_compute_pipeline(std::format("{}_build_deferred_propagation_ping_write_shader", p_shader_prefix));
    svo_deferred_propagation_pong_write_compute_pipeline =
        compile_compute_pipeline(std::format("{}_build_deferred_propagation_pong_write_shader", p_shader_prefix));
    svo_pre_deferred_debug_capture_compute_pipeline = compile_compute_pipeline("svo_pre_deferred_debug_capture_shader");
    svo_debug_draw_setup_compute_pipeline = compile_compute_pipeline("svo_debug_draw_setup_shader");
    GraphicsPipelineCompilerVKContext svo_debug_draw_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
        .depth_attachment_format = GBUFFER_DEPTH_FORMAT,
        .polygon_mode = VK_POLYGON_MODE_LINE,
    };
    svo_debug_draw_graphics_pipeline =
        GraphicsPipelineCompilerVK(svo_debug_draw_context)
            .compile("svo_debug_draw_pipeline", resource_manager->get_resource<RUGraphicsPipeline>("svo_debug_draw_pipeline"))
            .graphics_pipeline;
    svo_pre_deferred_debug_draw_graphics_pipeline =
        GraphicsPipelineCompilerVK(svo_debug_draw_context)
            .compile("svo_pre_deferred_debug_draw_pipeline",
                resource_manager->get_resource<RUGraphicsPipeline>("svo_pre_deferred_debug_draw_pipeline"))
            .graphics_pipeline;
}

void ISVORenderFeature::create_resources() {
    svo_info = ROBufferVK::Builder().set_properties(sizeof(SVOInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    svo_nodes =
        ROBufferVK::Builder().set_properties(sizeof(SVONode), SVO_MAX_NODE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    dynamic_nodes = ROBufferVK::Builder()
                        .set_properties(sizeof(SVONode), SVO_MAX_DYNAMIC_NODE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                        .create_ref(base_render_data);
    dynamic_leaves = ROBufferVK::Builder()
                         .set_properties(sizeof(SVODynamicLeafBlock), SVO_MAX_DYNAMIC_LEAVES_COUNT / 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                         .create_ref(base_render_data);
    svo_leaves =
        ROBufferVK::Builder().set_properties(sizeof(SVOLeaf), SVO_MAX_LEAVES_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    svo_deferred_list_ping = ROBufferVK::Builder()
                                    .set_properties(sizeof(uint32_t), SVO_DEFERRED_LIST_ELEMENT_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                                    .create_ref(base_render_data);
    svo_deferred_list_pong = ROBufferVK::Builder()
                                    .set_properties(sizeof(uint32_t), SVO_DEFERRED_LIST_ELEMENT_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                                    .create_ref(base_render_data);
    svo_debug_draw_index_buffer = create_initialized_buffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, SVO_DEBUG_CUBE_INDICES.data(), sizeof(SVO_DEBUG_CUBE_INDICES));
    const VkDrawIndexedIndirectCommand initial_debug_draw_command{
        .indexCount = SVO_DEBUG_CUBE_INDICES.size(),
        .instanceCount = 0,
        .firstIndex = 0,
        .vertexOffset = 0,
        .firstInstance = 0,
    };
    svo_debug_draw_indirect_buffer = create_initialized_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        &initial_debug_draw_command,
        sizeof(initial_debug_draw_command));
    svo_pre_deferred_debug_info_buffer =
        ROBufferVK::Builder().set_properties(sizeof(SVOInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    svo_pre_deferred_debug_list_buffer =
        ROBufferVK::Builder()
            .set_properties(sizeof(uint32_t), SVO_MAX_DEFERRED_TASK_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            .create_ref(base_render_data);
    svo_pre_deferred_debug_draw_indirect_buffer =
        create_initialized_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            &initial_debug_draw_command,
            sizeof(initial_debug_draw_command));
    initialize();
}

void ISVORenderFeature::initialize() {
    const SVOInfo initial_svo_info{
        .origin = SVO_ORIGIN,
        .width = SVO_WIDTH,
        .max_depth = SVO_MAX_DEPTH,
        .max_node_count = SVO_MAX_NODE_COUNT,
        .max_leaves_count = SVO_MAX_LEAVES_COUNT,
        .node_count = 8,
        .leaves_count = 0,
        .data_count = 8,
        .max_deferred_task_count = SVO_MAX_DEFERRED_TASK_COUNT,
        .max_dynamic_node_count = SVO_MAX_DYNAMIC_NODE_COUNT,
        .max_dynamic_leaves_count = SVO_MAX_DYNAMIC_LEAVES_COUNT,
        .dynamic_node_count = 0,
        .dynamic_leaves_count = 0,
    };
    std::array<SVONode, 8> first_sectors{};
    for (uint32_t i = 0; i < first_sectors.size(); ++i) {
        first_sectors[i] = SVONode{
            .children_data = static_cast<uint32_t>(SVONode::ChildrenState::EMPTY) << 29U,
            .data_index = i,
            .parent_index = -1,
        };
    }
    svo_info->update_buffer(&initial_svo_info, sizeof(initial_svo_info));
    svo_nodes->update_buffer(first_sectors.data(), sizeof(first_sectors));
}

RendTaskDescVK ISVORenderFeature::get_reset_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RESET_SVO;
    task_desc.factory = [this](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTRunComputePipelineVK>(render_allocator,
            base_render_data,
            svo_reset_compute_pipeline,
            VkExtent3D{.width = get_reset_group_count(), .height = 1, .depth = 1},
            QueueFamilyVK::COMPUTE_1);
    };
    alias_mesh_resources(task_desc, *svo_reset_compute_pipeline, make_mesh_resource_keys(STATIC_MESH_KEY));
    return task_desc;
}

RendTaskDescVK ISVORenderFeature::get_construction_render_task_desc(
    std::string_view p_task_id, bool p_double_sided, const DynamicMeshRenderFeature* p_dynamic_mesh, size_t p_dynamic_entity_index) const {
    constexpr std::array<VkClearValue, 0> CLEAR_VALUES{};
    constexpr std::array<std::string_view, 0> COLOR_ATTACHMENT_IDS{};
    RendTaskDescVK task_desc = p_dynamic_mesh != nullptr
                                   ? p_dynamic_mesh->make_task(p_task_id,
                                         p_dynamic_entity_index,
                                         dynamic_build_pipeline,
                                         make_fixed_extent_factory(VkExtent2D{.width = SVO_EXTENT, .height = SVO_EXTENT}))
                                   : make_mesh_task_desc(render_allocator,
                                         base_render_data,
                                         p_task_id,
                                         static_mesh_resources,
                                         svo_build_graphics_pipeline,
                                         false,
                                         CLEAR_VALUES,
                                         make_fixed_extent_factory(VkExtent2D{.width = SVO_EXTENT, .height = SVO_EXTENT}),
                                         COLOR_ATTACHMENT_IDS,
                                         std::nullopt,
                                         VK_ATTACHMENT_LOAD_OP_CLEAR,
                                         VK_ATTACHMENT_LOAD_OP_CLEAR,
                                         p_double_sided);
    return task_desc;
}

RendTaskDescVK ISVORenderFeature::get_deferred_construction_render_task_desc(bool p_dynamic) const {
    RendTaskDescVK task_desc = make_deferred_construction_render_task_desc(p_dynamic);
    task_desc.task_id = RT_RUN_BUILD_SVO_DEFERRED;
    return task_desc;
}

RendTaskDescVK ISVORenderFeature::get_debug_capture_render_task_desc(std::string_view p_task_id, VoxelDebugViewMode p_mode) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = p_task_id;
    task_desc.factory = [this, p_mode](const RendGraphContextVK& /*p_context*/) {
        Unique<IRenderTaskVK> task = make_unique<RTRunComputePipelineVK>(render_allocator,
            base_render_data,
            svo_pre_deferred_debug_capture_compute_pipeline,
            VkExtent3D{.width = SVO_DEFERRED_DISPATCH_GROUP_COUNT, .height = 1, .depth = 1},
            QueueFamilyVK::COMPUTE_1);
        return make_unique<RTVoxelDebugView>(render_allocator, std::move(task), debug_view_mode, p_mode, p_mode);
    };
    return task_desc;
}

RendTaskDescVK ISVORenderFeature::get_debug_draw_setup_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_SETUP_SVO_DEBUG_DRAW;
    task_desc.factory = [this](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTRunComputePipelineVK>(
            render_allocator, base_render_data, svo_debug_draw_setup_compute_pipeline, VkExtent3D{.width = 1, .height = 1, .depth = 1}, QueueFamilyVK::COMPUTE_1);
    };
    return task_desc;
}

RendTaskDescVK ISVORenderFeature::get_debug_draw_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_SVO_DEBUG_DRAW;
    const std::array<std::string_view, 1> color_targets{REND_RES_SWAP_CHAIN};
    alias_render_targets(task_desc, color_targets, GBUFFER_DEPTH_IMAGE_VIEW);
    RTRunGraphicsPipelineIndirectVK::Builder task_builder(render_allocator);
    task_builder.set_index_buffer(SVO_DEBUG_DRAW_INDEX_BUFFER)
        .set_indirect_buffer(SVO_DEBUG_DRAW_INDIRECT_BUFFER)
        .set_pipeline(svo_debug_draw_graphics_pipeline, 1, VertexInputDescriptionVK{})
        .set_color_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
        .set_depth_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    task_desc.factory = [this, task_builder](const RendGraphContextVK& p_context) {
        RTRunGraphicsPipelineIndirectVK::Builder builder = task_builder;
        builder.set_render_extent(p_context.swap_chain_extent);
        return make_unique<RTVoxelDebugView>(render_allocator,
            builder.create_unique(base_render_data),
            debug_view_mode,
            VoxelDebugViewMode::POST_SVO_BUILD,
            VoxelDebugViewMode::POST_SVO_BUILD);
    };
    return task_desc;
}

RendTaskDescVK ISVORenderFeature::get_pre_deferred_debug_draw_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_SVO_PRE_DEFERRED_DEBUG_DRAW;
    const std::array<std::string_view, 1> color_targets{REND_RES_SWAP_CHAIN};
    alias_render_targets(task_desc, color_targets, GBUFFER_DEPTH_IMAGE_VIEW);
    RTRunGraphicsPipelineIndirectVK::Builder task_builder(render_allocator);
    task_builder.set_index_buffer(SVO_DEBUG_DRAW_INDEX_BUFFER)
        .set_indirect_buffer(SVO_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER)
        .set_pipeline(svo_pre_deferred_debug_draw_graphics_pipeline, 1, VertexInputDescriptionVK{})
        .set_color_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
        .set_depth_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
    task_desc.factory = [this, task_builder](const RendGraphContextVK& p_context) {
        RTRunGraphicsPipelineIndirectVK::Builder builder = task_builder;
        builder.set_render_extent(p_context.swap_chain_extent);
        return make_unique<RTVoxelDebugView>(render_allocator,
            builder.create_unique(base_render_data),
            debug_view_mode,
            VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION,
            VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION);
    };
    return task_desc;
}

Ref<ROBufferVK> ISVORenderFeature::create_initialized_buffer(VkBufferUsageFlags p_usage, const void* p_data, size_t p_size) const {
    return ROBufferVK::Builder().set_usage(p_usage).initial_data(p_data, p_size).create_ref(base_render_data);
}

void ISVORenderFeature::register_render_graph_data(RendGraphDescVK& p_graph) const {
    p_graph.add_resource(SVO_DYNAMIC_NODES, *dynamic_nodes).add_resource(SVO_DYNAMIC_LEAVES, *dynamic_leaves);
    p_graph.add_resource(SVO_INFO, *svo_info)
        .add_resource(SVO_NODE_BUFFER, *svo_nodes)
        .add_resource(SVO_LEAF_BUFFER, *svo_leaves)
        .add_resource(SVO_DEFERRED_LIST_PING, *svo_deferred_list_ping)
        .add_resource(SVO_DEFERRED_LIST_PONG, *svo_deferred_list_pong)
        .add_resource(SVO_DEBUG_DRAW_INDEX_BUFFER, *svo_debug_draw_index_buffer)
        .add_resource(SVO_DEBUG_DRAW_INDIRECT_BUFFER, *svo_debug_draw_indirect_buffer)
        .add_resource(SVO_PRE_DEFERRED_DEBUG_INFO_BUFFER, *svo_pre_deferred_debug_info_buffer)
        .add_resource(SVO_PRE_DEFERRED_DEBUG_LIST_BUFFER, *svo_pre_deferred_debug_list_buffer)
        .add_resource(SVO_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER, *svo_pre_deferred_debug_draw_indirect_buffer);
    register_implementation_render_graph_data(p_graph);
}

void ISVORenderFeature::add_construction_render_tasks(RendGraphDescVK& p_graph, const DynamicMeshRenderFeature& p_dynamic_mesh) const {
    RendTaskDescVK begin(render_allocator);
    begin.task_id = RT_SVO_FRAME_BEGIN;
    begin.factory = [this](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTSVOFrameBeginVK>(render_allocator);
    };
    p_graph.add_task(std::move(begin));
    p_graph.add_task(make_static_once(get_reset_render_task_desc()))
        .add_task(make_static_once(get_construction_render_task_desc(RT_RUN_BUILD_SVO)))
        .add_task(make_static_once(get_construction_render_task_desc(RT_RUN_BUILD_SVO_DOUBLE_SIDED, true)));
    add_static_publication_render_tasks(p_graph);
    auto static_deferred = get_deferred_construction_render_task_desc(false);
    static_deferred.task_id = RT_STATIC_SVO_DEFERRED;
    p_graph.add_task(make_static_once(std::move(static_deferred), true));
    p_graph.add_task(get_dynamic_clear_task_desc());
    p_graph.add_task(get_construction_render_task_desc(RT_RUN_BUILD_DYNAMIC_SVO, false, &p_dynamic_mesh, 0))
        .add_task(get_construction_render_task_desc(RT_RUN_BUILD_DYNAMIC_SVO_SECOND, false, &p_dynamic_mesh, 1));
    add_dynamic_publication_render_tasks(p_graph);
    p_graph.add_task(get_debug_capture_render_task_desc(RT_CAPTURE_SVO_PRE_DEFERRED_DEBUG, VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION));
}

void ISVORenderFeature::connect_render_tasks(RendGraphDescVK& p_graph) const {
    // The frame-begin barrier orders this frame's SVO mutation after the previous frame's consumers.
    p_graph.get_task(RT_SVO_FRAME_BEGIN).connect(RT_RESET_SVO, SVO_INFO);
    connect_implementation_render_tasks(p_graph);
    connect_resource_chain(p_graph,
        SVO_LEAF_BUFFER,
        {
            RT_RUN_BUILD_SVO,
            RT_RUN_BUILD_SVO_DOUBLE_SIDED,
            RT_STATIC_SVO_DEFERRED,
            RT_RUN_BUILD_DYNAMIC_SVO,
            RT_RUN_BUILD_DYNAMIC_SVO_SECOND,
            RT_RUN_BUILD_SVO_DEFERRED,
            RT_RUN_SVO_DEBUG_DRAW,
            RT_RUN_SVO_PRE_DEFERRED_DEBUG_DRAW,
        });
    connect_resource_chain(p_graph,
        SVO_DEFERRED_LIST_PING,
        {
            RT_RESET_SVO,
            RT_RUN_BUILD_SVO,
            RT_RUN_BUILD_SVO_DOUBLE_SIDED,
            RT_STATIC_SVO_DEFERRED,
            RT_CLEAR_DYNAMIC_SVO,
            RT_RUN_BUILD_DYNAMIC_SVO,
            RT_RUN_BUILD_DYNAMIC_SVO_SECOND,
            RT_CAPTURE_SVO_PRE_DEFERRED_DEBUG,
            RT_RUN_BUILD_SVO_DEFERRED,
        });
    connect_resource_chain(p_graph,
        SVO_DEFERRED_LIST_PONG,
        {RT_RESET_SVO, RT_STATIC_SVO_DEFERRED, RT_CLEAR_DYNAMIC_SVO, RT_RUN_BUILD_SVO_DEFERRED});
    connect_resource_chain(
        p_graph, SVO_PRE_DEFERRED_DEBUG_INFO_BUFFER, {RT_CAPTURE_SVO_PRE_DEFERRED_DEBUG, RT_RUN_SVO_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(
        p_graph, SVO_PRE_DEFERRED_DEBUG_LIST_BUFFER, {RT_CAPTURE_SVO_PRE_DEFERRED_DEBUG, RT_RUN_SVO_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(p_graph,
        SVO_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER,
        {RT_CAPTURE_SVO_PRE_DEFERRED_DEBUG, RT_RUN_SVO_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(p_graph, SVO_DEBUG_DRAW_INDIRECT_BUFFER, {RT_SETUP_SVO_DEBUG_DRAW, RT_RUN_SVO_DEBUG_DRAW});
}

RendTaskDescVK ISVORenderFeature::make_static_once(RendTaskDescVK p_task, bool p_finish) const {
    if (rebuild_static_every_frame) {
        // The reset stage clears the counters and root nodes, so every frame rebuilds from an empty tree.
        return p_task;
    }
    p_task.factory = [this, task_factory = p_task.factory, p_finish](const RendGraphContextVK& p_context) {
        auto stage = make_unique<RTSVOStaticStageVK>(render_allocator, task_factory(p_context), static_build_completed, p_finish);
        return make_unique<RTTriggeredVK>(render_allocator, std::move(stage), true);
    };
    return p_task;
}

RendTaskDescVK ISVORenderFeature::get_dynamic_clear_task_desc() const {
    RendTaskDescVK task(render_allocator);
    task.task_id = RT_CLEAR_DYNAMIC_SVO;
    constexpr uint32_t GROUPS = static_cast<uint32_t>((SVO_MAX_NODE_COUNT + 127) / 128);
    task.factory = [this](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTRunComputePipelineVK>(
            render_allocator, base_render_data, dynamic_clear_pipeline, VkExtent3D{.width = GROUPS, .height = 1, .depth = 1}, QueueFamilyVK::COMPUTE_1);
    };
    return task;
}

} // namespace WhiteBirdEngine
