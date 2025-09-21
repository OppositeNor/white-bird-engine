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

#include "core/surface/3d_primitive.hh"
#include "function/render/material.hh"
#include "global/stl_allocator.hh"
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace WhiteBirdEngine {

/**
 * @brief Types for the render database.
 *
 * @tparam T
 */
template <typename T>
concept RenderDatabaseTypeConcept = requires {
    // The type of the allocator
    typename T::AllocType;
    // The type of images
    typename T::ImageTypeGPU;
};

/**
 * @class RenderDatabase
 * @brief Renderer render database.
 *
 * @todo Test
 */
template <RenderDatabaseTypeConcept Types>
class RenderDatabase {
private:
    using AllocType = Types::AllocType;
    using ImageTypeGPU = Types::ImageTypeGPU;

    template <typename T>
    using vector = ::WhiteBirdEngine::vector<T, AllocType>;
    template <typename T>
    using set = ::WhiteBirdEngine::set<T, AllocType>;
public:

    ~RenderDatabase() {}
    RenderDatabase(const RenderDatabase&) = delete;
    RenderDatabase(RenderDatabase&&) = delete;
    RenderDatabase& operator=(const RenderDatabase&) = delete;
    RenderDatabase& operator=(RenderDatabase&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator for resource allocations.
     */
    RenderDatabase(AllocType* p_allocator);


    /**
     * @brief Set the static opaque triangles.
     *
     * @param p_vertices The vertices of the triangles. Note that this vector have to be at the same memory pool
     * with this render database.
     * @param p_indices The indices of the triangles, with respect to its material.
     */
    template <typename VertType, typename IndxType>
    void set_static_opaque_triangles(VertType&& p_vertices, IndxType&& p_indices) {
        set_opaque_triangles(static_opaque_triangle_map, static_opaque_triangle_vertices, static_opaque_triangle_indices,
                             std::forward<VertType>(p_vertices), std::forward<IndxType>(p_indices));
    }

    /**
     * @brief Set the dynamic opaque triangles.
     *
     * @param p_vertices The vertices of the triangles. Note that this vector have to be at the same memory pool
     * with this render database.
     * @param p_indices The indices of the triangles, with respect to its material.
     */
    template <typename VertType, typename IndxType>
    void set_dynamic_opaque_triangles(VertType&& p_vertices, IndxType&& p_indices) {
        set_opaque_triangles(dynamic_opaque_triangle_map, dynamic_opaque_triangle_vertices, dynamic_opaque_triangle_indices,
                             std::move(p_vertices), std::move(p_indices));
    }

    /**
     * @brief Register an image referencing to the GPU resource to the database.
     *
     * @param p_key The key of the image to register.
     * @param p_image The image to be registered.
     */
    void register_image_gpu(const std::string& p_key, ImageTypeGPU p_image) {
        images[p_key] = p_image;
    }

    /**
     * @brief Get an image referencing to the GPU resource from the database.
     *
     * @param p_key The key of the image.
     * @return The image instance.
     */
    ImageTypeGPU get_image_gpu(const std::string& p_key) const {
        return images.at(p_key);
    }

    /**
     * @brief Remove an image referencing to the GPU resource from the database.
     * This will NOT free the allocated GPU resource. The resource should be freed
     * mannually.
     *
     * @param p_key The key of the image.
     */
    void remove_image_gpu(const std::string& p_key) const {
        std::erase(images.find(p_key));
    }

private:

    AllocType* allocator;
    std::map<MaterialID, std::pair<size_t, size_t>> static_opaque_triangle_map;
    vector<Vertex3DTextured> static_opaque_triangle_vertices;
    vector<uint32_t> static_opaque_triangle_indices;

    std::map<MaterialID, std::pair<size_t, size_t>> dynamic_opaque_triangle_map;
    vector<Vertex3DTextured> dynamic_opaque_triangle_vertices;
    vector<uint32_t> dynamic_opaque_triangle_indices;

    std::map<std::string, ImageTypeGPU> images;

    void set_opaque_triangles(
        std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
        vector<Vertex3DTextured>& p_opaque_triangle_vertices,
        vector<uint32_t>& p_opaque_triangle_indices,
        vector<Vertex3DTextured>&& p_vertices,
        vector<std::pair<MaterialID, Triangle3DIndxTextured>>&& p_indices);

    void set_opaque_triangles(
        std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
        vector<Vertex3DTextured>& p_opaque_triangle_vertices,
        vector<uint32_t>& p_opaque_triangle_indices,
        const vector<Vertex3DTextured>& p_vertices,
        const vector<std::pair<MaterialID, Triangle3DIndxTextured>>& p_indices);

};

template <RenderDatabaseTypeConcept Types>
RenderDatabase<Types>::RenderDatabase(AllocType* p_allocator)
    : static_opaque_triangle_vertices(p_allocator), static_opaque_triangle_indices(p_allocator),
    dynamic_opaque_triangle_vertices(p_allocator), dynamic_opaque_triangle_indices(p_allocator) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Failed to create renderer database: allocator is null.");
    }
    allocator = p_allocator;
}

