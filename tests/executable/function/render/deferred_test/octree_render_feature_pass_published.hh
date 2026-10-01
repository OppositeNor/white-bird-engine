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
#ifndef WBE_FILE_OCTREE_RENDER_FEATURE_PASS_PUBLISHED_HH
#define WBE_FILE_OCTREE_RENDER_FEATURE_PASS_PUBLISHED_HH

#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/mesh_resources_vk.hh"
#include "i_octree_render_feature.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include <atomic>
#include <cstdint>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class OctreeRenderFeaturePassPublished
 * @brief Octree construction whose child blocks become visible only at publication passes between construction rounds.
 * @details Traversal reads only published blocks, so construction passes need no acquire ordering; each deferred round is
 * followed by a publication pass. Neither copyable nor movable; confined to the rendering thread.
 */
class OctreeRenderFeaturePassPublished final : public IOctreeRenderFeature {
public:
    OctreeRenderFeaturePassPublished() = delete;
    /**
     * @brief Compiles the pass-published octree shaders and creates the shared and pending-children buffers on the rendering thread.
     * @details The renderer and debug-mode state must outlive this object and its registered graphs. Invalid dependencies throw.
     * @param p_rebuild_static_every_frame When true, the full static-scene construction runs every frame instead of once.
     */
    OctreeRenderFeaturePassPublished(RendererVK* p_renderer,
        Ref<MeshResourcesVK> p_static_mesh_resources,
        const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
        bool p_rebuild_static_every_frame = false);
    ~OctreeRenderFeaturePassPublished() override = default;
    OctreeRenderFeaturePassPublished(const OctreeRenderFeaturePassPublished&) = delete;
    OctreeRenderFeaturePassPublished(OctreeRenderFeaturePassPublished&&) = delete;
    OctreeRenderFeaturePassPublished& operator=(const OctreeRenderFeaturePassPublished&) = delete;
    OctreeRenderFeaturePassPublished& operator=(OctreeRenderFeaturePassPublished&&) = delete;

    std::string_view get_name() const override;

private:
    uint32_t get_reset_group_count() const override;
    void register_implementation_render_graph_data(RendGraphDescVK& p_graph) const override;
    void add_static_publication_render_tasks(RendGraphDescVK& p_graph) const override;
    void add_dynamic_publication_render_tasks(RendGraphDescVK& p_graph) const override;
    void connect_implementation_render_tasks(RendGraphDescVK& p_graph) const override;
    RendTaskDescVK make_deferred_construction_render_task_desc(bool p_dynamic) const override;
    RendTaskDescVK get_state_update_render_task_desc() const;
    RendTaskDescVK get_dynamic_publication_task_desc() const;

    Ref<ROComputePipelineVK> update_states_pipeline;
    Ref<ROComputePipelineVK> dynamic_publication_pipeline;
    Ref<ROBufferVK> pending_children;
};

} // namespace WhiteBirdEngine

#endif
