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
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/ext/vector_int2.hpp"
#include "glm/ext/vector_int3.hpp"
#include "glm/ext/vector_int4.hpp"
#include "glm/ext/vector_uint2.hpp"
#include "glm/ext/vector_uint3.hpp"
#include "glm/ext/vector_uint4.hpp"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <string_view>
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
 * @brief One material texture role declared inline or by resource id.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaMaterialTextureSlot {
    /** Non-empty shader-defined texture role. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string texture_role;
    /** Inline texture data. Exactly one of texture and texture_id must be present. */
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaImage> texture;
    /** Reference to an already populated texture. Exactly one of texture and texture_id must be present. */
    WBE_META(WBE_REFLECT)
    std::optional<std::string> texture_id;
};

/**
 * @class RUSchemaMaterial
 * @brief The scheme of RUMaterial.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemaMaterial : public RUSchema {
    WBE_DECL_SERIALIZABLE(RUSchemaMaterial)
    /** The graphics pipelines that can render this material. */
    WBE_META(WBE_REFLECT)
    std::vector<std::string> graphics_pipeline_ids;
    /** The inline graphics pipeline used to render this material. */
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaGraphicsPipeline> graphics_pipeline;
    /** Shader-defined texture slots used by the material. Texture roles must not be empty or duplicated. */
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
 * @class RUShaderRole
 * @brief Chunk-owned identity for an open shader-defined resource role.
 *
 * The name is retained for exact matching and diagnostics. The hash is an acceleration value and must always match the full
 * NUL-terminated name; consumers must not treat the hash alone as identity. This type is a non-owning resource-chunk view
 * and is copyable and movable with shallow chunk references.
 */
struct RUShaderRole {
    /**
     * @brief Construct an empty role for resource population.
     */
    RUShaderRole() = default;
    /**
     * @brief Destroy a non-owning role view.
     */
    ~RUShaderRole() = default;
    /**
     * @brief Copy a shallow role view.
     */
    RUShaderRole(const RUShaderRole&) = default;
    /**
     * @brief Move a shallow role view.
     */
    RUShaderRole(RUShaderRole&&) noexcept = default;
    /**
     * @brief Copy a shallow role view.
     */
    RUShaderRole& operator=(const RUShaderRole&) = default;
    /**
     * @brief Move a shallow role view.
     */
    RUShaderRole& operator=(RUShaderRole&&) noexcept = default;

    /**
     * @brief Construct a populated shallow role view; validation occurs when the role is accessed.
     */
    RUShaderRole(RefRes<char> p_name, HashCode p_hash) : name(p_name), hash(p_hash) {
    }

    /** Non-empty NUL-terminated role name. */
    RefRes<char> name;
    /** Runtime hash of name. */
    HashCode hash = 0;

    /**
     * @brief Return and validate the retained role name.
     * @throws std::runtime_error When the name is missing or empty, or its hash is inconsistent.
     */
    std::string_view get_name() const;
    /**
     * @brief Test a role using the hash and exact retained name.
     * @throws std::runtime_error When this role or the requested name is invalid.
     */
    bool matches(std::string_view p_name) const;
};

/**
 * @class RUMaterial
 * @brief Typed material resource unit.
 */
struct RUMaterial : public ResourceUnit {
    /** The graphics pipeline used to render this material. */
    RefRes<RUGraphicsPipeline> graphics_pipeline;
    /** One resolved texture-role slot. */
    struct TextureSlot {
        /** Exact shader-defined texture role. */
        RUShaderRole role;
        /** Resolved texture resource. */
        RefRes<RUImage> texture;
    };
    /** Chunk-owned contiguous texture-slot storage. */
    RefRes<TextureSlot> textures;
    /** Number of entries in textures. */
    size_t texture_count = 0;
};

/**
 * @enum RUVertexAttributeFormat
 * @brief Supported tightly packed 32-bit vertex attribute formats.
 */
