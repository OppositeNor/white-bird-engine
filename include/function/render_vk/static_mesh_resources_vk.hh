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
#ifndef WBE_FILE_STATIC_MESH_RESOURCES_VK_HH
#define WBE_FILE_STATIC_MESH_RESOURCES_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_resource_list_vk.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/i_bindable_render_resource_vk.hh"
#include "function/render_vk/i_mesh_compiler_vk.hh"
#include "function/render_vk/mesh_draw_batch_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class StaticMeshResourcesVK
 * @brief Pipeline-independent Vulkan resources compiled from static mesh categories, with the source meshes they came from.
 *
 * The object owns no pipeline or render-task state. It is neither copyable nor movable and is shared through Ref. Every
 * contained render object is rendering-thread-confined. bind() imports each present member under `<key>.<member>`, using
 * the member names below; absent optional categories and material resources are not declared.
 */
class StaticMeshResourcesVK final : public IBindableRenderResourceVK {
public:
    /** Member name of the shared vertex buffer. */
    static constexpr std::string_view VERTICES = "vertices";
    /** Member name of the shared uint32 index buffer. */
    static constexpr std::string_view INDICES = "indices";
    /** Member name of the opaque indirect commands. */
    static constexpr std::string_view OPAQUE_DRAW_BATCHES = "opaque_draw_batches";
    /** Member name of the opaque double-sided indirect commands. */
    static constexpr std::string_view OPAQUE_DOUBLE_SIDED_DRAW_BATCHES = "opaque_double_sided_draw_batches";
    /** Member name of the masked indirect commands. */
    static constexpr std::string_view MASKED_DRAW_BATCHES = "masked_draw_batches";
    /** Member name of the submesh table. */
    static constexpr std::string_view SUBMESHES = "submeshes";
    /** Member name of the node-transform table. */
    static constexpr std::string_view NODES = "nodes";
    /** Member name of the instance-link table. */
    static constexpr std::string_view INSTANCES = "instances";
    /** Member name of the material parameter table. */
    static constexpr std::string_view MATERIAL_PARAMS = "material_params";
    /** Member name of the material sampler. */
    static constexpr std::string_view MATERIAL_SAMPLER = "material_sampler";
    /** Member name of the material texture list. */
    static constexpr std::string_view TEXTURES = "textures";

    StaticMeshResourcesVK() = delete;
    ~StaticMeshResourcesVK() = default;
    StaticMeshResourcesVK(const StaticMeshResourcesVK&) = delete;
    StaticMeshResourcesVK(StaticMeshResourcesVK&&) noexcept = delete;
    StaticMeshResourcesVK& operator=(const StaticMeshResourcesVK&) = delete;
    StaticMeshResourcesVK& operator=(StaticMeshResourcesVK&&) noexcept = delete;

    /** @brief Construct empty allocator-owned static-mesh resource storage. */
    explicit StaticMeshResourcesVK(RenderAllocator* p_allocator);

    /** @brief Acquire the shared vertex buffer. */
    Ref<ROBufferVK> acquire_vertices() const {
        return vertices;
    }

    /** @brief Acquire the shared uint32 index buffer. */
    Ref<ROBufferVK> acquire_indices() const {
        return indices;
    }

    /** @brief Acquire opaque indexed-indirect draw commands. */
    Ref<ROBufferVK> acquire_opaque_draw_batches() const {
        return opaque_draw_batches;
    }

    /**
     * @brief Acquire opaque double-sided indexed-indirect draw commands.
     */
    Ref<ROBufferVK> acquire_opaque_double_sided_draw_batches() const {
        return opaque_double_sided_draw_batches;
    }

    /** @brief Acquire masked indexed-indirect draw commands. */
    Ref<ROBufferVK> acquire_masked_draw_batches() const {
        return masked_draw_batches;
    }

    /** @brief Acquire per-submesh information addressed by instance records. */
    Ref<ROBufferVK> acquire_submeshes() const {
        return submeshes;
    }

    /** @brief Acquire per-node transforms addressed by instance records. */
    Ref<ROBufferVK> acquire_nodes() const {
        return nodes;
    }

