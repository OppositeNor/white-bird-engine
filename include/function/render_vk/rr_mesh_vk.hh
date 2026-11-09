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
#ifndef WBE_FILE_RR_MESH_VK_HH
#define WBE_FILE_RR_MESH_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "function/render/rr_typed.hh"
#include "function/render_vk/rd_params_mesh_vk.hh"
#include <string_view>

namespace WhiteBirdEngine {

class IRenderGraph;

/**
 * @class RRMeshVK
 * @brief Registers meshes: compiles the RUMesh3D a RDParamsMeshVK names into MeshResourcesVK (MeshCompilerVK::acquire()), which graphics_draw_mesh tasks draw by the registration key. Neither copyable nor movable.
 *
 * Compiled objects are shared through the GlobalRenderCache. Requires the RendererVK and the ResourceManager; runs on the
 * rendering thread.
 */
class RRMeshVK final : public RRTyped<RDParamsMeshVK> {
public:
    RRMeshVK();
    virtual ~RRMeshVK() override;
    RRMeshVK(const RRMeshVK&) = delete;
    RRMeshVK(RRMeshVK&&) noexcept = delete;
    RRMeshVK& operator=(const RRMeshVK&) = delete;
    RRMeshVK& operator=(RRMeshVK&&) noexcept = delete;

    virtual TypeID get_description_type_uuid() const override;
    virtual Unique<IResourceDescription> deserialize_description(const DataValue& p_description, RenderAllocator* p_allocator) const override;

protected:
    /**
     * @throws std::runtime_error If the renderer or the resource manager is unavailable, the source resource is missing, or
     * compiling fails.
     */
    virtual Ref<IRenderObject> compile_typed(IRenderGraph& p_graph, std::string_view p_key, const RDParamsMeshVK& p_params) override;
};

} // namespace WhiteBirdEngine

#endif
