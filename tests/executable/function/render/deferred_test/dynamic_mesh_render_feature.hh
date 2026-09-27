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
#ifndef WBE_FILE_DYNAMIC_MESH_RENDER_FEATURE_HH
#define WBE_FILE_DYNAMIC_MESH_RENDER_FEATURE_HH

#include "function/render/render_resource_unit.hh"
#include "function/render_vk/geometry_draw_data_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/static_set_resources_vk.hh"

namespace WhiteBirdEngine {
class RendererVK;

/**
 * @class DynamicMeshRenderFeature
 * @brief Fixed known opaque mesh demonstration resources. Neither copyable nor movable.
 * All methods run on the rendering thread. Geometry and capacity require reconstruction to change.
 */
class DynamicMeshRenderFeature {
public:
    DynamicMeshRenderFeature() = delete;
    /**
     * @brief Prepare a mesh once using the existing geometry, vertex-input and material compilers.
     */
    DynamicMeshRenderFeature(
        RendererVK* p_renderer, RefRes<RUMesh3D> p_mesh, RefRes<RUGraphicsPipeline> p_material_pipeline, uint32_t p_capacity);
    ~DynamicMeshRenderFeature() = default;
    DynamicMeshRenderFeature(const DynamicMeshRenderFeature&) = delete;
    DynamicMeshRenderFeature(DynamicMeshRenderFeature&&) = delete;
    DynamicMeshRenderFeature& operator=(const DynamicMeshRenderFeature&) = delete;
    DynamicMeshRenderFeature& operator=(DynamicMeshRenderFeature&&) = delete;
    /**
     * @brief Register stable containers before graph construction.
     */
    void register_resources(RendGraphDescVK& p_graph) const;
    /**
     * @brief Publish a complete dense list of visible transforms to all slots. Capacity overflow throws.
     */
    void publish_visible_transforms(std::span<const glm::mat4> p_transforms);
    /**
     * @brief Construct an indirect pass with task-local shader mappings and explicit mesh reads.
     * Existing attachments are loaded; the caller declares pass-specific shader reads and dependencies.
     */
    RendTaskDescVK make_task(std::string_view p_task_id,
        Ref<ROGraphicsPipelineVK> p_pipeline,
        VkExtent2D p_extent,
        std::span<const std::string_view> p_colors = {},
        std::optional<std::string_view> p_depth = std::nullopt) const;

private:
    const BaseRenderDataVK* render_data;
    Ref<StaticSetResourcesVK> geometry;
    Unique<GeometryDrawDataVK> draw_data;
    Ref<ROCMultislotsVK> nodes;
    Ref<ROCMultislotsVK> commands;
};

} // namespace WhiteBirdEngine
#endif
