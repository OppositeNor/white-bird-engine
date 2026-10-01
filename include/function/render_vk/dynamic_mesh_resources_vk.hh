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
#ifndef WBE_FILE_DYNAMIC_MESH_RESOURCES_VK_HH
#define WBE_FILE_DYNAMIC_MESH_RESOURCES_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render_vk/geometry_draw_data_vk.hh"
#include "function/render_vk/mesh_draw_batch_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include <cstddef>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <span>
#include <string_view>

namespace WhiteBirdEngine {

inline constexpr std::string_view REND_RES_DYNAMIC_MESH_VERTICES = "dynamic_mesh_vertices";
inline constexpr std::string_view REND_RES_DYNAMIC_MESH_INDICES = "dynamic_mesh_indices";
inline constexpr std::string_view REND_RES_DYNAMIC_MESH_DRAW_BATCHES = "dynamic_mesh_draw_batches";
inline constexpr std::string_view REND_RES_DYNAMIC_MESH_SUBMESHES = "dynamic_mesh_submeshes";
inline constexpr std::string_view REND_RES_DYNAMIC_MESH_NODES = "dynamic_mesh_nodes";
inline constexpr std::string_view REND_RES_DYNAMIC_MESH_INSTANCES = "dynamic_mesh_instances";
inline constexpr std::string_view REND_RES_DYNAMIC_MESH_MATERIAL_PARAMS = "dynamic_mesh_material_params";
inline constexpr std::string_view REND_RES_DYNAMIC_MESH_MATERIAL_SAMPLER = "dynamic_mesh_material_sampler";
inline constexpr std::string_view REND_RES_DYNAMIC_MESH_TEXTURES = "dynamic_mesh_textures";

/**
 * @class DynamicMeshResourcesVK
 * @brief Pipeline-independent Vulkan resources of one mesh drawn with per-frame instance transforms.
 *
 * Geometry, submesh, instance-link, and material resources are immutable. Node transforms and indirect commands live in
 * one buffer per frame in flight and change through publish_visible_transforms(). The object is neither copyable nor
 * movable, is shared through Ref, and is confined to the rendering thread.
 */
class DynamicMeshResourcesVK final {
public:
    DynamicMeshResourcesVK() = delete;
    ~DynamicMeshResourcesVK() = default;
    DynamicMeshResourcesVK(const DynamicMeshResourcesVK&) = delete;
    DynamicMeshResourcesVK(DynamicMeshResourcesVK&&) noexcept = delete;
    DynamicMeshResourcesVK& operator=(const DynamicMeshResourcesVK&) = delete;
    DynamicMeshResourcesVK& operator=(DynamicMeshResourcesVK&&) noexcept = delete;

    /**
     * @brief Construct empty allocator-owned dynamic-mesh resource storage.
     * @throws std::runtime_error If p_allocator is null.
     */
    explicit DynamicMeshResourcesVK(RenderAllocator* p_allocator);

    /** @brief Acquire the shared vertex buffer. */
    Ref<ROBufferVK> acquire_vertices() const {
        return vertices;
    }

    /** @brief Acquire the shared uint32 index buffer. */
    Ref<ROBufferVK> acquire_indices() const {
        return indices;
    }

    /** @brief Acquire the per-frame indexed-indirect draw commands. */
    Ref<ROCMultislotsVK> acquire_draw_batches() const {
        return draw_batches;
    }

    /** @brief Acquire per-submesh information addressed by instance records. */
    Ref<ROBufferVK> acquire_submeshes() const {
        return submeshes;
    }

    /** @brief Acquire the per-frame node transforms addressed by instance records. */
    Ref<ROCMultislotsVK> acquire_nodes() const {
        return nodes;
    }

    /** @brief Acquire drawn-instance links between submeshes and nodes. */
    Ref<ROBufferVK> acquire_instances() const {
        return instances;
    }

    /** @brief Acquire the reflected material parameter table; null without a material contract. */
    Ref<ROBufferVK> acquire_material_params() const {
        return material_params;
    }

    /** @brief Acquire the sampler used by the mesh's materials; null without a material contract. */
    Ref<ROSamplerVK> acquire_material_sampler() const {
        return material_sampler;
    }

    /** @brief Acquire every sampled image retained by the material table. */
    Ref<RenderResourceList> acquire_textures() const {
        return textures;
    }

    /** @brief Return the maximum number of instances one publication may contain. */
    uint32_t get_capacity() const {
        return capacity;
    }

    /** @brief Return the number of draw batches. */
    size_t get_draw_batch_count() const {
        return batches.size();
    }

    /**
     * @brief Return one draw batch.
     * @throws std::runtime_error If p_index is out of bounds.
     */
    const MeshDrawBatchVK& get_draw_batch(size_t p_index) const;

    /**
     * @brief Publish a dense list of visible instance transforms to every frame slot.
     *
     * Each slot applies the latest publication when it is next resolved.
     * @throws std::runtime_error If the list exceeds the capacity or a transform has no valid normal matrix.
     */
    void publish_visible_transforms(std::span<const glm::mat4> p_transforms);

private:
    friend class DynamicMeshCompilerVK;

    RenderAllocator* allocator = nullptr;
    uint32_t capacity = 0;
    Ref<ROBufferVK> vertices = MEM_NULL;
    Ref<ROBufferVK> indices = MEM_NULL;
    Ref<ROCMultislotsVK> draw_batches = MEM_NULL;
    Ref<ROBufferVK> submeshes = MEM_NULL;
    Ref<ROCMultislotsVK> nodes = MEM_NULL;
    Ref<ROBufferVK> instances = MEM_NULL;
    Ref<ROBufferVK> material_params = MEM_NULL;
    Ref<ROSamplerVK> material_sampler = MEM_NULL;
    Ref<RenderResourceList> textures = MEM_NULL;
    Vector<MeshDrawBatchVK, RenderAllocator> batches;
    Unique<GeometryDrawDataVK> draw_data = MEM_NULL;
};

} // namespace WhiteBirdEngine

#endif
