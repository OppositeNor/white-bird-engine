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
#ifndef WBE_FILE_STATIC_MESH_COMPILER_VK_HH
#define WBE_FILE_STATIC_MESH_COMPILER_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/i_mesh_compiler_vk.hh"
#include "function/render_vk/static_mesh_resources_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <optional>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class StaticMeshCompilerVK
 * @brief Compiles static mesh categories into pipeline-independent Vulkan resources.
 */
class StaticMeshCompilerVK final : public IMeshCompilerVK {
public:
    StaticMeshCompilerVK() = delete;
    ~StaticMeshCompilerVK() override = default;
    StaticMeshCompilerVK(const StaticMeshCompilerVK&) = delete;
    StaticMeshCompilerVK(StaticMeshCompilerVK&&) noexcept = delete;
    StaticMeshCompilerVK& operator=(const StaticMeshCompilerVK&) = delete;
    StaticMeshCompilerVK& operator=(StaticMeshCompilerVK&&) noexcept = delete;

    /**
     * @brief Construct a compiler using renderer-owned Vulkan services.
     * @throws std::runtime_error If the renderer or its render data is missing.
     */
    explicit StaticMeshCompilerVK(RendererVK* p_renderer);

    /**
     * @brief Compile shared geometry with opaque, masked, and opaque double-sided draw batches.
     *
     * Vertex metadata supplies layout and mesh-node contracts only. The result owns no pipeline or render-task behavior.
     * Null categories are allowed, but at least one non-empty mesh is required.
     * Double-sided draws use the opaque vertex metadata and entry, with a separately selected no-culling graphics pipeline.
     * The returned object is neither copyable nor movable and must be shared through Ref.
     *
     * @throws std::runtime_error If resource data or reflected contracts are invalid or incompatible.
     */
    Ref<StaticMeshResourcesVK> compile(RefRes<RUStaticOpaqueMesh> p_opaque_mesh,
        RefRes<RUStaticMaskedMesh> p_masked_mesh,
        const ShaderMetadata& p_opaque_vertex_metadata,
        std::string_view p_opaque_vertex_entry,
        const ShaderMetadata& p_masked_vertex_metadata,
        std::string_view p_masked_vertex_entry,
        const std::optional<MaterialTextureContract>& p_material_contract,
        const Config& p_config,
        RefRes<RUStaticOpaqueDoubleSidedMesh> p_opaque_double_sided_mesh = MEM_NULL) const;
};

} // namespace WhiteBirdEngine

#endif
