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
#include "octree_render_feature_atomic.hh"
#include "../render_test.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/rt_ping_pong_vk.hh"
#include "function/render_vk/static_set_resources_vk.hh"
#include "i_octree_render_feature.hh"
#include <atomic>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

OctreeRenderFeatureAtomic::OctreeRenderFeatureAtomic(RendererVK* p_renderer,
    Ref<StaticSetResourcesVK> p_static_set_resources,
    const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
    bool p_rebuild_static_every_frame)
    : IOctreeRenderFeature(p_renderer, std::move(p_static_set_resources), p_debug_view_mode, "octree", "octree_dynamic", p_rebuild_static_every_frame) {
}

std::string_view OctreeRenderFeatureAtomic::get_name() const {
    return "octree (atomic publication)";
}

uint32_t OctreeRenderFeatureAtomic::get_reset_group_count() const {
    return 1;
}

void OctreeRenderFeatureAtomic::register_implementation_render_graph_data(RendGraphDescVK& /*p_graph*/) const {
}

void OctreeRenderFeatureAtomic::add_static_publication_render_tasks(RendGraphDescVK& /*p_graph*/) const {
    // CAS owners publish static blocks while allocating them.
}

void OctreeRenderFeatureAtomic::add_dynamic_publication_render_tasks(RendGraphDescVK& /*p_graph*/) const {
    // CAS owners publish dynamic blocks while allocating them.
}

void OctreeRenderFeatureAtomic::connect_implementation_render_tasks(RendGraphDescVK& p_graph) const {
    connect_resource_chain(p_graph,
        OCTREE_INFO,
        {RT_RESET_OCTREE,
            RT_RUN_BUILD_OCTREE,
            RT_RUN_BUILD_OCTREE_DOUBLE_SIDED,
            RT_STATIC_OCTREE_DEFERRED,
            RT_CLEAR_DYNAMIC_OCTREE,
            RT_RUN_BUILD_DYNAMIC_OCTREE,
            RT_CAPTURE_OCTREE_PRE_DEFERRED_DEBUG,
            RT_RUN_BUILD_OCTREE_DEFERRED,
            RT_SETUP_OCTREE_DEBUG_DRAW,
            RT_RUN_OCTREE_DEBUG_DRAW});
    connect_resource_chain(p_graph,
        OCTREE_NODE_BUFFER,
        {RT_RESET_OCTREE,
            RT_RUN_BUILD_OCTREE,
            RT_RUN_BUILD_OCTREE_DOUBLE_SIDED,
            RT_STATIC_OCTREE_DEFERRED,
            RT_CLEAR_DYNAMIC_OCTREE,
            RT_RUN_BUILD_DYNAMIC_OCTREE,
            RT_RUN_BUILD_OCTREE_DEFERRED,
            RT_RUN_OCTREE_DEBUG_DRAW,
            RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(p_graph,
        OCTREE_DYNAMIC_NODES,
        {RT_CLEAR_DYNAMIC_OCTREE, RT_RUN_BUILD_DYNAMIC_OCTREE, RT_RUN_BUILD_OCTREE_DEFERRED, RT_RUN_OCTREE_DEBUG_DRAW, RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW});
    connect_resource_chain(p_graph,
        OCTREE_DYNAMIC_LEAVES,
        {RT_CLEAR_DYNAMIC_OCTREE, RT_RUN_BUILD_DYNAMIC_OCTREE, RT_RUN_BUILD_OCTREE_DEFERRED, RT_RUN_OCTREE_DEBUG_DRAW, RT_RUN_OCTREE_PRE_DEFERRED_DEBUG_DRAW});
}

RendTaskDescVK OctreeRenderFeatureAtomic::make_deferred_construction_render_task_desc(bool p_dynamic) const {
    RendTaskDescVK task_desc(render_allocator);
    task_desc.create = [task_render_allocator = render_allocator,
                           task_base_render_data = base_render_data,
                           reset_ping_pipeline = octree_deferred_count_reset_pong_compute_pipeline,
                           reset_pong_pipeline = octree_deferred_count_reset_ping_compute_pipeline,
                           ping_pipeline = p_dynamic ? dynamic_ping_pipeline : octree_deferred_propagation_pong_write_compute_pipeline,
                           pong_pipeline = p_dynamic ? dynamic_pong_pipeline
                                                     : octree_deferred_propagation_ping_write_compute_pipeline]() -> Unique<IRenderTaskVK> {
        constexpr VkExtent3D RESET_GROUP_COUNT{.width = 1, .height = 1, .depth = 1};
        constexpr VkExtent3D PROPAGATION_GROUP_COUNT{.width = OCTREE_DEFERRED_DISPATCH_GROUP_COUNT, .height = 1, .depth = 1};
        Unique<RTPingPongVK> task = make_unique<RTPingPongVK>(task_render_allocator,
            task_base_render_data,
            ping_pipeline,
            pong_pipeline,
            false,
            // A single raster pass can leave contenders at any depth; allow one retry per tree level.
            static_cast<uint32_t>(OCTREE_MAX_DEPTH),
            PROPAGATION_GROUP_COUNT);
        task->set_iteration_prefixes(reset_ping_pipeline, reset_pong_pipeline, RESET_GROUP_COUNT);
        return task;
    };
    return task_desc;
}

} // namespace WhiteBirdEngine