enum class RUVertexAttributeFormat : uint8_t {
    FLOAT,
    FLOAT2,
    FLOAT3,
    FLOAT4,
    INT,
    INT2,
    INT3,
    INT4,
    UINT,
    UINT2,
    UINT3,
    UINT4,
};

/**
 * @class RUVertexData
 * @brief Chunk-owned mutable interleaved vertex data and its fixed layout.
 *
 * The layout is immutable after loading. Vertex bytes may be changed by the mutation methods, which are not thread-safe. A
 * renderer update is required before changes reach the GPU. The resource is a non-owning view into its resource chunk and is
 * copyable and movable with shallow chunk references.
 */
struct RUVertexData : public ResourceUnit {
    /**
     * @brief One attribute in declaration and packing order.
     */
    struct Attribute {
        /** Exact shader-defined vertex role. */
        RUShaderRole role;
        /** Exact 32-bit scalar or vector format. */
        RUVertexAttributeFormat format = RUVertexAttributeFormat::FLOAT;
        /** Byte offset inside one interleaved vertex. */
        size_t offset = 0;
        /** Byte size inside one interleaved vertex. */
        size_t size = 0;
    };

    /** Mutable tightly interleaved vertex bytes. */
    RefRes<uint8_t> vertices;
    /** Total byte size of vertices. */
    size_t vertices_size = 0;
    /** Ordered fixed-layout attributes. */
    RefRes<Attribute> attributes;
    /** Number of entries in attributes. */
    size_t attribute_count = 0;
    /** Byte stride between vertices. */
    size_t vertex_stride = 0;
    /** Number of interleaved vertices. */
    size_t vertex_count = 0;

    /**
     * @brief Swap two complete interleaved vertices.
     * @throws std::runtime_error When storage is missing or either vertex is out of bounds.
     */
    void swap_vertices(size_t p_first, size_t p_second);
    /**
     * @brief Return the byte offset of a role inside one vertex.
     * @throws std::runtime_error When the role is empty, duplicated, or absent, or layout storage is missing.
     */
    size_t get_vertex_attribute_offset(std::string_view p_role) const;
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
    /** Chunk-owned interleaved vertices and fixed layout. */
    RefRes<RUVertexData> vertex_data;
    /** Chunk-owned contiguous uint32 indices. */
    RefRes<uint32_t> indices;
    /** Number of entries in indices. */
    size_t index_count = 0;

    /**
     * @copydoc RUVertexData::swap_vertices
     */
    void swap_vertices(size_t p_first, size_t p_second);
    /** Set an exact float scalar attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, float p_value);
    /** Set an exact float2 attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, const glm::vec2& p_value);
    /** Set an exact float3 attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, const glm::vec3& p_value);
    /** Set an exact float4 attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, const glm::vec4& p_value);
    /** Set an exact signed 32-bit scalar attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, int32_t p_value);
    /** Set an exact signed 32-bit integer2 attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, const glm::ivec2& p_value);
    /** Set an exact signed 32-bit integer3 attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, const glm::ivec3& p_value);
    /** Set an exact signed 32-bit integer4 attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, const glm::ivec4& p_value);
    /** Set an exact unsigned 32-bit scalar attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, uint32_t p_value);
    /** Set an exact unsigned 32-bit integer2 attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, const glm::uvec2& p_value);
    /** Set an exact unsigned 32-bit integer3 attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, const glm::uvec3& p_value);
    /** Set an exact unsigned 32-bit integer4 attribute. */
    void set_vertex_attribute(size_t p_vertex_index, std::string_view p_role, const glm::uvec4& p_value);
    /**
     * @copydoc RUVertexData::get_vertex_attribute_offset
     */
    size_t get_vertex_attribute_offset(std::string_view p_role) const;
    /**
     * @brief Return the byte stride between vertices.
     */
    size_t get_vertex_stride() const;
    /**
     * @brief Return the number of vertices.
     */
    size_t get_vertices_num() const;
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
