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
#ifndef WBE_FILE_STATIC_SET_RENDER_RESOURCES_VK_HH
#define WBE_FILE_STATIC_SET_RENDER_RESOURCES_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

struct RendTaskDescVK;

inline constexpr std::string_view REND_RES_STATIC_SET_VERTICES = "static_set_vertices";
inline constexpr std::string_view REND_RES_STATIC_SET_INDICES = "static_set_indices";
inline constexpr std::string_view REND_RES_STATIC_SET_OPAQUE_DRAW_BATCHES = "static_set_opaque_draw_batches";
inline constexpr std::string_view REND_RES_STATIC_SET_MASKED_DRAW_BATCHES = "static_set_masked_draw_batches";
inline constexpr std::string_view REND_RES_STATIC_SET_SUBMESHES = "submeshes";
inline constexpr std::string_view REND_RES_STATIC_SET_NODES = "nodes";
inline constexpr std::string_view REND_RES_STATIC_SET_INSTANCES = "instances";
inline constexpr std::string_view REND_RES_STATIC_SET_MATERIAL_SAMPLER = "material_sampler";
inline constexpr std::string_view REND_RES_STATIC_SET_TEXTURES = "static_set_textures";

/**
 * @class StaticSetRenderResourcesVK
 * @brief Pipeline-independent Vulkan resources compiled for static opaque and masked geometry.
 *
 * The object is neither copyable nor movable and is shared through Ref. Its allocator and every contained render object must
 * outlive task descriptors configured from it. Render objects remain confined to the rendering thread.
 */
class StaticSetRenderResourcesVK final {
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

    StaticSetRenderResourcesVK() = delete;
    ~StaticSetRenderResourcesVK() = default;
    StaticSetRenderResourcesVK(const StaticSetRenderResourcesVK&) = delete;
    StaticSetRenderResourcesVK(StaticSetRenderResourcesVK&&) noexcept = delete;
    StaticSetRenderResourcesVK& operator=(const StaticSetRenderResourcesVK&) = delete;
    StaticSetRenderResourcesVK& operator=(StaticSetRenderResourcesVK&&) noexcept = delete;

    /** @brief Construct empty allocator-owned static-set resource storage. */
    explicit StaticSetRenderResourcesVK(RenderAllocator* p_allocator);

    /**
     * @brief Register all available resources on one task descriptor and declare reflected shader reads.
     * @return The supplied descriptor.
     */
    RendTaskDescVK& register_resources(RendTaskDescVK& p_task_desc) const;

    /** @brief Acquire the shared vertex buffer. */
    Ref<ROBufferVK> acquire_vertices() const {
        return vertices;
    }

    /** @brief Acquire the shared uint32 index buffer. */
    Ref<ROBufferVK> acquire_indices() const {
        return indices;
    }

    /** @brief Acquire every sampled material texture retained by this static set. */
    Ref<RenderResourceList> acquire_textures() const {
        return textures;
    }

    /** @brief Acquire opaque indexed-indirect draw commands. */
    Ref<ROBufferVK> acquire_opaque_draw_batches() const {
        return opaque_draw_batches;
    }

    /** @brief Acquire masked indexed-indirect draw commands. */
    Ref<ROBufferVK> acquire_masked_draw_batches() const {
        return masked_draw_batches;
    }

    /** @brief Return the number of opaque draw batches. */
    size_t get_opaque_draw_batch_count() const {
        return opaque_batches.size();
    }

    /** @brief Return one opaque draw batch. */
    const DrawBatch& get_opaque_draw_batch(size_t p_index) const;

    /** @brief Return the number of masked draw batches. */
    size_t get_masked_draw_batch_count() const {
        return masked_batches.size();
    }

    /** @brief Return one masked draw batch. */
    const DrawBatch& get_masked_draw_batch(size_t p_index) const;

private:
    friend class CompiledStaticSetVK;
    friend class StaticSetCompilerVK;

    RenderAllocator* allocator = nullptr;
    Ref<ROBufferVK> vertices = MEM_NULL;
    Ref<ROBufferVK> indices = MEM_NULL;
    Ref<ROBufferVK> opaque_draw_batches = MEM_NULL;
    Ref<ROBufferVK> masked_draw_batches = MEM_NULL;
    Ref<ROBufferVK> submeshes = MEM_NULL;
    Ref<ROBufferVK> nodes = MEM_NULL;
    Ref<ROBufferVK> instances = MEM_NULL;
    Ref<ROBufferVK> material_params = MEM_NULL;
    Ref<ROSamplerVK> material_sampler = MEM_NULL;
    Ref<RenderResourceList> textures = MEM_NULL;
    Vector<DrawBatch, RenderAllocator> opaque_batches;
    Vector<DrawBatch, RenderAllocator> masked_batches;
    std::string material_params_resource_id;
};

} // namespace WhiteBirdEngine

#endif
