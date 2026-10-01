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
#include "function/render_vk/mesh_resources_vk.hh"
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
 * @brief Draws one moving mesh compiled by MeshCompilerVK. Neither copyable nor movable.
 * All methods run on the rendering thread. Geometry requires reconstruction to change; set_transform() moves the mesh.
 */
class DynamicMeshRenderFeature {
public:
    DynamicMeshRenderFeature() = delete;
    /**
     * @brief Compile p_mesh against p_material_pipeline's vertex input and material contract.
     */
    DynamicMeshRenderFeature(RendererVK* p_renderer, RefRes<RUMesh3D> p_mesh, RefRes<RUGraphicsPipeline> p_material_pipeline);
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
     * @brief Place the whole mesh; every frame slot applies the latest transform.
     */
    void set_transform(const glm::mat4& p_transform);
    /**
     * @brief Construct an indirect pass drawing the mesh's opaque category with its resources mapped to the pipeline.
     * Existing attachments are loaded; the caller declares pass-specific shader reads and dependencies.
     */
    RendTaskDescVK make_task(std::string_view p_task_id,
        Ref<ROGraphicsPipelineVK> p_pipeline,
        VkExtent2D p_extent,
        std::span<const std::string_view> p_colors = {},
        std::optional<std::string_view> p_depth = std::nullopt) const;

private:
    const BaseRenderDataVK* render_data;
    Ref<MeshResourcesVK> resources;
};

} // namespace WhiteBirdEngine
#endif
