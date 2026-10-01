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

#include "core/memory/reference_strong.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/dynamic_mesh_resources_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "resource/resource_unit/resource_unit.hh"
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
class RendererVK;

/**
 * @class DynamicMeshRenderFeature
 * @brief Draws one dynamic mesh compiled by DynamicMeshCompilerVK. Neither copyable nor movable.
 * All methods run on the rendering thread. Geometry and capacity require reconstruction to change.
 */
class DynamicMeshRenderFeature {
public:
    DynamicMeshRenderFeature() = delete;
    /**
     * @brief Compile p_mesh for p_capacity instances against p_material_pipeline's vertex input and material contract.
     */
    DynamicMeshRenderFeature(
        RendererVK* p_renderer, RefRes<RUMesh3D> p_mesh, RefRes<RUGraphicsPipeline> p_material_pipeline, uint32_t p_capacity);
    ~DynamicMeshRenderFeature() = default;
    DynamicMeshRenderFeature(const DynamicMeshRenderFeature&) = delete;
    DynamicMeshRenderFeature(DynamicMeshRenderFeature&&) = delete;
    DynamicMeshRenderFeature& operator=(const DynamicMeshRenderFeature&) = delete;
    DynamicMeshRenderFeature& operator=(DynamicMeshRenderFeature&&) = delete;
    /**
     * @brief Import the dynamic mesh's resources into p_graph before graph construction.
     */
    void register_resources(RendGraphDescVK& p_graph) const;
    /**
     * @brief Publish a complete dense list of visible transforms to all slots. Capacity overflow throws.
     */
    void publish_visible_transforms(std::span<const glm::mat4> p_transforms);
    /**
     * @brief Construct an indirect pass that maps the pipeline's mesh resource ids to the dynamic mesh's resources.
     * Existing attachments are loaded; the caller declares pass-specific shader reads and dependencies.
     */
    RendTaskDescVK make_task(std::string_view p_task_id,
        Ref<ROGraphicsPipelineVK> p_pipeline,
        VkExtent2D p_extent,
        std::span<const std::string_view> p_colors = {},
        std::optional<std::string_view> p_depth = std::nullopt) const;

private:
    const BaseRenderDataVK* render_data;
    Ref<DynamicMeshResourcesVK> resources;
};

} // namespace WhiteBirdEngine
#endif
