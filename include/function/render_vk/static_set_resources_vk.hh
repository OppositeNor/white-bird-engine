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
#ifndef WBE_FILE_STATIC_SET_RESOURCES_VK_HH
#define WBE_FILE_STATIC_SET_RESOURCES_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

inline constexpr std::string_view REND_RES_STATIC_SET_VERTICES = "static_set_vertices";
inline constexpr std::string_view REND_RES_STATIC_SET_INDICES = "static_set_indices";
inline constexpr std::string_view REND_RES_STATIC_SET_OPAQUE_DRAW_BATCHES = "static_set_opaque_draw_batches";
inline constexpr std::string_view REND_RES_STATIC_SET_OPAQUE_DOUBLE_SIDED_DRAW_BATCHES = "static_set_opaque_double_sided_draw_batches";
inline constexpr std::string_view REND_RES_STATIC_SET_MASKED_DRAW_BATCHES = "static_set_masked_draw_batches";
inline constexpr std::string_view REND_RES_STATIC_SET_SUBMESHES = "static_set_submeshes";
inline constexpr std::string_view REND_RES_STATIC_SET_NODES = "static_set_nodes";
inline constexpr std::string_view REND_RES_STATIC_SET_INSTANCES = "static_set_instances";
inline constexpr std::string_view REND_RES_STATIC_SET_MATERIAL_PARAMS = "material_params";
inline constexpr std::string_view REND_RES_STATIC_SET_MATERIAL_SAMPLER = "material_sampler";
inline constexpr std::string_view REND_RES_STATIC_SET_TEXTURES = "static_set_textures";

/**
 * @class StaticSetResourcesVK
 * @brief Pipeline-independent Vulkan resources compiled from static geometry categories.
 *
 * The object owns no pipeline or render-task state. It is neither copyable nor movable and is shared through Ref. Every
 * contained render object is rendering-thread-confined and must be registered explicitly with each graph that uses it.
 */
class StaticSetResourcesVK final {
public:
    /**
     * @class DrawBatch
     * @brief One compatible range in an indexed-indirect command buffer.
     */
    struct DrawBatch {
        DrawBatch() = default;
        ~DrawBatch() = default;
        DrawBatch(const DrawBatch&) = default;
        DrawBatch(DrawBatch&&) noexcept = default;
        DrawBatch& operator=(const DrawBatch&) = default;
        DrawBatch& operator=(DrawBatch&&) noexcept = default;

        /** First tightly packed indirect command in the batch buffer. */
        uint32_t first_draw = 0;
        /** Number of tightly packed indirect commands. */
        uint32_t draw_count = 0;
        /** Dynamic vertex-input layout shared by the commands. */
        VertexInputDescriptionVK vertex_input;
        /** Shared byte offset used when binding the vertex buffer. */
        VkDeviceSize vertex_buffer_offset = 0;
    };

    StaticSetResourcesVK() = delete;
    ~StaticSetResourcesVK() = default;
    StaticSetResourcesVK(const StaticSetResourcesVK&) = delete;
    StaticSetResourcesVK(StaticSetResourcesVK&&) noexcept = delete;
    StaticSetResourcesVK& operator=(const StaticSetResourcesVK&) = delete;
    StaticSetResourcesVK& operator=(StaticSetResourcesVK&&) noexcept = delete;

    /** @brief Construct empty allocator-owned static-set resource storage. */
    explicit StaticSetResourcesVK(RenderAllocator* p_allocator);

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

    /** @brief Acquire the sampler used by static-set materials. */
    Ref<ROSamplerVK> acquire_material_sampler() const {
        return material_sampler;
    }

    /** @brief Acquire every sampled image retained by the material table. */
    Ref<RenderResourceList> acquire_textures() const {
        return textures;
    }

    /** @brief Return the graph-resource id expected by the material parameter reflection contract. */
    std::string_view get_material_params_resource_id() const {
        return material_params_resource_id;
    }

    /** @brief Return the number of opaque draw batches. */
    size_t get_opaque_draw_batch_count() const {
        return opaque_batches.size();
    }

    /** @brief Return one opaque draw batch. */
    const DrawBatch& get_opaque_draw_batch(size_t p_index) const;

    /**
     * @brief Return the number of opaque double-sided draw batches.
     */
    size_t get_opaque_double_sided_draw_batch_count() const {
        return opaque_double_sided_batches.size();
    }

    /**
     * @brief Return one opaque double-sided draw batch.
     */
    const DrawBatch& get_opaque_double_sided_draw_batch(size_t p_index) const;

    /** @brief Return the number of masked draw batches. */
    size_t get_masked_draw_batch_count() const {
        return masked_batches.size();
    }

    /** @brief Return one masked draw batch. */
    const DrawBatch& get_masked_draw_batch(size_t p_index) const;

private:
    friend class StaticSetCompilerVK;

    RenderAllocator* allocator = nullptr;
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
    Ref<RenderResourceList> textures = MEM_NULL;
    Vector<DrawBatch, RenderAllocator> opaque_batches;
    Vector<DrawBatch, RenderAllocator> opaque_double_sided_batches;
    Vector<DrawBatch, RenderAllocator> masked_batches;
    std::string material_params_resource_id = std::string(REND_RES_STATIC_SET_MATERIAL_PARAMS);
};

} // namespace WhiteBirdEngine

#endif
