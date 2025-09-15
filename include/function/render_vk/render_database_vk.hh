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
#ifndef __WBE_RENDER_DATABASE_VK_HH__
#define __WBE_RENDER_DATABASE_VK_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/memory/reference_weak.hh"
#include "core/surface/3d_primitive.hh"
#include "core/surface/primitive_slots.hh"
#include "function/render/material.hh"
#include "function/render/texture.hh"
#include "global/stl_allocator.hh"
#include <cstddef>
#include <cstdint>
#include <map>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RenderDatabaseVK
 * @brief Vulkan renderer render database.
 *
 * @todo Test
 */
class RenderDatabaseVK {
public:
    ~RenderDatabaseVK() {}
    RenderDatabaseVK(const RenderDatabaseVK&) = delete;
    RenderDatabaseVK(RenderDatabaseVK&&) = delete;
    RenderDatabaseVK& operator=(const RenderDatabaseVK&) = delete;
    RenderDatabaseVK& operator=(RenderDatabaseVK&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator for resource allocations.
     */
    RenderDatabaseVK(HeapAllocatorAlignedPool* p_allocator);


    /**
     * @brief Set the static opaque triangles.
     *
     * @param p_vertices The vertices of the triangles. Note that this vector have to be at the same memory pool
     * with this render database.
     * @param p_indices The indices of the triangles, with respect to its material.
     */
    void set_static_opaque_triangles(vectorp<Vertex3DTextured>&& p_vertices,
                                     vectorp<std::pair<MaterialID, Triangle3DIndxTextured>>&& p_indices);

private:
    HeapAllocatorAlignedPool* allocator;

    std::map<MaterialID, std::pair<size_t, size_t>> static_opaque_triangle_map;
    vectorp<Vertex3DTextured> static_opaque_triangle_vertices;
    vectorp<uint32_t> static_opaque_triangle_indices;

    std::map<MaterialID, vectorp<RefWeak<SubMesh<SlotTextured>>*>> dynamic_opaque_sub_meshes;

    std::unordered_map<TextureID, VkImage> texture_map;

};

}

#endif
