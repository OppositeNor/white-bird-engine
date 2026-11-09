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
#ifndef WBE_FILE_RD_PARAMS_MESH_VK_HH
#define WBE_FILE_RD_PARAMS_MESH_VK_HH

#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/vulkan_enums_vk.hh"
#include <string>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class RDParamsMeshVK
 * @brief Description of a mesh registered on a render graph under the "mesh" kind (RRMeshVK). Copyable and movable.
 *
 * The mesh is compiled into MeshResourcesVK, which graphics_draw_mesh tasks draw by the registration key. Deserialization is
 * strict.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC, WBE_STRICT_DESERIALIZATION, WBE_REFLECT_SCRIPT) RDParamsMeshVK {
    /**
     * @brief ResourceManager key of the RUMesh3D resource.
     */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string mesh;
    /**
     * @brief Material resource used to fill missing shader texture roles.
     */
    WBE_META(WBE_REFLECT)
    std::string default_material = "default_material";
    /**
     * @brief Usage added to the shared vertex buffer, such as the acceleration-structure build-input usages.
     */
    WBE_META(WBE_REFLECT)
    std::vector<BufferUsageFlagVK> vertex_buffer_usage;
    /**
     * @brief Usage added to the shared index buffer, such as the acceleration-structure build-input usages.
     */
    WBE_META(WBE_REFLECT)
    std::vector<BufferUsageFlagVK> index_buffer_usage;

    bool operator==(const RDParamsMeshVK&) const = default;
};

} // namespace WhiteBirdEngine

#endif
