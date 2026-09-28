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
#include "oct_tree_render_feature.hh"
#include "../render_test.hh"
#include "core/logging/log.hh"
#include "core/memory/unique.hh"
#include "dynamic_mesh_render_feature.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/rt_ping_pong_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipelines_sequential_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/rt_triggered_vk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {
namespace {

/**
 * @brief Shares static-build completion across graph variants; neither copyable nor movable.
 */
class RTOctTreeStaticStageVK final : public IRenderTaskVK {
public:
    RTOctTreeStaticStageVK() = delete;
    RTOctTreeStaticStageVK(Unique<IRenderTaskVK> p_task, Ref<bool> p_completed, bool p_finish)
        : task(std::move(p_task)), completed(std::move(p_completed)), finish(p_finish) {
        WBE_DEBUG_ASSERT(task != MEM_NULL && completed != MEM_NULL);
    }
    ~RTOctTreeStaticStageVK() override = default;
    RTOctTreeStaticStageVK(const RTOctTreeStaticStageVK&) = delete;
    RTOctTreeStaticStageVK(RTOctTreeStaticStageVK&&) = delete;
    RTOctTreeStaticStageVK& operator=(const RTOctTreeStaticStageVK&) = delete;
    RTOctTreeStaticStageVK& operator=(RTOctTreeStaticStageVK&&) = delete;
    QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }
    Vector<ResAccessVK, RenderAllocator> read_resources() const override {
        return task->read_resources();
    }
    Vector<ResAccessVK, RenderAllocator> write_resources() const override {
        return task->write_resources();
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
class RTOctTreeFrameBeginVK final : public IRenderTaskVK {
public:
    RTOctTreeFrameBeginVK() = delete;
    explicit RTOctTreeFrameBeginVK(RenderAllocator* p_allocator) : allocator(p_allocator) {
    }
    ~RTOctTreeFrameBeginVK() override = default;
    RTOctTreeFrameBeginVK(const RTOctTreeFrameBeginVK&) = delete;
    RTOctTreeFrameBeginVK(RTOctTreeFrameBeginVK&&) = delete;
    RTOctTreeFrameBeginVK& operator=(const RTOctTreeFrameBeginVK&) = delete;
    RTOctTreeFrameBeginVK& operator=(RTOctTreeFrameBeginVK&&) = delete;
    QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }
    Vector<ResAccessVK, RenderAllocator> read_resources() const override {
        return Vector<ResAccessVK, RenderAllocator>(allocator);
    }
    Vector<ResAccessVK, RenderAllocator> write_resources() const override {
        return Vector<ResAccessVK, RenderAllocator>(allocator);
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

private:
    RenderAllocator* allocator;
};

constexpr std::array<uint32_t, 36> OCT_TREE_DEBUG_CUBE_INDICES{
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

    Vector<ResAccessVK, RenderAllocator> read_resources() const override {
        return task->read_resources();
    }

    Vector<ResAccessVK, RenderAllocator> write_resources() const override {
        return task->write_resources();
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

OctTreeRenderFeature::OctTreeRenderFeature(RendererVK* p_renderer,
    Ref<StaticSetResourcesVK> p_static_set_resources,
    const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
    Implementation p_implementation)
    : renderer_vk(p_renderer), base_render_data(nullptr), render_allocator(nullptr),
      static_set_resources(std::move(p_static_set_resources)), debug_view_mode(p_debug_view_mode), implementation(p_implementation) {
    if (renderer_vk == nullptr || static_set_resources == MEM_NULL || debug_view_mode == nullptr) {
        throw std::runtime_error("OctTreeRenderFeature: renderer, scene and debug state are required.");
    }
    if (implementation != Implementation::ATOMIC && implementation != Implementation::PASS_PUBLISHED) {
        throw std::runtime_error("OctTreeRenderFeature: unknown implementation.");
    }
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = base_render_data->allocator;
    static_build_completed = make_ref<bool>(render_allocator, false);
    compile_pipelines();
    create_resources();
}

void OctTreeRenderFeature::compile_pipelines() {
    auto* resource_manager = ResourceManager::get_singleton();
    const std::string_view shader_prefix = implementation == Implementation::PASS_PUBLISHED ? "oct_tree_2" : "oct_tree";
    auto oct_tree_build_pipeline_resource =
        resource_manager->get_resource<RUGraphicsPipeline>(std::format("{}_build_static_set_pipeline", shader_prefix));
    auto oct_tree_reset_shader_resource = resource_manager->get_resource<RUShader>(std::format("{}_reset_shader", shader_prefix));
    auto oct_tree_deferred_count_reset_ping_shader_resource =
        resource_manager->get_resource<RUShader>("oct_tree_deferred_count_reset_ping_shader");
    auto oct_tree_deferred_count_reset_pong_shader_resource =
        resource_manager->get_resource<RUShader>("oct_tree_deferred_count_reset_pong_shader");
    auto oct_tree_deferred_propagation_ping_write_shader_resource =
        resource_manager->get_resource<RUShader>(std::format("{}_build_deferred_propagation_ping_write_shader", shader_prefix));
    auto oct_tree_deferred_propagation_pong_write_shader_resource =
        resource_manager->get_resource<RUShader>(std::format("{}_build_deferred_propagation_pong_write_shader", shader_prefix));
    auto oct_tree_pre_deferred_debug_capture_shader_resource =
        resource_manager->get_resource<RUShader>("oct_tree_pre_deferred_debug_capture_shader");
    auto oct_tree_debug_draw_setup_shader_resource = resource_manager->get_resource<RUShader>("oct_tree_debug_draw_setup_shader");
    auto oct_tree_debug_draw_pipeline_resource = resource_manager->get_resource<RUGraphicsPipeline>("oct_tree_debug_draw_pipeline");
    auto oct_tree_pre_deferred_debug_draw_pipeline_resource =
        resource_manager->get_resource<RUGraphicsPipeline>("oct_tree_pre_deferred_debug_draw_pipeline");

    GraphicsPipelineCompilerVK oct_tree_compiler(GraphicsPipelineCompilerVKContext{.allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk});
    oct_tree_build_graphics_pipeline =
        oct_tree_compiler
            .compile(std::format("{}_build_static_set_pipeline", shader_prefix), oct_tree_build_pipeline_resource)
            .graphics_pipeline;
    ComputePipelineCompilerVK oct_tree_compute_compiler(ComputePipelineCompilerVKContext{
        .allocator = render_allocator, .render_data = base_render_data, .render_database = renderer_vk->get_render_database()});
    dynamic_build_pipeline = oct_tree_compiler
                                 .compile("oct_tree_dynamic_build_static_set_pipeline",
                                     resource_manager->get_resource<RUGraphicsPipeline>("oct_tree_dynamic_build_static_set_pipeline"))
                                 .graphics_pipeline;
    dynamic_clear_pipeline =
        oct_tree_compute_compiler
            .compile("oct_tree_clear_dynamic_shader", resource_manager->get_resource<RUShader>("oct_tree_clear_dynamic_shader"))
            .compute_pipeline;
    dynamic_publication_pipeline = oct_tree_compute_compiler
                                       .compile("oct_tree_dynamic_update_states_shader",
                                           resource_manager->get_resource<RUShader>("oct_tree_dynamic_update_states_shader"))
                                       .compute_pipeline;
    dynamic_ping_pipeline =
        oct_tree_compute_compiler
            .compile("oct_tree_dynamic_build_deferred_propagation_pong_write_shader",
                resource_manager->get_resource<RUShader>("oct_tree_dynamic_build_deferred_propagation_pong_write_shader"))
            .compute_pipeline;
    dynamic_pong_pipeline =
        oct_tree_compute_compiler
            .compile("oct_tree_dynamic_build_deferred_propagation_ping_write_shader",
                resource_manager->get_resource<RUShader>("oct_tree_dynamic_build_deferred_propagation_ping_write_shader"))
            .compute_pipeline;
    oct_tree_reset_compute_pipeline =
        oct_tree_compute_compiler.compile(std::format("{}_reset_shader", shader_prefix), oct_tree_reset_shader_resource).compute_pipeline;
    oct_tree_deferred_count_reset_ping_compute_pipeline =
        oct_tree_compute_compiler
            .compile("oct_tree_deferred_count_reset_ping_shader", oct_tree_deferred_count_reset_ping_shader_resource)
            .compute_pipeline;
    oct_tree_deferred_count_reset_pong_compute_pipeline =
        oct_tree_compute_compiler
            .compile("oct_tree_deferred_count_reset_pong_shader", oct_tree_deferred_count_reset_pong_shader_resource)
            .compute_pipeline;
    oct_tree_deferred_propagation_ping_write_compute_pipeline =
        oct_tree_compute_compiler
            .compile(std::format("{}_build_deferred_propagation_ping_write_shader", shader_prefix),
                oct_tree_deferred_propagation_ping_write_shader_resource)
            .compute_pipeline;
    oct_tree_deferred_propagation_pong_write_compute_pipeline =
        oct_tree_compute_compiler
            .compile(std::format("{}_build_deferred_propagation_pong_write_shader", shader_prefix),
                oct_tree_deferred_propagation_pong_write_shader_resource)
            .compute_pipeline;
    oct_tree_pre_deferred_debug_capture_compute_pipeline =
        oct_tree_compute_compiler
            .compile("oct_tree_pre_deferred_debug_capture_shader", oct_tree_pre_deferred_debug_capture_shader_resource)
            .compute_pipeline;
    oct_tree_debug_draw_setup_compute_pipeline =
        oct_tree_compute_compiler.compile("oct_tree_debug_draw_setup_shader", oct_tree_debug_draw_setup_shader_resource).compute_pipeline;
    GraphicsPipelineCompilerVKContext oct_tree_debug_draw_context{
        .allocator = render_allocator,
        .render_data = base_render_data,
        .render_database = renderer_vk->get_render_database(),
        .renderer = renderer_vk,
        .color_attachment_formats = {base_render_data->swap_chain_format},
        .depth_attachment_format = GBUFFER_DEPTH_FORMAT,
        .polygon_mode = VK_POLYGON_MODE_LINE,
    };
    oct_tree_debug_draw_graphics_pipeline = GraphicsPipelineCompilerVK(oct_tree_debug_draw_context)
                                                .compile("oct_tree_debug_draw_pipeline", oct_tree_debug_draw_pipeline_resource)
                                                .graphics_pipeline;
    oct_tree_pre_deferred_debug_draw_graphics_pipeline =
        GraphicsPipelineCompilerVK(oct_tree_debug_draw_context)
            .compile("oct_tree_pre_deferred_debug_draw_pipeline", oct_tree_pre_deferred_debug_draw_pipeline_resource)
            .graphics_pipeline;
    if (implementation == Implementation::PASS_PUBLISHED) {
        update_states_pipeline = oct_tree_compute_compiler
                                     .compile("oct_tree_2_update_states_shader",
                                         resource_manager->get_resource<RUShader>("oct_tree_2_update_states_shader"))
                                     .compute_pipeline;
    }
}

void OctTreeRenderFeature::create_resources() {
    oct_tree_info = ROBufferVK::Builder().set_properties(sizeof(OctTreeInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    oct_tree_nodes = ROBufferVK::Builder()
                         .set_properties(sizeof(OctTreeNode), OCT_TREE_MAX_NODE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                         .create_ref(base_render_data);
    dynamic_nodes = ROBufferVK::Builder()
                        .set_properties(sizeof(OctTreeNode), OCT_TREE_MAX_DYNAMIC_NODE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                        .create_ref(base_render_data);
    dynamic_leaves = ROBufferVK::Builder()
                         .set_properties(sizeof(OctTreeDynamicLeafBlock), OCT_TREE_MAX_DYNAMIC_LEAVES_COUNT / 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                         .create_ref(base_render_data);
    oct_tree_leaves = ROBufferVK::Builder()
                          .set_properties(sizeof(OctTreeLeaf), OCT_TREE_MAX_LEAVES_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                          .create_ref(base_render_data);
    oct_tree_deferred_list_ping =
        ROBufferVK::Builder()
            .set_properties(sizeof(OctTreeDeferredListEntry), OCT_TREE_MAX_DEFERRED_TASK_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            .create_ref(base_render_data);
    oct_tree_deferred_list_pong =
        ROBufferVK::Builder()
            .set_properties(sizeof(OctTreeDeferredListEntry), OCT_TREE_MAX_DEFERRED_TASK_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            .create_ref(base_render_data);
    oct_tree_debug_draw_index_buffer = create_initialized_buffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, OCT_TREE_DEBUG_CUBE_INDICES.data(), sizeof(OCT_TREE_DEBUG_CUBE_INDICES));
    const VkDrawIndexedIndirectCommand initial_debug_draw_command{
        .indexCount = OCT_TREE_DEBUG_CUBE_INDICES.size(),
        .instanceCount = 0,
        .firstIndex = 0,
        .vertexOffset = 0,
        .firstInstance = 0,
    };
    oct_tree_debug_draw_indirect_buffer = create_initialized_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        &initial_debug_draw_command,
        sizeof(initial_debug_draw_command));
    oct_tree_pre_deferred_debug_info_buffer =
        ROBufferVK::Builder().set_properties(sizeof(OctTreeInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
    oct_tree_pre_deferred_debug_list_buffer =
        ROBufferVK::Builder()
            .set_properties(sizeof(OctTreeDeferredListEntry), OCT_TREE_MAX_DEFERRED_TASK_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            .create_ref(base_render_data);
    oct_tree_pre_deferred_debug_draw_indirect_buffer =
        create_initialized_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            &initial_debug_draw_command,
            sizeof(initial_debug_draw_command));
    {
        voxel_claims = ROBufferVK::Builder()
                           .set_properties(sizeof(uint32_t), OCT_TREE_CLAIM_WORD_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                           .create_ref(base_render_data);
        pending_children = ROBufferVK::Builder()
                               .set_properties(sizeof(uint32_t), OCT_TREE_MAX_NODE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                               .create_ref(base_render_data);
    }
    initialize();
}

void OctTreeRenderFeature::initialize() {
    const OctTreeInfo initial_oct_tree_info{
        .origin = OCT_TREE_ORIGIN,
        .width = OCT_TREE_WIDTH,
        .max_depth = OCT_TREE_MAX_DEPTH,
        .max_node_count = OCT_TREE_MAX_NODE_COUNT,
        .max_leaves_count = OCT_TREE_MAX_LEAVES_COUNT,
        .node_count = 8,
        .leaves_count = 0,
        .data_count = 8,
        .deferred_task_count_ping = 0,
        .deferred_task_count_pong = 0,
        .max_deferred_task_count = OCT_TREE_MAX_DEFERRED_TASK_COUNT,
        .max_dynamic_node_count = OCT_TREE_MAX_DYNAMIC_NODE_COUNT,
        .max_dynamic_leaves_count = OCT_TREE_MAX_DYNAMIC_LEAVES_COUNT,
        .dynamic_node_count = 0,
        .dynamic_leaves_count = 0,
    };
    std::array<OctTreeNode, 8> first_sectors{};
    for (uint32_t i = 0; i < first_sectors.size(); ++i) {
        first_sectors[i] = OctTreeNode{
            .children_data = static_cast<uint32_t>(OctTreeNode::ChildrenState::EMPTY) << 29U,
            .data_index = i,
            .parent_index = -1,
        };
    }
    oct_tree_info->update_buffer(&initial_oct_tree_info, sizeof(initial_oct_tree_info));
    oct_tree_nodes->update_buffer(first_sectors.data(), sizeof(first_sectors));
}

RendTaskDescVK OctTreeRenderFeature::get_reset_render_task_desc(std::string_view p_previous_task_id) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RESET_OCT_TREE;
    task_desc.after(p_previous_task_id).write(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER).write(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER);
    if (implementation == Implementation::PASS_PUBLISHED) {
        task_desc.write(OCT_TREE_PENDING_CHILDREN, RendResUsageVK::STORAGE_BUFFER).write(OCT_TREE_VOXEL_CLAIMS, RendResUsageVK::STORAGE_BUFFER);
    }
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = oct_tree_reset_compute_pipeline,
                           group_count = implementation == Implementation::PASS_PUBLISHED
                                             ? static_cast<uint32_t>((OCT_TREE_CLAIM_WORD_COUNT + 63U) / 64U)
                                             : 1U]() -> Unique<IRenderTaskVK> {
        return make_unique<RTRunComputePipelineVK>(
            task_render_allocator, task_base_render_data, task_pipeline, VkExtent3D{.width = group_count, .height = 1, .depth = 1});
    };
    return task_desc;
}

RendTaskDescVK OctTreeRenderFeature::get_construction_render_task_desc(
    std::string_view p_task_id, std::string_view p_previous_task_id, bool p_double_sided, const DynamicMeshRenderFeature* p_dynamic_mesh) const {
    constexpr std::array<VkClearValue, 0> CLEAR_VALUES{};
    constexpr std::array<std::string_view, 0> COLOR_ATTACHMENT_IDS{};
    RendTaskDescVK task_desc =
        p_dynamic_mesh != nullptr
            ? p_dynamic_mesh->make_task(p_task_id, dynamic_build_pipeline, VkExtent2D{.width = OCT_TREE_EXTENT, .height = OCT_TREE_EXTENT})
            : make_static_set_task_desc(render_allocator,
                  base_render_data,
                  p_task_id,
                  static_set_resources,
                  oct_tree_build_graphics_pipeline,
                  false,
                  CLEAR_VALUES,
                  VkExtent2D{.width = OCT_TREE_EXTENT, .height = OCT_TREE_EXTENT},
                  COLOR_ATTACHMENT_IDS,
                  std::nullopt,
                  VK_ATTACHMENT_LOAD_OP_CLEAR,
                  VK_ATTACHMENT_LOAD_OP_CLEAR,
                  p_double_sided);
    task_desc.after(p_previous_task_id)
        .read(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_LEAF_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_DEFERRED_LIST_PING, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_LEAF_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_DEFERRED_LIST_PING, RendResUsageVK::STORAGE_BUFFER);
    if (implementation == Implementation::PASS_PUBLISHED || p_dynamic_mesh != nullptr) {
        task_desc.read(OCT_TREE_VOXEL_CLAIMS, RendResUsageVK::STORAGE_BUFFER)
            .write(OCT_TREE_VOXEL_CLAIMS, RendResUsageVK::STORAGE_BUFFER)
            .read(OCT_TREE_PENDING_CHILDREN, RendResUsageVK::STORAGE_BUFFER)
            .write(OCT_TREE_PENDING_CHILDREN, RendResUsageVK::STORAGE_BUFFER);
    }
    if (p_dynamic_mesh != nullptr) {
        task_desc.read(OCT_TREE_DYNAMIC_NODES, RendResUsageVK::STORAGE_BUFFER)
            .write(OCT_TREE_DYNAMIC_NODES, RendResUsageVK::STORAGE_BUFFER)
            .read(OCT_TREE_DYNAMIC_LEAVES, RendResUsageVK::STORAGE_BUFFER)
            .write(OCT_TREE_DYNAMIC_LEAVES, RendResUsageVK::STORAGE_BUFFER);
    }
    return task_desc;
}

RendTaskDescVK OctTreeRenderFeature::get_deferred_construction_render_task_desc(bool p_dynamic) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_BUILD_OCT_TREE_DEFERRED;
    task_desc.after(RT_CAPTURE_OCT_TREE_PRE_DEFERRED_DEBUG)
        .read(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_LEAF_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_DEFERRED_LIST_PING, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_DEFERRED_LIST_PONG, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_LEAF_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_DEFERRED_LIST_PING, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_DEFERRED_LIST_PONG, RendResUsageVK::STORAGE_BUFFER);
    if (implementation == Implementation::PASS_PUBLISHED) {
        task_desc.read(OCT_TREE_PENDING_CHILDREN, RendResUsageVK::STORAGE_BUFFER).write(OCT_TREE_PENDING_CHILDREN, RendResUsageVK::STORAGE_BUFFER);
    }
    if (p_dynamic) {
        task_desc.read(OCT_TREE_DYNAMIC_NODES, RendResUsageVK::STORAGE_BUFFER)
            .write(OCT_TREE_DYNAMIC_NODES, RendResUsageVK::STORAGE_BUFFER)
            .read(OCT_TREE_DYNAMIC_LEAVES, RendResUsageVK::STORAGE_BUFFER)
            .write(OCT_TREE_DYNAMIC_LEAVES, RendResUsageVK::STORAGE_BUFFER);
    }
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           reset_ping_pipeline = oct_tree_deferred_count_reset_pong_compute_pipeline,
                           reset_pong_pipeline = oct_tree_deferred_count_reset_ping_compute_pipeline,
                           ping_pipeline = p_dynamic ? dynamic_ping_pipeline : oct_tree_deferred_propagation_pong_write_compute_pipeline,
                           pong_pipeline = p_dynamic ? dynamic_pong_pipeline : oct_tree_deferred_propagation_ping_write_compute_pipeline,
                           publication_pipeline = p_dynamic ? dynamic_publication_pipeline : update_states_pipeline,
                           dynamic = p_dynamic]() -> Unique<IRenderTaskVK> {
        constexpr uint32_t THREADS_PER_GROUP = 64;
        constexpr uint32_t DISPATCH_GROUP_COUNT = static_cast<uint32_t>(
            (OCT_TREE_MAX_DEFERRED_TASK_COUNT + OCT_TREE_DEFERRED_THREADS_PER_GROUP - 1) / OCT_TREE_DEFERRED_THREADS_PER_GROUP);
        constexpr VkExtent3D RESET_GROUP_COUNT{.width = 1, .height = 1, .depth = 1};
        constexpr VkExtent3D PROPAGATION_GROUP_COUNT{.width = DISPATCH_GROUP_COUNT, .height = 1, .depth = 1};
        if (publication_pipeline != MEM_NULL) {
            RTRunComputePipelinesSequentialVK::Dispatch dispatch(task_render_allocator);
            const VkExtent3D PUBLICATION_GROUP_COUNT{
                .width = static_cast<uint32_t>(dynamic ? (OCT_TREE_MAX_NODE_COUNT + OCT_TREE_MAX_DYNAMIC_NODE_COUNT + 127) / 128
                                                       : (OCT_TREE_MAX_NODE_COUNT + THREADS_PER_GROUP - 1) / THREADS_PER_GROUP),
                .height = 1,
                .depth = 1};
            // Rasterization allocates depth one's children. Each round publishes one further level;
            // an extra retry consumes leaf occupancy and drains the remaining deferred entries.
            for (uint32_t iteration = 0; iteration < OCT_TREE_MAX_DEPTH; ++iteration) {
                bool write_pong = iteration % 2U == 0;
                dispatch.add_entry(write_pong ? reset_ping_pipeline : reset_pong_pipeline, RESET_GROUP_COUNT)
                    .add_entry(write_pong ? ping_pipeline : pong_pipeline, PROPAGATION_GROUP_COUNT)
                    .add_entry(publication_pipeline, PUBLICATION_GROUP_COUNT);
            }
            return make_unique<RTRunComputePipelinesSequentialVK>(task_render_allocator, task_base_render_data, dispatch);
        }
        Unique<RTPingPongVK> task = make_unique<RTPingPongVK>(task_render_allocator,
            task_base_render_data,
            ping_pipeline,
            pong_pipeline,
            false,
            // A single raster pass can leave contenders at any depth; allow one retry per tree level.
            static_cast<uint32_t>(OCT_TREE_MAX_DEPTH),
            PROPAGATION_GROUP_COUNT);
        task->set_iteration_prefixes(reset_ping_pipeline, reset_pong_pipeline, RESET_GROUP_COUNT);
        return task;
    };
    return task_desc;
}

RendTaskDescVK OctTreeRenderFeature::get_debug_capture_render_task_desc(
    std::string_view p_task_id, std::string_view p_previous_task_id, VoxelDebugViewMode p_mode) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = p_task_id;
    task_desc.after(p_previous_task_id)
        .read(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_DEFERRED_LIST_PING, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_PRE_DEFERRED_DEBUG_INFO_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_PRE_DEFERRED_DEBUG_LIST_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER, RendResUsageVK::STORAGE_BUFFER);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = oct_tree_pre_deferred_debug_capture_compute_pipeline,
                           task_debug_view_mode = debug_view_mode,
                           task_required_mode = p_mode]() -> Unique<IRenderTaskVK> {
        constexpr uint32_t DISPATCH_GROUP_COUNT = static_cast<uint32_t>(
            (OCT_TREE_MAX_DEFERRED_TASK_COUNT + OCT_TREE_DEFERRED_THREADS_PER_GROUP - 1) / OCT_TREE_DEFERRED_THREADS_PER_GROUP);
        Unique<IRenderTaskVK> task = make_unique<RTRunComputePipelineVK>(task_render_allocator,
            task_base_render_data,
            task_pipeline,
            VkExtent3D{.width = DISPATCH_GROUP_COUNT, .height = 1, .depth = 1});
        return make_unique<RTVoxelDebugView>(
            task_render_allocator, std::move(task), task_debug_view_mode, task_required_mode, task_required_mode);
    };
    return task_desc;
}

RendTaskDescVK OctTreeRenderFeature::get_debug_draw_setup_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_SETUP_OCT_TREE_DEBUG_DRAW;
    task_desc.after(RT_RUN_BUILD_OCT_TREE_DEFERRED)
        .read(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_DEBUG_DRAW_INDIRECT_BUFFER, RendResUsageVK::STORAGE_BUFFER);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = oct_tree_debug_draw_setup_compute_pipeline]() -> Unique<IRenderTaskVK> {
        return make_unique<RTRunComputePipelineVK>(
            task_render_allocator, task_base_render_data, task_pipeline, VkExtent3D{.width = 1, .height = 1, .depth = 1});
    };
    return task_desc;
}

RendTaskDescVK OctTreeRenderFeature::get_debug_draw_render_task_desc(std::string_view p_previous_task_id) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_OCT_TREE_DEBUG_DRAW;
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = oct_tree_debug_draw_graphics_pipeline,
                           task_debug_view_mode = debug_view_mode]() -> Unique<IRenderTaskVK> {
        RTRunGraphicsPipelineIndirectVK::Builder task_builder(task_render_allocator);
        task_builder.set_render_extent(task_base_render_data->swap_chain_extent)
            .set_index_buffer(OCT_TREE_DEBUG_DRAW_INDEX_BUFFER)
            .set_indirect_buffer(OCT_TREE_DEBUG_DRAW_INDIRECT_BUFFER)
            .set_pipeline(task_pipeline, 1, VertexInputDescriptionVK{})
            .add_color_attachment(REND_RES_SWAP_CHAIN)
            .set_depth_attachment(GBUFFER_DEPTH_IMAGE_VIEW)
            .set_color_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .set_depth_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
        return make_unique<RTVoxelDebugView>(task_render_allocator,
            task_builder.create_unique(task_base_render_data),
            task_debug_view_mode,
            VoxelDebugViewMode::POST_OCT_TREE_BUILD,
            VoxelDebugViewMode::POST_OCT_TREE_BUILD);
    };
    task_desc.after(RT_SETUP_OCT_TREE_DEBUG_DRAW)
        .after(p_previous_task_id)
        .read(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_LEAF_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_DYNAMIC_NODES, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_DYNAMIC_LEAVES, RendResUsageVK::STORAGE_BUFFER)
        .read(GBUFFER_CAMERA_INFO_BUFFER, RendResUsageVK::UNIFORM_BUFFER);
    return task_desc;
}

RendTaskDescVK OctTreeRenderFeature::get_pre_deferred_debug_draw_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_RUN_OCT_TREE_PRE_DEFERRED_DEBUG_DRAW;
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = oct_tree_pre_deferred_debug_draw_graphics_pipeline,
                           task_debug_view_mode = debug_view_mode]() -> Unique<IRenderTaskVK> {
        RTRunGraphicsPipelineIndirectVK::Builder task_builder(task_render_allocator);
        task_builder.set_render_extent(task_base_render_data->swap_chain_extent)
            .set_index_buffer(OCT_TREE_DEBUG_DRAW_INDEX_BUFFER)
            .set_indirect_buffer(OCT_TREE_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER)
            .set_pipeline(task_pipeline, 1, VertexInputDescriptionVK{})
            .add_color_attachment(REND_RES_SWAP_CHAIN)
            .set_depth_attachment(GBUFFER_DEPTH_IMAGE_VIEW)
            .set_color_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .set_depth_attachment_load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
        return make_unique<RTVoxelDebugView>(task_render_allocator,
            task_builder.create_unique(task_base_render_data),
            task_debug_view_mode,
            VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION,
            VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION);
    };
    task_desc.after(RT_RUN_OCT_TREE_DEBUG_DRAW)
        .read(OCT_TREE_PRE_DEFERRED_DEBUG_INFO_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_LEAF_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_DYNAMIC_NODES, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_DYNAMIC_LEAVES, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_PRE_DEFERRED_DEBUG_LIST_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(GBUFFER_CAMERA_INFO_BUFFER, RendResUsageVK::UNIFORM_BUFFER);
    return task_desc;
}

Ref<ROBufferVK> OctTreeRenderFeature::create_initialized_buffer(VkBufferUsageFlags p_usage, const void* p_data, size_t p_size) const {
    return ROBufferVK::Builder().set_usage(p_usage).initial_data(p_data, p_size).create_ref(base_render_data);
}

void OctTreeRenderFeature::register_render_graph_data(RendGraphDescVK& p_graph) const {
    p_graph.add_resource(OCT_TREE_DYNAMIC_NODES, dynamic_nodes).add_resource(OCT_TREE_DYNAMIC_LEAVES, dynamic_leaves);
    p_graph.add_resource(OCT_TREE_INFO, oct_tree_info)
        .add_resource(OCT_TREE_NODE_BUFFER, oct_tree_nodes)
        .add_resource(OCT_TREE_LEAF_BUFFER, oct_tree_leaves)
        .add_resource(OCT_TREE_DEFERRED_LIST_PING, oct_tree_deferred_list_ping)
        .add_resource(OCT_TREE_DEFERRED_LIST_PONG, oct_tree_deferred_list_pong)
        .add_resource(OCT_TREE_DEBUG_DRAW_INDEX_BUFFER, oct_tree_debug_draw_index_buffer)
        .add_resource(OCT_TREE_DEBUG_DRAW_INDIRECT_BUFFER, oct_tree_debug_draw_indirect_buffer)
        .add_resource(OCT_TREE_PRE_DEFERRED_DEBUG_INFO_BUFFER, oct_tree_pre_deferred_debug_info_buffer)
        .add_resource(OCT_TREE_PRE_DEFERRED_DEBUG_LIST_BUFFER, oct_tree_pre_deferred_debug_list_buffer)
        .add_resource(OCT_TREE_PRE_DEFERRED_DEBUG_DRAW_INDIRECT_BUFFER, oct_tree_pre_deferred_debug_draw_indirect_buffer);
    p_graph.add_resource(OCT_TREE_PENDING_CHILDREN, pending_children).add_resource(OCT_TREE_VOXEL_CLAIMS, voxel_claims);
}

void OctTreeRenderFeature::add_construction_render_tasks(
    RendGraphDescVK& p_graph, std::string_view p_previous_task_id, const DynamicMeshRenderFeature& p_dynamic_mesh) const {
    RendTaskDescVK begin(render_allocator);
    begin.task_id = "oct_tree_frame_begin";
    begin.after(p_previous_task_id);
    begin.create = [allocator = render_allocator]() -> Unique<IRenderTaskVK> {
        return make_unique<RTOctTreeFrameBeginVK>(allocator, allocator);
    };
    p_graph.add_task(std::move(begin));
    p_graph.add_task(make_static_once(get_reset_render_task_desc("oct_tree_frame_begin")))
        .add_task(make_static_once(get_construction_render_task_desc(RT_RUN_BUILD_OCT_TREE, RT_RESET_OCT_TREE)))
        .add_task(make_static_once(get_construction_render_task_desc(RT_RUN_BUILD_OCT_TREE_DOUBLE_SIDED, RT_RUN_BUILD_OCT_TREE, true)));
    std::string_view previous = RT_RUN_BUILD_OCT_TREE_DOUBLE_SIDED;
    if (implementation == Implementation::PASS_PUBLISHED) {
        p_graph.add_task(make_static_once(get_state_update_render_task_desc(previous)));
        previous = RT_UPDATE_OCT_TREE_STATES;
    }
    auto static_deferred = get_deferred_construction_render_task_desc(false);
    static_deferred.task_id = "static_oct_tree_deferred";
    static_deferred.prev.clear();
    static_deferred.after(previous);
    p_graph.add_task(make_static_once(std::move(static_deferred), true));
    p_graph.add_task(get_dynamic_clear_task_desc("static_oct_tree_deferred"));
    p_graph.add_task(get_construction_render_task_desc("dynamic_oct_tree", "clear_dynamic_oct_tree", false, &p_dynamic_mesh));
    p_graph.add_task(get_dynamic_publication_task_desc("dynamic_oct_tree"));
    p_graph.add_task(get_debug_capture_render_task_desc(
        RT_CAPTURE_OCT_TREE_PRE_DEFERRED_DEBUG, "publish_dynamic_oct_tree", VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION));
}

RendTaskDescVK OctTreeRenderFeature::get_state_update_render_task_desc(std::string_view p_previous_task_id) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_UPDATE_OCT_TREE_STATES;
    task_desc.after(p_previous_task_id)
        .read(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_PENDING_CHILDREN, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_PENDING_CHILDREN, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           task_pipeline = update_states_pipeline]() -> Unique<IRenderTaskVK> {
        constexpr VkExtent3D GROUP_COUNT{.width = static_cast<uint32_t>((OCT_TREE_MAX_NODE_COUNT + 63) / 64), .height = 1, .depth = 1};
        return make_unique<RTRunComputePipelineVK>(task_render_allocator, task_base_render_data, task_pipeline, GROUP_COUNT);
    };
    return task_desc;
}

RendTaskDescVK OctTreeRenderFeature::make_static_once(RendTaskDescVK p_task, bool p_finish) const {
    auto create = std::move(p_task.create);
    p_task.create = [create = std::move(create),
                        allocator = render_allocator,
                        completed = static_build_completed,
                        finish = p_finish]() -> Unique<IRenderTaskVK> {
        auto stage = make_unique<RTOctTreeStaticStageVK>(allocator, create(), completed, finish);
        return make_unique<RTTriggeredVK>(allocator, std::move(stage), true);
    };
    return p_task;
}

RendTaskDescVK OctTreeRenderFeature::get_dynamic_clear_task_desc(std::string_view p_previous_task_id) const {
    RendTaskDescVK task(render_allocator);
    task.task_id = "clear_dynamic_oct_tree";
    task.after(p_previous_task_id)
        .read(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_DYNAMIC_LEAVES, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_VOXEL_CLAIMS, RendResUsageVK::STORAGE_BUFFER);
    task.create = [allocator = render_allocator, data = base_render_data, pipeline = dynamic_clear_pipeline]() -> Unique<IRenderTaskVK> {
        constexpr uint32_t groups = static_cast<uint32_t>((std::max(OCT_TREE_MAX_NODE_COUNT, OCT_TREE_CLAIM_WORD_COUNT) + 127) / 128);
        return make_unique<RTRunComputePipelineVK>(allocator, data, pipeline, VkExtent3D{groups, 1, 1});
    };
    return task;
}

RendTaskDescVK OctTreeRenderFeature::get_dynamic_publication_task_desc(std::string_view p_previous_task_id) const {
    RendTaskDescVK task(render_allocator);
    task.task_id = "publish_dynamic_oct_tree";
    task.after(p_previous_task_id)
        .read(OCT_TREE_INFO, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_NODE_BUFFER, RendResUsageVK::STORAGE_BUFFER)
        .read(OCT_TREE_DYNAMIC_NODES, RendResUsageVK::STORAGE_BUFFER)
        .write(OCT_TREE_DYNAMIC_NODES, RendResUsageVK::STORAGE_BUFFER);
    task.create = [allocator = render_allocator, data = base_render_data, pipeline = dynamic_publication_pipeline]() -> Unique<IRenderTaskVK> {
        constexpr uint32_t groups = static_cast<uint32_t>((OCT_TREE_MAX_NODE_COUNT + OCT_TREE_MAX_DYNAMIC_NODE_COUNT + 127) / 128);
        return make_unique<RTRunComputePipelineVK>(allocator, data, pipeline, VkExtent3D{groups, 1, 1});
    };
    return task;
}

} // namespace WhiteBirdEngine