    /** @brief Acquire drawn-instance links between submeshes and nodes. */
    Ref<ROBufferVK> acquire_instances() const {
        return instances;
    }

    /** @brief Acquire the reflected material parameter table. */
    Ref<ROBufferVK> acquire_material_params() const {
        return material_params;
    }

    /** @brief Acquire the sampler used by static-mesh materials. */
    Ref<ROSamplerVK> acquire_material_sampler() const {
        return material_sampler;
    }

    /** @brief Acquire every sampled image retained by the material table. */
    Ref<RenderResourceListVK> acquire_textures() const {
        return textures;
    }

    /** @brief Return the opaque source mesh, or null when the category was not compiled. */
    RefRes<RUStaticOpaqueMesh> get_opaque_mesh() const {
        return opaque_mesh;
    }

    /** @brief Return the opaque double-sided source mesh, or null when the category was not compiled. */
    RefRes<RUStaticOpaqueDoubleSidedMesh> get_opaque_double_sided_mesh() const {
        return opaque_double_sided_mesh;
    }

    /** @brief Return the masked source mesh, or null when the category was not compiled. */
    RefRes<RUStaticMaskedMesh> get_masked_mesh() const {
        return masked_mesh;
    }

    /** @brief Return the shader resource id of the material parameter table configured at compilation. */
    std::string_view get_material_params_resource_id() const {
        return material_params_resource_id;
    }

    /** @brief Return the number of opaque draw batches. */
    size_t get_opaque_draw_batch_count() const {
        return opaque_batches.size();
    }

    /** @brief Return one opaque draw batch. */
    const MeshDrawBatchVK& get_opaque_draw_batch(size_t p_index) const;

    /**
     * @brief Return the number of opaque double-sided draw batches.
     */
    size_t get_opaque_double_sided_draw_batch_count() const {
        return opaque_double_sided_batches.size();
    }

    /**
     * @brief Return one opaque double-sided draw batch.
     */
    const MeshDrawBatchVK& get_opaque_double_sided_draw_batch(size_t p_index) const;

    /** @brief Return the number of masked draw batches. */
    size_t get_masked_draw_batch_count() const {
        return masked_batches.size();
    }

    /** @brief Return one masked draw batch. */
    const MeshDrawBatchVK& get_masked_draw_batch(size_t p_index) const;

    /**
     * @brief Import every present member under `<p_resource_id>.<member>`.
     * @throws std::runtime_error If p_resource_id is empty or a required member is missing.
     */
    void bind(RendGraphDescVK& p_graph, std::string_view p_resource_id) const override;

private:
    friend class StaticMeshCompilerVK;

    RenderAllocator* allocator = nullptr;
    RefRes<RUStaticOpaqueMesh> opaque_mesh = MEM_NULL;
    RefRes<RUStaticOpaqueDoubleSidedMesh> opaque_double_sided_mesh = MEM_NULL;
    RefRes<RUStaticMaskedMesh> masked_mesh = MEM_NULL;
    Ref<ROBufferVK> vertices = MEM_NULL;
    Ref<ROBufferVK> indices = MEM_NULL;
    Ref<ROBufferVK> opaque_draw_batches = MEM_NULL;
    Ref<ROBufferVK> opaque_double_sided_draw_batches = MEM_NULL;
    Ref<ROBufferVK> masked_draw_batches = MEM_NULL;
    Ref<ROBufferVK> submeshes = MEM_NULL;
    Ref<ROBufferVK> nodes = MEM_NULL;
    Ref<ROBufferVK> instances = MEM_NULL;
    Ref<ROBufferVK> material_params = MEM_NULL;
    Ref<ROSamplerVK> material_sampler = MEM_NULL;
    Ref<RenderResourceListVK> textures = MEM_NULL;
    Vector<MeshDrawBatchVK, RenderAllocator> opaque_batches;
    Vector<MeshDrawBatchVK, RenderAllocator> opaque_double_sided_batches;
    Vector<MeshDrawBatchVK, RenderAllocator> masked_batches;
    std::string material_params_resource_id = std::string(REND_RES_MESH_MATERIAL_PARAMS);
};

} // namespace WhiteBirdEngine

#endif
