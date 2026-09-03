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

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_role.hh"
#include "render_defs.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/interface/i_singleton.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

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
        /** Tightly interleaved vertex bytes for every draw group. */
        Vector<uint8_t> vertices;
        /** Local indices for every draw group. */
        Vector<uint32_t> indices;
        /** A unique id for each appended submesh. */
        Vector<uint32_t> obj_id;

        /**
         * @brief One unique material referenced by draw groups.
         */
        struct MaterialUnit {
            /** Material resource. */
            RefRes<RUMaterial> material;
        };
        /** Unique materials in stable draw order. */
        Vector<MaterialUnit> materials;

        /**
         * @brief One copied vertex attribute used to compare and retain layouts.
         */
        struct VertexAttribute {
            /** Exact shader-defined open role. */
            ShaderRole role;
            /** Exact 32-bit scalar or vector format. */
            RUVertexAttributeFormat format = RUVertexAttributeFormat::FLOAT;
            /** Byte offset inside one interleaved vertex. */
            size_t offset = 0;
            /** Byte size inside one interleaved vertex. */
            size_t size = 0;
        };

        /**
         * @brief Fixed vertex layout copied from a resource draw group.
         */
        struct VertexLayout {
            VertexLayout() = delete;
            ~VertexLayout() = default;
            VertexLayout(const VertexLayout&) = default;
            VertexLayout(VertexLayout&&) noexcept = default;
            VertexLayout& operator=(const VertexLayout&) = default;
            VertexLayout& operator=(VertexLayout&&) noexcept = default;

            /**
             * @brief Construct an empty layout using the render allocator.
             */
            explicit VertexLayout(AllocType* p_allocator) : attributes(p_allocator) {
            }

            /** Ordered attributes in resource declaration order. */
            Vector<VertexAttribute> attributes;
            /** Byte stride between vertices. */
            size_t vertex_stride = 0;
        };

        /**
         * @brief Contiguous geometry rendered with one material and vertex layout.
         */
        struct DrawGroup {
            DrawGroup() = delete;
            ~DrawGroup() = default;
            DrawGroup(const DrawGroup&) = default;
            DrawGroup(DrawGroup&&) noexcept = default;
            DrawGroup& operator=(const DrawGroup&) = default;
            DrawGroup& operator=(DrawGroup&&) noexcept = default;

            /**
             * @brief Construct an empty group using the render allocator.
             */
            explicit DrawGroup(AllocType* p_allocator) : vertex_layout(p_allocator) {
            }

            /** Byte offset bound for this group's first vertex. */
            size_t vertex_buffer_byte_offset = 0;
            /** Contiguous local-index range belonging to this group. */
            SubRegion index_sub_region{};
            /** Index into materials. */
            size_t material_index = 0;
            /** Fixed layout of the group's interleaved vertices. */
            VertexLayout vertex_layout;
            /** Contiguous indirect-command range generated for this group. */
            SubRegion indirect_command_sub_region{};
        };
        /** Draw groups in pipeline, material, and layout order. */
        Vector<DrawGroup> draw_groups;

        /**
         * @brief One graphics pipeline and its contiguous draw-group range.
         */
        struct GraphicsPipelineUnit {
            /** Graphics pipeline used by every draw group in the range. */
            RefRes<RUGraphicsPipeline> graphics_pipeline;
            /** Subregion of draw_groups rendered with this pipeline. */
            SubRegion draw_group_sub_region;
        };
        /** Graphics-pipeline batches in draw order. */
        Vector<GraphicsPipelineUnit> graphics_pipelines;

        /**
         * @brief Constructor.
         *
         * @param p_allocator The allocator to allocate resource on.
         */
        OpaqueTrianglesInfo(AllocType* p_allocator)
            : vertices(p_allocator), indices(p_allocator), obj_id(p_allocator), materials(p_allocator),
              draw_groups(p_allocator), graphics_pipelines(p_allocator) {
        }

        /**
         * @brief Clear all the data.
         */
        void reset() {
            vertices.clear();
            indices.clear();
            obj_id.clear();
            materials.clear();
            draw_groups.clear();
            graphics_pipelines.clear();
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
    RenderDatabase(const RenderDatabase&) = delete;
    RenderDatabase(RenderDatabase&&) noexcept = delete;
    RenderDatabase& operator=(const RenderDatabase&) = delete;
    RenderDatabase& operator=(RenderDatabase&&) noexcept = delete;

    /**
     * @brief Constructor.
     */
    RenderDatabase(AllocType* p_allocator);

    /**
     * @brief Set the static triangle set.
     *
     * @param p_mesh The mesh containing the triangles' data.
     */
    void set_static_triangle_set(RUMesh3D* p_mesh) {
        set_opaque_triangles_linear(static_opaque, p_mesh, nullptr, 0);
        static_opaque_mesh = p_mesh;
    }

    /**
     * @brief Rebuild static opaque data from its current resource values.
     *
     * Does nothing before a static opaque mesh has been set. The source mesh must remain alive until it is replaced.
     */
    void refresh_static_triangle_set();

    /**
     * @brief Set the static opaque geometry set.
     * @param p_set Static opaque set resource. It must outlive the render database reference.
     */
    void set_static_opaque_set(RefRes<RUStaticOpaqueSet> p_set) {
        if (p_set == nullptr) {
            throw std::runtime_error("Failed to set static opaque set: resource should not be NULL.");
        }
        static_opaque_set = p_set;
    }

    /**
     * @brief Set the static masked geometry set.
     * @param p_set Static masked set resource. It must outlive the render database reference.
     */
    void set_static_masked_set(RefRes<RUStaticMaskedSet> p_set) {
        if (p_set == nullptr) {
            throw std::runtime_error("Failed to set static masked set: resource should not be NULL.");
        }
        static_masked_set = p_set;
    }

    /**
     * @brief Set the dynamic opaque triangles.
     *
     * @param p_mesh The mesh containing the triangles' data.
     */
    void set_dynamic_opaque_triangles(RUMesh3D* p_mesh) {
        set_opaque_triangles_linear(dynamic_opaque, p_mesh, nullptr, 0);
    }

    /**
     * @brief Get the static triangle set's info.
     *
     * @return The static triangle set's info.
     */
    const OpaqueTrianglesInfo& get_static_triangle_set_info() const {
        return static_opaque;
    }

    /**
     * @brief Get the current static opaque set resource.
     */
    RefRes<RUStaticOpaqueSet> get_static_opaque_set() const {
        return static_opaque_set;
    }

    /**
     * @brief Get the current static masked set resource.
     */
    RefRes<RUStaticMaskedSet> get_static_masked_set() const {
        return static_masked_set;
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
     * @param p_render_object The compiled render object whose concrete type identifies the registry entry.
     */
    void register_compiled_render_resource(std::string_view p_resource_key, Ref<IRenderObject> p_render_object);

    /**
     * @brief Get a compiled render object by resource key and object type.
     *
     * @return A reference, or null if no entry exists.
     */
    Ref<IRenderObject> get_compiled_render_resource(std::string_view p_resource_key, HashCode p_render_object_type_id);

    /**
     * @brief Check whether a live compiled render object is registered.
     */
    bool contains_compiled_render_resource(std::string_view p_resource_key, HashCode p_render_object_type_id) const;

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
    RUMesh3D* static_opaque_mesh = nullptr;
    RefRes<RUStaticOpaqueSet> static_opaque_set;
    RefRes<RUStaticMaskedSet> static_masked_set;
    void set_opaque_triangles_linear(
        OpaqueTrianglesInfo& p_triangles_info, RUMesh3D* p_mesh, const uint32_t* p_preserved_object_ids, size_t p_preserved_object_id_count);
};

} // namespace WhiteBirdEngine

#endif