template <RenderDatabaseTypeConcept Types>
void RenderDatabase<Types>::set_opaque_triangles(
    std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
    vector<Vertex3DTextured>& p_opaque_triangle_vertices,
    vector<uint32_t>& p_opaque_triangle_indices,
    vector<Vertex3DTextured>&& p_vertices,
    vector<std::pair<MaterialID, Triangle3DIndxTextured>>&& p_indices) {

    p_opaque_triangle_map.clear();
    p_opaque_triangle_vertices = std::move(p_vertices);
    p_opaque_triangle_vertices.swap(p_opaque_triangle_vertices);  // Make the vector memory space compact
    p_opaque_triangle_indices = vector<uint32_t>(allocator);

    using TriType = std::pair<MaterialID, Triangle3DIndxTextured>;
    std::sort(p_indices.begin(), p_indices.end(), [](
        const TriType& p_tri1, const TriType& p_tri2) -> bool {
              return p_tri1.first < p_tri2.first;
              });
    MaterialID current_id = MATERIAL_INVALID;
    for (auto& tri : p_indices) {
        if (tri.first != current_id) {
            // Asign the next material ID's indices.
            p_opaque_triangle_map[tri.first].first = 0;
            p_opaque_triangle_map[tri.first].second = p_opaque_triangle_indices.size();
            current_id = tri.first;
        }
        // Asign indices.
        p_opaque_triangle_indices.insert(p_opaque_triangle_indices.end(),
                                         tri.second.vertex_indices, tri.second.vertex_indices + 3);
        p_opaque_triangle_map[tri.first].first += 3;
    }
    p_indices.clear();
    p_indices.swap(p_indices); // Clear resources allocated by indices.
}

template <RenderDatabaseTypeConcept Types>
void RenderDatabase<Types>::set_opaque_triangles(
    std::map<MaterialID, std::pair<size_t, size_t>>& p_opaque_triangle_map,
    vector<Vertex3DTextured>& p_opaque_triangle_vertices,
    vector<uint32_t>& p_opaque_triangle_indices,
    const vector<Vertex3DTextured>& p_vertices,
    const vector<std::pair<MaterialID, Triangle3DIndxTextured>>& p_indices) {

    p_opaque_triangle_map.clear();
    p_opaque_triangle_vertices = p_vertices;
    p_opaque_triangle_indices = vector<uint32_t>(allocator);

    using TriType = std::pair<MaterialID, Triangle3DIndxTextured>;
    vector<TriType> indices(p_indices);
    std::sort(indices.begin(), indices.end(), [](
        const TriType& p_tri1, const TriType& p_tri2) -> bool {
              return p_tri1.first < p_tri2.first;
              });
    MaterialID current_id = MATERIAL_INVALID;
    for (auto& tri : indices) {
        if (tri.first != current_id) {
            // Asign the next material ID's indices.
            p_opaque_triangle_map[tri.first].first = 0;
            p_opaque_triangle_map[tri.first].second = p_opaque_triangle_indices.size();
            current_id = tri.first;
        }
        // Asign indices.
        p_opaque_triangle_indices.insert(p_opaque_triangle_indices.end(),
                                         tri.second.vertex_indices, tri.second.vertex_indices + 3);
        p_opaque_triangle_map[tri.first].first += 3;
    }
}


}

#endif
