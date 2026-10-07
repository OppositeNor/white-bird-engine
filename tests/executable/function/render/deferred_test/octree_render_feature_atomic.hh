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
#ifndef WBE_FILE_OCTREE_RENDER_FEATURE_ATOMIC_HH
#define WBE_FILE_OCTREE_RENDER_FEATURE_ATOMIC_HH

#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/mesh_resources_vk.hh"
#include "i_octree_render_feature.hh"
#include <atomic>
#include <cstdint>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class OctreeRenderFeatureAtomic
 * @brief Octree construction whose CAS owners initialize and publish child blocks immediately.
 * @details Contenders that observe an allocation in progress are retried from the deferred lists; no publication pass is needed.
 * Neither copyable nor movable; confined to the rendering thread.
 */
class OctreeRenderFeatureAtomic final : public IOctreeRenderFeature {
public:
    OctreeRenderFeatureAtomic() = delete;
    /**
     * @brief Compiles the atomic-publication octree shaders and creates the shared buffers on the rendering thread.
     * @details The renderer and debug-mode state must outlive this object and its registered graphs. Invalid dependencies throw.
     * @param p_rebuild_static_every_frame When true, the full static-scene construction runs every frame instead of once.
     */
    OctreeRenderFeatureAtomic(RendererVK* p_renderer,
        Ref<MeshResourcesVK> p_static_mesh_resources,
        const std::atomic<VoxelDebugViewMode>* p_debug_view_mode,
        bool p_rebuild_static_every_frame = false);
    virtual ~OctreeRenderFeatureAtomic() override = default;
    OctreeRenderFeatureAtomic(const OctreeRenderFeatureAtomic&) = delete;
    OctreeRenderFeatureAtomic(OctreeRenderFeatureAtomic&&) = delete;
    OctreeRenderFeatureAtomic& operator=(const OctreeRenderFeatureAtomic&) = delete;
    OctreeRenderFeatureAtomic& operator=(OctreeRenderFeatureAtomic&&) = delete;

    virtual std::string_view get_name() const override;

private:
    virtual uint32_t get_reset_group_count() const override;
    virtual void register_implementation_render_graph_data(RendGraphDescVK& p_graph) const override;
    virtual void add_static_publication_render_tasks(RendGraphDescVK& p_graph) const override;
    virtual void add_dynamic_publication_render_tasks(RendGraphDescVK& p_graph) const override;
    virtual void connect_implementation_render_tasks(RendGraphDescVK& p_graph) const override;
    virtual RendTaskDescVK make_deferred_construction_render_task_desc(bool p_dynamic) const override;
};

} // namespace WhiteBirdEngine

#endif
