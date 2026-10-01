/* Copyright 2026 OppositeNor

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
#ifndef WBE_FILE_DYNAMIC_MESH_COMPILER_VK_HH
#define WBE_FILE_DYNAMIC_MESH_COMPILER_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/dynamic_mesh_resources_vk.hh"
#include "function/render_vk/i_mesh_compiler_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstdint>
#include <optional>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class DynamicMeshCompilerVK
 * @brief Compiles one 3D mesh into Vulkan resources drawn with a fixed capacity of per-frame instance transforms.
 */
class DynamicMeshCompilerVK final : public IMeshCompilerVK {
public:
    DynamicMeshCompilerVK() = delete;
    ~DynamicMeshCompilerVK() override = default;
    DynamicMeshCompilerVK(const DynamicMeshCompilerVK&) = delete;
    DynamicMeshCompilerVK(DynamicMeshCompilerVK&&) noexcept = delete;
    DynamicMeshCompilerVK& operator=(const DynamicMeshCompilerVK&) = delete;
    DynamicMeshCompilerVK& operator=(DynamicMeshCompilerVK&&) noexcept = delete;

    /**
     * @brief Construct a compiler using renderer-owned Vulkan services.
     * @throws std::runtime_error If the renderer or its render data is missing.
     */
    explicit DynamicMeshCompilerVK(RendererVK* p_renderer);

    /**
     * @brief Compile a mesh whose every submesh is drawn for up to p_capacity instances.
     *
     * Instance k of every submesh reads node transform k. Vertex metadata supplies layout and mesh-node contracts only.
     * The result starts with no visible instances. It is neither copyable nor movable and must be shared through Ref.
     *
     * @throws std::runtime_error If the mesh is empty, a submesh has no indices or material, submeshes do not share
     * binaries, p_capacity is zero or overflows the instance table, or reflected contracts are incompatible.
     */
    Ref<DynamicMeshResourcesVK> compile(RefRes<RUMesh3D> p_mesh,
        uint32_t p_capacity,
        const ShaderMetadata& p_vertex_metadata,
        std::string_view p_vertex_entry,
        const std::optional<MaterialTextureContract>& p_material_contract,
        const Config& p_config) const;
};

} // namespace WhiteBirdEngine

#endif
