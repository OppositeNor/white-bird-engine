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
#include <string>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class RUSchemeShader
 * @brief The scheme of RUShader.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeShader : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeShader)
    // The path to the shader file.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string path;
    // The path to the shader's metadata file.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string metadata;
};

/**
 * @class RUSchemeGraphicsPipeline
 * @brief The scheme of RUGraphicsPipeline.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeGraphicsPipeline : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeGraphicsPipeline)
    // The resource ID of the vertex shader.
    WBE_META(WBE_REFLECT)
    std::string vertex_stage_id;
    // Entry function for the vertex stage.
    WBE_META(WBE_REFLECT)
    std::string vertex_stage_entry = "main";
    // The vertex shader of the pipeline.
    WBE_META(WBE_REFLECT)
    RUSchemeShader vertex_stage;
    // The resource ID of the tessellation shader.
    WBE_META(WBE_REFLECT)
    std::string tessellation_stage_id;
    // Entry function for the tessellation stage.
    WBE_META(WBE_REFLECT)
    std::string tessellation_stage_entry = "main";
    // The tessellation shader of the pipeline.
    WBE_META(WBE_REFLECT)
    RUSchemeShader tessellation_stage;
    // The resource ID of the geometry shader.
    WBE_META(WBE_REFLECT)
    std::string geometry_stage_id;
    // Entry function for the geometry stage.
    WBE_META(WBE_REFLECT)
    std::string geometry_stage_entry = "main";
    // The geometry shader of the pipeline.
    WBE_META(WBE_REFLECT)
    RUSchemeShader geometry_stage;
    // The resource ID of the fragment shader.
    WBE_META(WBE_REFLECT)
    std::string fragment_stage_id;
    // Entry function for the fragment stage.
    WBE_META(WBE_REFLECT)
    std::string fragment_stage_entry = "main";
    // The fragment shader of the pipeline.
    WBE_META(WBE_REFLECT)
    RUSchemeShader fragment_stage;
    // Should enable depth test.
    WBE_META(WBE_REFLECT)
    bool depth_test = true;
    // Should enable depth write.
    WBE_META(WBE_REFLECT)
    bool depth_write = true;
};

/**
 * @class RUSchemeMaterialTextureSlot
 * @brief The scheme of one texture slot in RUMaterial.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemeMaterialTextureSlot {
    // The key used by the material shader to find this texture.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string texture_key;
    // Inline texture data.
    WBE_META(WBE_REFLECT)
    RUSchemeImage texture;
    // A string referencing an already populated texture.
    WBE_META(WBE_REFLECT)
    std::string texture_id;
};

/**
 * @class RUSchemeMaterial
 * @brief The scheme of RUMaterial.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeMaterial : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeMaterial)
    // The graphics pipeline used to render this material.
    WBE_META(WBE_REFLECT)
    std::string graphics_pipeline_id;
    // The inline graphics pipeline used to render this material.
    WBE_META(WBE_REFLECT)
    RUSchemeGraphicsPipeline graphics_pipeline;
    // The texture slots used by this material.
    WBE_META(WBE_REFLECT)
    std::vector<RUSchemeMaterialTextureSlot> textures;
};

/**
 * @class RUSchemeSubmesh3D
 * @brief The scheme of RUSubmesh3D.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeSubmesh3D : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeSubmesh3D)
    // The material if it could be populated in place.
    WBE_META(WBE_REFLECT)
    RUSchemeMaterial material;
    // A string referencing unpopulated material.
    WBE_META(WBE_REFLECT)
    std::string material_id;
    // The vertices data.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::vector<Vertex3DTextured> vertices_data;
    // The indices data.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::vector<uint32_t> indices_data;
};

/**
 * @class RUSchemeMesh3D
 * @brief The scheme of RUMesh3D.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeMesh3D : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeMesh3D)
    // Submeshes that could be populated in place.
    WBE_META(WBE_REFLECT)
    std::vector<RUSchemeSubmesh3D> submeshes;
    // Submesh ids referencing unpopulated submeshes.
    WBE_META(WBE_REFLECT)
    std::vector<std::string> submeshes_id;
};

/**
 * @class RUShader
 * @brief Shader resource unit. contains a size (in bytes) and a MemID pointing
 * to the data chunk that stores the data of the binary data.
 *
 * The data should always be pushed before pushing this resource unit, and the
 * release process should always be backwards (first release the resource unit,
 * and then release the data) It is recommended to store the image data right
 * before the text resource unit in a stack allocator.
 *
 */
struct RUShader : public ResourceUnit {
    // The shader source.
    RefRes<uint32_t> data;
    // The size of the binary data.
    size_t size;
    // The path to the metadata. Ending with an NUL terminater.
    RefRes<char> metadata_path;
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
 * The data should always be pushed before pushing this resource unit, and the
 * release process should always be backwards (first release the resource unit,
 * and then release the data) It is recommended to store the image data right
 * before the text resource unit in a stack allocator.
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
