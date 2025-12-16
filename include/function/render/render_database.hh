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
#ifndef __WBE_RENDER_DATABASE_HH__
#define __WBE_RENDER_DATABASE_HH__

#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/surface/3d_primitive.hh"
#include "function/render/material.hh"
#include "render_defs.hh"
#include "global/stl_allocator.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include "utils/interface/singleton.hh"

#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>

namespace WhiteBirdEngine {


/**
 * @class RenderDatabase
 * @brief Renderer render database.
 *
 * @todo Test
 */
class RenderDatabase : public Singleton<RenderDatabase> {
private:
    using AllocType = RenderAllocator;
    template <typename T, typename Allocator = AllocType>
    using Vector = ::WhiteBirdEngine::Vector<T, AllocType>;
    template <typename T>
    using Set = ::WhiteBirdEngine::Set<T, AllocType>;
public:
    // Represents a subregion.
    struct SubRegion {
        // The starting index of the subregion.
        size_t starting_index;
        // The number of elements of the subregion.
        size_t size;
    };

    /**
     * @class OpaqueTrianglesInfo
     * @brief Information of chunk of opaque triangles.
     */
    struct OpaqueTrianglesInfo {
        // The vertices of the triangles.
        Vector<Vertex3DTextured> vertices;
        // The indices of the triangles.
        Vector<uint32_t> indices;
        // A unique id for each triangle that distinguishes from other submeshes.
        Vector<uint32_t> obj_id;
        // Represents a material that is used.
        struct MaterialUnit {
            // The instance of the material.
            Ref<Material> material;
            // The subregion of indices that this material is applying on.
            SubRegion sub_region;
        };
        // The materials that are used.
        Vector<MaterialUnit> materials;

        /**
         * @brief Constructor.
         *
         * @param p_allocator The allocator to allocate resource on.
         */
        OpaqueTrianglesInfo(AllocType* p_allocator)
            : vertices(p_allocator), indices(p_allocator), obj_id(p_allocator), materials(p_allocator) {}

        /**
         * @brief Clear all the data.
         */
        void reset() {
            vertices.clear();
            indices.clear();
            obj_id.clear();
            materials.clear();
        }
    };

    ~RenderDatabase() {
        singleton = nullptr;
    }
    WBE_R6_NDCD_DELETE_COPY_MOVE(RenderDatabase)

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator for resource allocations.
     */
    RenderDatabase(AllocType* p_allocator, Unique<MaterialFactory> p_material_factory);


    /**
     * @brief Set the static opaque triangles.
     *
     * @param p_mesh The mesh containing the triangles' data.
     */
    void set_static_opaque_triangles(RUMesh3D* p_mesh) {
        set_opaque_triangles_linear(static_opaque, p_mesh);
    }

    /**
     * @brief Set the dynamic opaque triangles.
     *
     * @param p_mesh The mesh containing the triangles' data.
     */
    void set_dynamic_opaque_triangles(RUMesh3D* p_mesh) {
        set_opaque_triangles_linear(dynamic_opaque, p_mesh);
    }

    /**
     * @brief Get the static opaque triangles' info.
     *
     * @return The static opaque triangles' info.
     */
    const OpaqueTrianglesInfo& get_static_opaque_triangles_info() const {
        return static_opaque;
    }

    /**
     * @brief Get the dynamic opaque triangles' info.
     *
     * @return The dynamic opaque triangles' info.
     */
    const OpaqueTrianglesInfo& get_dynamic_opaque_triangle_vertices() const {
        return dynamic_opaque;
    }

    static RenderDatabase* get_singleton() {
        return singleton;
    }

private:

    inline static RenderDatabase* singleton = nullptr;
    Unique<MaterialFactory> material_factory;

    AllocType* allocator;

    OpaqueTrianglesInfo static_opaque;
    OpaqueTrianglesInfo dynamic_opaque;

    void set_opaque_triangles_linear(
        OpaqueTrianglesInfo& p_triangles_info,
        RUMesh3D* p_mesh);
};


} // namespace WhiteBirdEngine

#endif
