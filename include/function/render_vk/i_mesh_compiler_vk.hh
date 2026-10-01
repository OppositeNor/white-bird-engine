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
#ifndef WBE_FILE_I_MESH_COMPILER_VK_HH
#define WBE_FILE_I_MESH_COMPILER_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/mesh_buffer_compiler_vk.hh"
#include "function/render_vk/mesh_draw_batch_planner_vk.hh"
#include "function/render_vk/mesh_draw_batch_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/** Shader resource id of the submesh table declared by MeshDescriptorIndices. */
inline constexpr std::string_view REND_RES_MESH_SUBMESHES = "mesh_submeshes";
/** Shader resource id of the node-transform table declared by MeshDescriptorIndices. */
inline constexpr std::string_view REND_RES_MESH_NODES = "mesh_nodes";
/** Shader resource id of the instance-link table declared by MeshDescriptorIndices. */
inline constexpr std::string_view REND_RES_MESH_INSTANCES = "mesh_instances";
/** Default shader resource id of the reflected material parameter table. */
inline constexpr std::string_view REND_RES_MESH_MATERIAL_PARAMS = "material_params";
/** Shader resource id of the material sampler. */
inline constexpr std::string_view REND_RES_MESH_MATERIAL_SAMPLER = "material_sampler";

class RendererVK;

/**
 * @class IMeshCompilerVK
 * @brief Base of compilers that turn mesh resource units into pipeline-independent Vulkan resources.
 *
 * Derived compilers own their input format and result type; this base supplies the shared geometry, draw-planning, and
 * material compilation steps. Compilers run on the rendering thread and are neither copyable nor movable.
 */
class IMeshCompilerVK {
public:
    /**
     * @class Config
     * @brief Material-contract configuration used during resource compilation.
     */
    struct Config {
        Config() = default;
        ~Config() = default;
        Config(const Config&) = default;
        Config(Config&&) noexcept = default;
        Config& operator=(const Config&) = default;
        Config& operator=(Config&&) noexcept = default;

        /** Default material resource used to fill missing shader texture roles. */
        std::string default_material_resource_key = "default_material";
        /** Reflected material parameter-buffer base name. */
        std::string material_params_variable_name = std::string(REND_RES_MESH_MATERIAL_PARAMS);
    };

    IMeshCompilerVK() = delete;
    virtual ~IMeshCompilerVK() = default;
    IMeshCompilerVK(const IMeshCompilerVK&) = delete;
    IMeshCompilerVK(IMeshCompilerVK&&) noexcept = delete;
    IMeshCompilerVK& operator=(const IMeshCompilerVK&) = delete;
    IMeshCompilerVK& operator=(IMeshCompilerVK&&) noexcept = delete;

protected:
    /**
     * @struct MaterialBuild
     * @brief Material resources compiled for one ordered material table. Copyable and movable.
     */
    struct MaterialBuild {
        /** Reflected material parameter table; null without a material contract. */
        Ref<ROBufferVK> material_params = MEM_NULL;
        /** Sampler used by material textures; null without a material contract. */
        Ref<ROSamplerVK> material_sampler = MEM_NULL;
        /** Every sampled image view retained by the material table. */
        Ref<RenderResourceList> textures = MEM_NULL;
    };

    /**
     * @struct DrawPlan
     * @brief Move-only indirect commands and their batches for one pipeline's vertex input.
     */
    struct DrawPlan {
        DrawPlan() = delete;
        /**
         * @brief Construct empty storage owned by p_allocator, which must outlive the plan.
         */
        explicit DrawPlan(RenderAllocator* p_allocator);
        ~DrawPlan() = default;
        DrawPlan(const DrawPlan&) = delete;
        DrawPlan(DrawPlan&&) noexcept = default;
        DrawPlan& operator=(const DrawPlan&) = delete;
        DrawPlan& operator=(DrawPlan&&) noexcept = default;

        /** Tightly packed indirect commands in batch order. */
        Vector<VkDrawIndexedIndirectCommand, RenderAllocator> commands;
        /** Batches addressing ranges of commands. */
        Vector<MeshDrawBatchVK, RenderAllocator> batches;
    };

    /**
     * @brief Construct a compiler using renderer-owned Vulkan services.
     * @throws std::runtime_error If the renderer or its render data is missing.
     */
    explicit IMeshCompilerVK(RendererVK* p_renderer);

    /**
     * @brief Return the renderer whose services the compiler uses.
     */
    RendererVK* get_renderer() const {
        return renderer;
    }

    /**
     * @brief Return the renderer's base render data.
     */
    const BaseRenderDataVK* get_render_data() const;

    /**
     * @brief Check that the shader's reflected mesh node layout matches GeometryNodeGPU.
     * @throws std::runtime_error If the layout is missing or differs.
     */
    void validate_node_layout(const ShaderMetadata& p_vertex_metadata) const;

    /**
     * @brief Require a submesh material.
     * @throws std::runtime_error If p_material is null.
     */
    static void validate_material(RefRes<RUMaterial> p_material);

    /**
     * @brief Return the row of p_material in p_materials, appending it when absent.
     * @throws std::runtime_error If the table exceeds uint32 rows.
     */
    static uint32_t get_or_append_material_index(std::vector<RefRes<RUMaterial>>& p_materials, RefRes<RUMaterial> p_material);

    /**
     * @brief Create a host-visible storage buffer holding p_size bytes from p_data; returns null when p_size is zero.
     */
    Ref<ROBufferVK> create_storage_buffer(const void* p_data, size_t p_size) const;

    /**
     * @brief Create a host-visible indirect buffer holding p_commands; returns null when p_commands is empty.
     */
    Ref<ROBufferVK> create_indirect_buffer(std::span<const VkDrawIndexedIndirectCommand> p_commands) const;

    /**
     * @brief Compile or reuse the shared vertex and index buffers of a static mesh.
     */
    MeshBufferCompilerVK::BuildResult compile_geometry_buffers(RefRes<RUStaticMesh> p_mesh) const;

    /**
     * @brief Compile or reuse the shared vertex and index buffers of a 3D mesh.
     */
    MeshBufferCompilerVK::BuildResult compile_geometry_buffers(RefRes<RUMesh3D> p_mesh) const;

    /**
     * @brief Plan indirect draws for one vertex entry, split at the device's indirect draw limit.
     */
    DrawPlan plan_draws(
        std::span<const MeshDrawBatchPlannerVK::Draw> p_draws, const ShaderMetadata& p_vertex_metadata, std::string_view p_vertex_entry) const;

    /**
     * @brief Compile the material table, sampler, and texture views for p_materials.
     *
     * Without a material contract only an empty texture list is returned.
     * @throws std::runtime_error If the contract cannot be satisfied by the materials.
     */
    MaterialBuild compile_materials(const std::optional<MaterialTextureContract>& p_material_contract,
        const Config& p_config,
        std::span<const RefRes<RUMaterial>> p_materials) const;

private:
    RendererVK* renderer = nullptr;
};

} // namespace WhiteBirdEngine

#endif
