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
#ifndef WBE_FILE_RENDER_RESOURCE_UNIT_HH
#define WBE_FILE_RENDER_RESOURCE_UNIT_HH

#include "core/reflection/reflection_defs.hh"
#include "core/reflection/serializable.hh"
#include "core/surface/3d_primitive.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class RUSchemaShader
 * @brief The scheme of RUShader.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemaShader : public RUSchema {
    WBE_DECL_SERIALIZABLE(RUSchemaShader)
    // The path to the shader file.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string path;
    // The path to the shader's metadata file.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string metadata;
    // Default vertex entry discovered for this shader module.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> vertex_entry;
    // Default tessellation entry discovered for this shader module.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> tessellation_entry;
    // Default geometry entry discovered for this shader module.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> geometry_entry;
    // Default fragment entry discovered for this shader module.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> fragment_entry;
    // Default compute entry discovered for this shader module.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> compute_entry;
};

/**
 * @class RUSchemaGraphicsPipeline
 * @brief The scheme of RUGraphicsPipeline.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemaGraphicsPipeline : public RUSchema {
    WBE_DECL_SERIALIZABLE(RUSchemaGraphicsPipeline)
    // The resource ID of the vertex shader.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> vertex_stage_id;
    // Entry function for the vertex stage.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> vertex_stage_entry;
    // The vertex shader of the pipeline.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaShader> vertex_stage;
    // The resource ID of the tessellation shader.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> tessellation_stage_id;
    // Entry function for the tessellation stage.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> tessellation_stage_entry;
    // The tessellation shader of the pipeline.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaShader> tessellation_stage;
    // The resource ID of the geometry shader.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> geometry_stage_id;
    // Entry function for the geometry stage.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> geometry_stage_entry;
    // The geometry shader of the pipeline.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaShader> geometry_stage;
    // The resource ID of the fragment shader.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> fragment_stage_id;
    // Entry function for the fragment stage.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> fragment_stage_entry;
    // The fragment shader of the pipeline.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaShader> fragment_stage;
    // Should enable depth test.
    WBE_META(WBE_REFLECT)
    bool depth_test = true;
    // Should enable depth write.
    WBE_META(WBE_REFLECT)
    bool depth_write = true;
    // Should cull back-facing triangles.
    WBE_META(WBE_REFLECT)
    bool cull_backfaces = true;
};

/**
 * @class RUSchemaMaterialTextureSlot
 * @brief The scheme of one texture slot in RUMaterial.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaMaterialTextureSlot {
    // The key used by the material shader to find this texture.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string texture_key;
    // Inline texture data.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaImage> texture;
    // A string referencing an already populated texture.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> texture_id;
};

/**
 * @class RUSchemaMaterial
 * @brief The scheme of RUMaterial.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemaMaterial : public RUSchema {
    WBE_DECL_SERIALIZABLE(RUSchemaMaterial)
    // The graphics pipelines that can render this material.
    WBE_META(WBE_REFLECT)
    std::vector<std::string> graphics_pipeline_ids;
    // The inline graphics pipeline used to render this material.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaGraphicsPipeline> graphics_pipeline;
    // The texture slots used by this material.
    WBE_META(WBE_REFLECT)
    std::vector<RUSchemaMaterialTextureSlot> textures;
};

/**
 * @class RUSchemaSubmesh3DGeometrySection
 * @brief Byte-addressed geometry section inside a submesh sidecar binary.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaSubmesh3DGeometrySection {
    // The geometry slot stored by this section.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string slot;
    // Byte offset from the beginning of the sidecar binary.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    size_t start = 0;
    // Byte length of this section.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    size_t size = 0;
    // The element type expected by the slot.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string type;
};

/**
 * @class RUSchemaSubmesh3D
 * @brief The scheme of RUSubmesh3D.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemaSubmesh3D : public RUSchema {
    WBE_DECL_SERIALIZABLE(RUSchemaSubmesh3D)
    // The material if it could be populated in place.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaMaterial> material;
    // A string referencing unpopulated material.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> material_id;
    // Resource-relative path to the sidecar geometry binary.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string geometry_path;
    // Byte-addressed sections inside the geometry binary.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::vector<RUSchemaSubmesh3DGeometrySection> geometry_sections;
};

/**
 * @class RUSchemaMesh3D
 * @brief The scheme of RUMesh3D.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemaMesh3D : public RUSchema {
    WBE_DECL_SERIALIZABLE(RUSchemaMesh3D)
    // Submeshes that could be populated in place.
    WBE_META(WBE_REFLECT)
    std::vector<RUSchemaSubmesh3D> submeshes;
    // Submesh ids referencing unpopulated submeshes.
    WBE_META(WBE_REFLECT)
    std::vector<std::string> submeshes_id;
};

/**
 * @class RUShader
 * @brief Shader resource unit. Contains a word count and a MemID pointing to
 * the data chunk that stores the SPIR-V words.
 *
 * Push any chunk-owned payload data before emplacing this unit so the stored
 * references remain valid. Chunk teardown now runs registered resource-unit
 * destructors before releasing the allocator storage behind the unit and its
 * payload.
 *
 */
