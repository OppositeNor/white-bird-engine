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
#include "octree_render_feature_pass_published.hh"
#include "../render_test.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipelines_sequential_vk.hh"
#include "function/render_vk/mesh_resources_vk.hh"
#include "i_octree_render_feature.hh"
#include <atomic>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

OctreeRenderFeaturePassPublished::OctreeRenderFeaturePassPublished(RendererVK* p_renderer,
    Ref<MeshResourcesVK> p_static_mesh_resources,
    const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
    bool p_rebuild_static_every_frame)
    : IOctreeRenderFeature(p_renderer, std::move(p_static_mesh_resources), p_debug_view_mode, "octree_2", "octree_2_dynamic", p_rebuild_static_every_frame) {
    update_states_pipeline = compile_compute_pipeline("octree_2_update_states_shader");
    dynamic_publication_pipeline = compile_compute_pipeline("octree_2_dynamic_update_states_shader");
    pending_children =
        ROBufferVK::Builder().set_properties(sizeof(uint32_t), OCTREE_MAX_NODE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).create_ref(base_render_data);
}

std::string_view OctreeRenderFeaturePassPublished::get_name() const {
    return "octree_2 (pass publication)";
}

uint32_t OctreeRenderFeaturePassPublished::get_reset_group_count() const {
    // The reset also clears the voxel claims.
    return static_cast<uint32_t>((OCTREE_CLAIM_WORD_COUNT + 63U) / 64U);
}

void OctreeRenderFeaturePassPublished::register_implementation_render_graph_data(RendGraphDescVK& p_graph) const {
    p_graph.add_resource(OCTREE_PENDING_CHILDREN, *pending_children);
}

void OctreeRenderFeaturePassPublished::add_static_publication_render_tasks(RendGraphDescVK& p_graph) const {
    p_graph.add_task(make_static_once(get_state_update_render_task_desc()));
}

void OctreeRenderFeaturePassPublished::add_dynamic_publication_render_tasks(RendGraphDescVK& p_graph) const {
    p_graph.add_task(get_dynamic_publication_task_desc());
}

