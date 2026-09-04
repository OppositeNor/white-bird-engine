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
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {
using RenderAllocator = HeapAllocatorAlignedPoolImplicitList;

/**
 * @class RenderBindpointBufferDeclaration
 * @brief Byte range for one caller-provided reflected shader bindpoint.
 */
struct RenderBindpointBufferDeclaration {
    /**
     * @brief Reflected shader field path, e.g. "camera_info.view" or descriptor scope such as "material".
     */
    std::string field_path;
    /**
     * @brief Byte offset into the owning packed bindpoint buffer.
     */
    size_t offset = 0;
    /**
     * @brief Byte size inside the owning packed bindpoint buffer.
     */
    size_t size = 0;
    /**
     * @brief Render-object-backed buffer for storage-buffer descriptors.
     */
    Ref<IRenderObject> buffer_render_object = MEM_NULL;
};

/**
 * @class RenderBindpointImageDeclaration
 * @brief Caller-provided image bindpoint resolved from either a resource unit or a render object.
 */
struct RenderBindpointImageDeclaration {
    std::string field_path;
    RefRes<RUTexture> image_resource = MEM_NULL;
    Ref<IRenderObject> image_render_object = MEM_NULL;
};

/**
 * @class RenderBindpointSamplerDeclaration
 * @brief Caller-provided sampler bindpoint resolved from an already-built render object.
 */
struct RenderBindpointSamplerDeclaration {
    std::string field_path;
    Ref<IRenderObject> sampler_render_object = MEM_NULL;
};

/**
 * @class RenderBindpointDeclarations
 * @brief Aggregated caller-provided bindpoint data.
 */
struct RenderBindpointDeclarations {
    std::vector<uint8_t> buffer_data;
    std::vector<RenderBindpointBufferDeclaration> buffer_declerations;
    std::vector<RenderBindpointImageDeclaration> image_declerations;
    std::vector<RenderBindpointSamplerDeclaration> sampler_declerations;

    std::span<const uint8_t> get_buffer_data(const RenderBindpointBufferDeclaration& p_decleration) const {
        return std::span<const uint8_t>(buffer_data.data() + p_decleration.offset, p_decleration.size);
    }
};

/**
 * @class RenderBindpointDeclarationsBuilder
 * @brief A helper builder to construct arrays of resource bindings.
 */
struct RenderBindpointDeclarationsBuilder {
    /**
     * @brief Add a buffer decleration.
     *
     * @param p_field_path Reflected shader field path, e.g. "camera_info.view".
     * @param p_data Typed source data copied into the packed byte buffer.
     * @return *this
     */
    RenderBindpointDeclarationsBuilder& add_buffer_decleration(std::string_view p_field_path, std::span<const uint8_t> p_data) {
        RenderBindpointBufferDeclaration decleration{
            .field_path = std::string(p_field_path),
            .offset = declerations.buffer_data.size(),
            .size = p_data.size(),
            .buffer_render_object = MEM_NULL,
        };
        declerations.buffer_data.resize(declerations.buffer_data.size() + p_data.size());
        if (!p_data.empty()) {
            std::memcpy(declerations.buffer_data.data() + decleration.offset, p_data.data(), p_data.size());
        }
        declerations.buffer_declerations.push_back(decleration);
        return *this;
    }

    /**
     * @brief Add a buffer decleration from raw bytes.
     */
    RenderBindpointDeclarationsBuilder& add_buffer_decleration(std::string_view p_field_path, const void* p_data, size_t p_size) {
        return add_buffer_decleration(p_field_path, std::span<const uint8_t>(static_cast<const uint8_t*>(p_data), p_size));
    }

    /**
     * @brief Add a render-object-backed storage buffer declaration.
     */
    RenderBindpointDeclarationsBuilder& add_storage_buffer_decleration(std::string_view p_field_path,
        Ref<IRenderObject> p_buffer_render_object,
        size_t p_offset = 0,
        size_t p_size = std::numeric_limits<size_t>::max()) {
        declerations.buffer_declerations.push_back(RenderBindpointBufferDeclaration{
            .field_path = std::string(p_field_path), .offset = p_offset, .size = p_size, .buffer_render_object = p_buffer_render_object});
        return *this;
    }

    /**
     * @brief Add an image decleration backed by a resource unit.
     */
    RenderBindpointDeclarationsBuilder& add_image_decleration(std::string_view p_field_path, RefRes<RUTexture> p_image_resource) {
        declerations.image_declerations.push_back(
            {.field_path = std::string(p_field_path), .image_resource = p_image_resource, .image_render_object = MEM_NULL});
        return *this;
    }

    /**
     * @brief Add an image decleration backed by a render object.
     */
    RenderBindpointDeclarationsBuilder& add_image_decleration(std::string_view p_field_path, Ref<IRenderObject> p_image_render_object) {
        declerations.image_declerations.push_back(
            {.field_path = std::string(p_field_path), .image_resource = MEM_NULL, .image_render_object = p_image_render_object});
        return *this;
    }

    /**
     * @brief Add a sampler decleration.
     */
    RenderBindpointDeclarationsBuilder& add_sampler_decleration(std::string_view p_field_path, Ref<IRenderObject> p_sampler_render_object) {
        declerations.sampler_declerations.push_back(
            {.field_path = std::string(p_field_path), .sampler_render_object = p_sampler_render_object});
        return *this;
    }

    /**
     * @brief Build the resource bindings.
     *
     * @return The resource bindings.
     */
    RenderBindpointDeclarations build() const {
        return declerations;
    }

    RenderBindpointDeclarations declerations;
};

} // namespace WhiteBirdEngine

#endif
