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
#ifndef WBE_FILE_RENDER_DATABASE_HH
#define WBE_FILE_RENDER_DATABASE_HH

#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "core/surface/3d_primitive.hh"
#include "function/render/i_render_object.hh"
#include "global/stl_allocator.hh"
#include "render_defs.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include "utils/interface/i_singleton.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <string>

namespace WhiteBirdEngine {

/**
 * @class RenderDatabase
 * @brief Renderer render database.
 *
 * @todo Test
 */
class RenderDatabase final : public ISingleton<RenderDatabase> {
private:
    using AllocType = RenderAllocator;
    template <typename T, typename Allocator = AllocType>
    using Vector = ::WhiteBirdEngine::Vector<T, AllocType>;
    template <typename T>
    using Set = ::WhiteBirdEngine::Set<T, AllocType>;
    template <typename TKey, typename TVal>
    using Map = ::WhiteBirdEngine::Map<TKey, TVal, AllocType>;

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
        // A unique id for each triangle that distinguishes from other
        // submeshes.
        Vector<uint32_t> obj_id;
        // Represents a material that is used.
        struct MaterialUnit {
            // The instance of the material.
            RefRes<RUMaterial> material;
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
            : vertices(p_allocator), indices(p_allocator), obj_id(p_allocator), materials(p_allocator) {
        }

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

    static constexpr size_t CLEAN_DEAD_RO_WEAK_REFS_THREASHOLD = 128;

    /**
     * @class CompiledRenderResourceKey
     * @brief Key for a backend-neutral compiled render resource registry entry.
     *
     * A resource key alone is not enough because one resource can compile into
     * multiple backend objects, such as shaders, layouts, and pipelines.
     */
    struct CompiledRenderResourceKey {
        /**
         * @brief Manifest resource key or compiler-generated subresource key.
         */
        std::string resource_key;
        /**
         * @brief Runtime type id of the compiled render object.
         */
        HashCode render_object_type_id = 0;

        bool operator<(const CompiledRenderResourceKey& p_other) const {
            if (resource_key != p_other.resource_key) {
                return resource_key < p_other.resource_key;
            }
            return render_object_type_id < p_other.render_object_type_id;
        }
    };

    virtual ~RenderDatabase() override {
        singleton = nullptr;
    }
    WBE_R6_NDCD_DELETE_COPY_MOVE(RenderDatabase)

    /**
     * @brief Constructor.
     */
    RenderDatabase(AllocType* p_allocator);

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

    /**
     * @brief Register or replace a compiled render object for a resource key.
     *
     * @param p_resource_key The source resource key or a derived compiler key.
     * @param p_render_object_type_id The expected render object type id.
     * @param p_render_object The compiled render object to keep weakly.
     */
    void register_compiled_render_resource(
        const std::string& p_resource_key, HashCode p_render_object_type_id, RefWeak<IRenderObject> p_render_object);

    /**
     * @brief Get a compiled render object by resource key and object type.
     *
     * @return A reference, or null if no entry exists.
     */
    Ref<IRenderObject> get_compiled_render_resource(const std::string& p_resource_key, HashCode p_render_object_type_id);

    /**
     * @brief Check whether a live compiled render object is registered.
     */
    bool contains_compiled_render_resource(const std::string& p_resource_key, HashCode p_render_object_type_id) const;

    /**
     * @brief Clear every compiled render object registry entry.
     */
    void clear_compiled_render_resources();

    static RenderDatabase* get_singleton() {
        return singleton;
    }

private:
    inline static RenderDatabase* singleton = nullptr;

    AllocType* allocator = nullptr;

    Map<CompiledRenderResourceKey, RefWeak<IRenderObject>> compiled_render_resources;

    OpaqueTrianglesInfo static_opaque;
    OpaqueTrianglesInfo dynamic_opaque;

    void set_opaque_triangles_linear(OpaqueTrianglesInfo& p_triangles_info, RUMesh3D* p_mesh);
};

} // namespace WhiteBirdEngine

#endif