struct RUShader : public ResourceUnit {
    // The shader SPIR-V words.
    RefRes<uint32_t> data;
    // The number of SPIR-V words.
    size_t size;
    // The path to the metadata. Ending with an NUL terminater.
    RefRes<char> metadata_path;
    // Default vertex entry for this shader module.
    RefRes<char> vertex_entry;
    // Default tessellation entry for this shader module.
    RefRes<char> tessellation_entry;
    // Default geometry entry for this shader module.
    RefRes<char> geometry_entry;
    // Default fragment entry for this shader module.
    RefRes<char> fragment_entry;
    // Default compute entry for this shader module.
    RefRes<char> compute_entry;
};

/**
 * @class RUGraphicsPipeline
 * @brief Graphics pipeline.
 *
 */
struct RUGraphicsPipeline : public ResourceUnit {
    // The vertex shader.
    RefRes<RUShader> vertex_stage;
    // Entry function for the vertex stage.
    RefRes<char> vertex_stage_entry;
    // The tessellation shader.
    RefRes<RUShader> tessellation_stage;
    // Entry function for the tessellation stage.
    RefRes<char> tessellation_stage_entry;
    // The geometry shader.
    RefRes<RUShader> geometry_stage;
    // Entry function for the geometry stage.
    RefRes<char> geometry_stage_entry;
    // The fragment shader.
    RefRes<RUShader> fragment_stage;
    // Entry function for the fragment stage.
    RefRes<char> fragment_stage_entry;
    // Should enable depth test.
    bool depth_test = true;
    // Should enable depth write.
    bool depth_write = true;
    // Should cull back-facing triangles.
    bool cull_backfaces = true;
};

/**
 * @class RUMaterial
 * @brief Cook-Torrance material resource unit.
 */
struct RUMaterial : public ResourceUnit {
    // The graphics pipeline to run this material.
    RefRes<RUGraphicsPipeline> graphics_pipeline;
    /**
     * @brief The texture slot.
     */
    struct TextureSlot {
        // The key of the texture.
        RefRes<RUText> texture_key;
        // The texture instance.
        RefRes<RUImage> texture;
    };
    // The texture slots.
    RefRes<TextureSlot> textures;
    // The number of texture slots.
    size_t texture_count = 0;
};

/**
 * @class RUSubmesh3D
 * @brief 3D submesh resource unit.
 *
 * Push any chunk-owned geometry payload before emplacing this unit so the
 * stored references remain valid. Chunk teardown now runs registered
 * resource-unit destructors before releasing the allocator storage behind the
 * unit and its payload.
 */
struct RUSubmesh3D : public ResourceUnit {
    // A reference to the material.
    RefRes<RUMaterial> material;
    // The total number of triangles.
    size_t triangles_count;
    // The triangles instances.
    RefRes<Triangle3DIndxTextured> triangles;
};

/**
 * @class RUMesh3D
 * @brief 3D mesh resource unit.
 */
struct RUMesh3D : public ResourceUnit {
    // Number of submeshes.
    size_t submesh_count;
    // The submeshes.
    RefRes<RefRes<RUSubmesh3D>> submeshes;
};

} // namespace WhiteBirdEngine

#endif
