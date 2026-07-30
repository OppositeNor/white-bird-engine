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
#ifndef WBE_FILE_RENDER_DEFS_HH
#define WBE_FILE_RENDER_DEFS_HH

#include "core/allocator/heap_allocator_aligned_pool_impl_list.hh"
#include <any>
#include <string>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {
using RenderAllocator = HeapAllocatorAlignedPoolImplicitList;

/**
 * @class RenderBindpointDeclaration
 * @brief Caller-provided data for one reflected shader bindpoint.
 */
struct RenderBindpointDeclaration {
    /**
     * @brief Reflected shader field path, e.g. "camera_info.view". Empty for resource bindpoints such as materials.
     */
    std::string field_path;
    /**
     * @brief Typed source data. Data is copied during compilation.
     */
    std::any data;
};

/**
 * @class RenderBindpointDeclarationsBuilder
 * @brief A helper builder to construct arrays of resource bindings.
 */
struct RenderBindpointDeclarationsBuilder {
    /**
     * @brief Add a decleration.
     *
     * @param Reflected shader field path, e.g. "camera_info.view". Empty for resource bindpoints such as materials.
     * @param Typed source data. Data is copied during compilation.
     * @return *this
     */
    RenderBindpointDeclarationsBuilder& add_decleratoin(std::string_view p_field_path, std::any p_data) {
        declerations.push_back({.field_path = std::string(p_field_path), .data = p_data});
        return *this;
    }

    /**
     * @brief Add a resource bindpoint.
     *
     * @param p_data The resource.
     * @return *this.
     */
    RenderBindpointDeclarationsBuilder& add_resource_bindpoint(std::any p_data) {
        declerations.push_back({.field_path = std::string(), .data = p_data});
        return *this;
    }
    /**
     * @brief Build the resource bindings.
     *
     * @return The resource bindings.
     */
    std::vector<RenderBindpointDeclaration> build() const {
        return declerations;
    }

    std::vector<RenderBindpointDeclaration> declerations;
};

} // namespace WhiteBirdEngine

#endif