void OctreeRenderFeaturePassPublished::connect_implementation_render_tasks(RendGraphDescVK& p_graph) const {
    connect_resource_chain(p_graph,
        OCTREE_INFO,
        {RT_RESET_OCTREE,
            RT_RUN_BUILD_OCTREE,
            RT_RUN_BUILD_OCTREE_DOUBLE_SIDED,
            RT_UPDATE_OCTREE_STATES,
            RT_STATIC_OCTREE_DEFERRED,
            RT_CLEAR_DYNAMIC_OCTREE,
            RT_RUN_BUILD_DYNAMIC_OCTREE,
            RT_PUBLISH_DYNAMIC_OCTREE,
            RT_CAPTURE_OCTREE_PRE_DEFERRED_DEBUG,
            RT_RUN_BUILD_OCTREE_DEFERRED,
            RT_SETUP_OCTREE_DEBUG_DRAW,
            RT_RUN_OCTREE_DEBUG_DRAW});
    connect_resource_chain(p_graph,
        OCTREE_NODE_BUFFER,
        {RT_RESET_OCTREE,
            RT_RUN_BUILD_OCTREE,
            RT_RUN_BUILD_OCTREE_DOUBLE_SIDED,
            RT_UPDATE_OCTREE_STATES,
            RT_STATIC_OCTREE_DEFERRED,
            RT_CLEAR_DYNAMIC_OCTREE,
            RT_RUN_BUILD_DYNAMIC_OCTREE,
            RT_PUBLISH_DYNAMIC_OCTREE,
            RT_RUN_BUILD_OCTREE_DEFERRED,
            RT_RUN_OCTREE_DEBUG_DRAW,
            RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(p_graph,
        OCTREE_PENDING_CHILDREN,
        {RT_RESET_OCTREE, RT_RUN_BUILD_OCTREE, RT_RUN_BUILD_OCTREE_DOUBLE_SIDED, RT_UPDATE_OCTREE_STATES, RT_STATIC_OCTREE_DEFERRED, RT_RUN_BUILD_DYNAMIC_OCTREE, RT_RUN_BUILD_OCTREE_DEFERRED});
    connect_resource_chain(p_graph,
        OCTREE_VOXEL_CLAIMS,
        {RT_RESET_OCTREE, RT_RUN_BUILD_OCTREE, RT_RUN_BUILD_OCTREE_DOUBLE_SIDED, RT_CLEAR_DYNAMIC_OCTREE, RT_RUN_BUILD_DYNAMIC_OCTREE, RT_PUBLISH_DYNAMIC_OCTREE, RT_RUN_BUILD_OCTREE_DEFERRED});
    connect_resource_chain(p_graph,
        OCTREE_DYNAMIC_NODES,
        {RT_CLEAR_DYNAMIC_OCTREE, RT_RUN_BUILD_DYNAMIC_OCTREE, RT_PUBLISH_DYNAMIC_OCTREE, RT_RUN_BUILD_OCTREE_DEFERRED, RT_RUN_OCTREE_DEBUG_DRAW, RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(p_graph,
        OCTREE_DYNAMIC_LEAVES,
        {RT_CLEAR_DYNAMIC_OCTREE, RT_RUN_BUILD_DYNAMIC_OCTREE, RT_PUBLISH_DYNAMIC_OCTREE, RT_RUN_BUILD_OCTREE_DEFERRED, RT_RUN_OCTREE_DEBUG_DRAW, RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW});
}

RendTaskDescVK OctreeRenderFeaturePassPublished::make_deferred_construction_render_task_desc(bool p_dynamic) const {
    RendTaskDescVK task_desc(render_allocator);
    const Ref<ROComputePipelineVK>& reset_ping_pipeline = octree_deferred_count_reset_pong_compute_pipeline;
    const Ref<ROComputePipelineVK>& reset_pong_pipeline = octree_deferred_count_reset_ping_compute_pipeline;
    const Ref<ROComputePipelineVK>& ping_pipeline =
        p_dynamic ? dynamic_ping_pipeline : octree_deferred_propagation_pong_write_compute_pipeline;
    const Ref<ROComputePipelineVK>& pong_pipeline =
        p_dynamic ? dynamic_pong_pipeline : octree_deferred_propagation_ping_write_compute_pipeline;
    const Ref<ROComputePipelineVK>& publication_pipeline = p_dynamic ? dynamic_publication_pipeline : update_states_pipeline;
    constexpr uint32_t THREADS_PER_GROUP = 64;
    constexpr VkExtent3D RESET_GROUP_COUNT{.width = 1, .height = 1, .depth = 1};
    constexpr VkExtent3D PROPAGATION_GROUP_COUNT{.width = OCTREE_DEFERRED_DISPATCH_GROUP_COUNT, .height = 1, .depth = 1};
    RTRunComputePipelinesSequentialVK::Dispatch dispatch(render_allocator);
    const VkExtent3D publication_group_count{
        .width = static_cast<uint32_t>(p_dynamic ? (OCTREE_MAX_NODE_COUNT + OCTREE_MAX_DYNAMIC_NODE_COUNT + 127) / 128
                                                 : (OCTREE_MAX_NODE_COUNT + THREADS_PER_GROUP - 1) / THREADS_PER_GROUP),
        .height = 1,
        .depth = 1};
    // Rasterization allocates depth one's children. Each round publishes one further level;
    // an extra retry consumes leaf occupancy and drains the remaining deferred entries.
    for (uint32_t iteration = 0; iteration < OCTREE_MAX_DEPTH; ++iteration) {
        bool write_pong = iteration % 2U == 0;
        dispatch.add_entry(write_pong ? reset_ping_pipeline : reset_pong_pipeline, RESET_GROUP_COUNT)
            .add_entry(write_pong ? ping_pipeline : pong_pipeline, PROPAGATION_GROUP_COUNT)
            .add_entry(publication_pipeline, publication_group_count);
    }
    task_desc.task = make_unique<RTRunComputePipelinesSequentialVK>(render_allocator, base_render_data, dispatch);
    return task_desc;
}

RendTaskDescVK OctreeRenderFeaturePassPublished::get_state_update_render_task_desc() const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.task_id = RT_UPDATE_OCTREE_STATES;
    constexpr VkExtent3D GROUP_COUNT{.width = static_cast<uint32_t>((OCTREE_MAX_NODE_COUNT + 63) / 64), .height = 1, .depth = 1};
    task_desc.task = make_unique<RTRunComputePipelineVK>(render_allocator, base_render_data, update_states_pipeline, GROUP_COUNT);
    return task_desc;
}

RendTaskDescVK OctreeRenderFeaturePassPublished::get_dynamic_publication_task_desc() const {
    RendTaskDescVK task(render_allocator);
    task.task_id = RT_PUBLISH_DYNAMIC_OCTREE;
    constexpr uint32_t GROUPS = static_cast<uint32_t>((OCTREE_MAX_NODE_COUNT + OCTREE_MAX_DYNAMIC_NODE_COUNT + 127) / 128);
    task.task = make_unique<RTRunComputePipelineVK>(
        render_allocator, base_render_data, dynamic_publication_pipeline, VkExtent3D{.width = GROUPS, .height = 1, .depth = 1});
    return task;
}

} // namespace WhiteBirdEngine
