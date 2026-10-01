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
#ifndef WBE_FILE_STATIC_MESH_ACCELERATION_STRUCTURE_COMPILER_VK_HH
#define WBE_FILE_STATIC_MESH_ACCELERATION_STRUCTURE_COMPILER_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/static_mesh_acceleration_structure_vk.hh"
#include "function/render_vk/static_mesh_resources_vk.hh"

namespace WhiteBirdEngine {

/**
 * @class StaticMeshAccelerationStructureCompilerVK
 * @brief Creates the ray-tracing acceleration structures of a compiled static mesh. Copyable and movable.
 *
 * Covers the opaque and opaque double-sided categories: one bottom-level structure per drawable submesh, instanced by the
 * top-level structure once per instance transform. Compilation allocates every structure, input, and scratch buffer but
 * records no GPU work; StaticMeshAccelerationStructureVK::record_build() performs the build. Rendering-thread only.
 */
class StaticMeshAccelerationStructureCompilerVK final {
public:
    StaticMeshAccelerationStructureCompilerVK() = delete;
    ~StaticMeshAccelerationStructureCompilerVK() = default;
    StaticMeshAccelerationStructureCompilerVK(const StaticMeshAccelerationStructureCompilerVK&) = default;
    StaticMeshAccelerationStructureCompilerVK(StaticMeshAccelerationStructureCompilerVK&&) noexcept = default;
    StaticMeshAccelerationStructureCompilerVK& operator=(const StaticMeshAccelerationStructureCompilerVK&) = default;
    StaticMeshAccelerationStructureCompilerVK& operator=(StaticMeshAccelerationStructureCompilerVK&&) noexcept = default;

    /**
     * @brief Construct a compiler over renderer and ray-tracing device state; both must outlive the compiler.
     * @throws std::runtime_error If either is null or the ray-tracing device commands are missing.
     */
    StaticMeshAccelerationStructureCompilerVK(const BaseRenderDataVK* p_render_data, const BaseRayTracingDataVK* p_ray_tracing_data);

    /**
     * @brief Create the acceleration structures of p_static_mesh.
     *
     * The mesh must have been compiled with StaticMeshAccelerationStructureVK::GEOMETRY_BUFFER_USAGE added to its vertex and
     * index buffer usage.
     *
     * @throws std::runtime_error If the mesh is null, has no drawable opaque geometry, lacks the required buffer usage, has
     * submeshes without a FLOAT3 position attribute or complete triangles, or exceeds device limits.
     */
    Ref<StaticMeshAccelerationStructureVK> compile(Ref<StaticMeshResourcesVK> p_static_mesh) const;

private:
    const BaseRenderDataVK* render_data = nullptr;
    const BaseRayTracingDataVK* ray_tracing_data = nullptr;
};

} // namespace WhiteBirdEngine

#endif
