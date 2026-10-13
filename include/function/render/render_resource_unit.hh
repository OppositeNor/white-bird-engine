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
#include "glm/ext/matrix_float4x4.hpp"
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
    /**
     * @brief Whether the pipeline overestimates primitive coverage using conservative rasterization.
     */
    WBE_META(WBE_REFLECT)
    bool conservative_rasterization = false;
};

/**
 * @class RUSchemaRayTracingPipeline
 * @brief The scheme of RURayTracingPipeline.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemaRayTracingPipeline : public RUSchema {
    WBE_DECL_SERIALIZABLE(RUSchemaRayTracingPipeline)
    // The resource ID of the ray-generation shader.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> raygen_stage_id;
    // Entry function for the ray-generation stage.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> raygen_stage_entry;
    // The ray-generation shader of the pipeline.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaShader> raygen_stage;
    // The resource ID of the miss shader.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> miss_stage_id;
    // Entry function for the miss stage.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> miss_stage_entry;
    // The miss shader of the pipeline.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaShader> miss_stage;
    // The resource ID of the closest-hit shader.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> closest_hit_stage_id;
    // Entry function for the closest-hit stage.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> closest_hit_stage_entry;
    // The closest-hit shader of the pipeline.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaShader> closest_hit_stage;
    // The resource ID of the any-hit shader.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> any_hit_stage_id;
    // Entry function for the any-hit stage.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> any_hit_stage_entry;
    // The any-hit shader of the pipeline.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaShader> any_hit_stage;
    // The resource ID of the intersection shader.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> intersection_shader_stage_id;
    // Entry function for the intersection-shader stage.
    WBE_META(WBE_REFLECT)
    std::optional<std::string> intersection_shader_stage_entry;
    // The intersection shader of the pipeline.
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaShader> intersection_shader_stage;
};

/**
 * @class RUSchemaMaterialTextureSlot
 * @brief One material texture role declared inline or by resource id.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaMaterialTextureSlot {
    /** Non-empty shader-defined texture role. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string texture_role;
    /** Inline KTX 2 texture schema. Exactly one of texture and texture_id must be present. */
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaTexture> texture;
    /** Resource ID of an already populated texture. Exactly one of texture and texture_id must be present. */
    WBE_META(WBE_REFLECT)
    std::optional<std::string> texture_id;
};

/**
 * @class RUSchemaMaterial
 * @brief The scheme of RUMaterial.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemaMaterial : public RUSchema {
    WBE_DECL_SERIALIZABLE(RUSchemaMaterial)
    /** Shader-defined texture slots used by the material. Texture roles must not be empty or duplicated. */
    WBE_META(WBE_REFLECT)
    std::vector<RUSchemaMaterialTextureSlot> textures;
};

/**
 * @class RUSchemaBinaryView
 * @brief Byte range view into a binary resource.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaBinaryView {
    /** Resource id of the binary backing this view. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string binary_id;
    /** Byte offset from the beginning of the binary. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    size_t start = 0;
    /** Byte length of this view. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    size_t size = 0;
};

/**
 * @class RUSchemaVertexAttribute
 * @brief One vertex attribute inside a binary-backed interleaved vertex view.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaVertexAttribute {
    /** Exact shader-defined vertex role. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string role;
    /** Exact 32-bit scalar or vector attribute type. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string type;
    /** Byte offset inside one interleaved vertex. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    size_t offset = 0;
};

/**
 * @class RUSchemaVertexView
 * @brief Binary-backed interleaved vertex view.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaVertexView {
    /** Backing binary byte range. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    RUSchemaBinaryView binary;
    /** Byte stride between vertices. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    size_t stride = 0;
    /** Ordered vertex attributes. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::vector<RUSchemaVertexAttribute> attributes;
};

/**
 * @class RUSchemaIndexView
 * @brief Binary-backed uint32 index view.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaIndexView {
    /** Backing binary byte range. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    RUSchemaBinaryView binary;
};

/**
 * @class RUSchemaAABB3D
 * @brief The scheme of RUAABB3D.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaAABB3D {
    /** Box center as exactly three floats `[x, y, z]`. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::vector<float> center{0.0F, 0.0F, 0.0F};
    /** Full X extent. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    float width = 0.0F;
    /** Full Y extent. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    float height = 0.0F;
};

/**
 * @class RUSchemaSubmesh3D
 * @brief The scheme of RUSubmesh3D.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaSubmesh3D {
    /** Inline material for this submesh. */
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaMaterial> material;
    /** Referenced material for this submesh. */
    WBE_META(WBE_REFLECT)
    std::optional<std::string> material_id;
    /** Axis-aligned bounds of the submesh vertex positions. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    RUSchemaAABB3D aabb;
    /** Binary-backed vertex data. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    RUSchemaVertexView vertices;
    /** Binary-backed uint32 indices. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    RUSchemaIndexView indices;
    /**
     * @brief Binary-backed per-vertex skeleton joint bindings.
     *
     * Requires the mesh to reference a skeleton. The view must hold one entry per vertex and declare the `ivec4`
     * `joint_binding_ids` and `vec4` `joint_binding_weights` attributes.
     */
    WBE_META(WBE_REFLECT)
    std::optional<RUSchemaVertexView> joint_bindings;
    /** First transform entry used by this submesh. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    size_t first_instance = 0;
    /** Number of transform entries used by this submesh. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    size_t instance_count = 0;
};

/**
 * @class RUSchemaInstance3D
 * @brief The scheme of RUInstance3D.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaInstance3D {
    /** Column-major global transform. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::mat4 global_transform{1.0F};
};

/**
 * @class RUSchemaSubmeshInstance3D
 * @brief The scheme of RUSubmeshInstance3D.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC) RUSchemaSubmeshInstance3D {
    /** Submeshes in declared draw order. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::vector<RUSchemaSubmesh3D> submeshes;
    /** Instance transforms shared by the submeshes. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::vector<RUSchemaInstance3D> instances;
};

/**
 * @class RUSchemaMesh3D
 * @brief The scheme of RUMesh3D.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemaMesh3D : public RUSchema {
    WBE_DECL_SERIALIZABLE(RUSchemaMesh3D)
    /** Opaque submeshes drawn with face culling. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    RUSchemaSubmeshInstance3D opaque_instances;
    /** Alpha-tested submeshes. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    RUSchemaSubmeshInstance3D masked_instances;
    /** Opaque submeshes drawn without face culling. */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    RUSchemaSubmeshInstance3D double_sided_opaque_instances;
    /** Resource ID of the skeleton the submesh joint bindings index. */
    WBE_META(WBE_REFLECT)
    std::optional<std::string> skeleton_id;
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
    /**
     * @brief Whether the pipeline overestimates primitive coverage using conservative rasterization.
     */
    bool conservative_rasterization = false;
};

/**
 * @class RURayTracingPipeline
 * @brief Ray-tracing pipeline resource with one fixed slot for each supported stage.
 */
struct RURayTracingPipeline : public ResourceUnit {
    // The ray-generation shader.
    RefRes<RUShader> raygen_stage;
    // Entry function for the ray-generation stage.
    RefRes<char> raygen_stage_entry;
    // The miss shader.
    RefRes<RUShader> miss_stage;
    // Entry function for the miss stage.
    RefRes<char> miss_stage_entry;
    // The closest-hit shader.
    RefRes<RUShader> closest_hit_stage;
    // Entry function for the closest-hit stage.
    RefRes<char> closest_hit_stage_entry;
    // The any-hit shader.
    RefRes<RUShader> any_hit_stage;
    // Entry function for the any-hit stage.
    RefRes<char> any_hit_stage_entry;
    // The intersection shader.
    RefRes<RUShader> intersection_shader_stage;
    // Entry function for the intersection-shader stage.
    RefRes<char> intersection_shader_stage_entry;
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
    /** One resolved texture-role slot. */
    struct TextureSlot {
        /** Exact shader-defined texture role. */
        RUShaderRole role;
        /** Resolved texture resource. */
        RefRes<RUTexture> texture;
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
 * @class RUBinaryView
 * @brief Non-owning byte range view into a chunk-owned binary resource.
 */
struct RUBinaryView {
    /** Resolved backing binary. */
    RefRes<RUBinary> binary;
    /** Byte offset from the beginning of the binary. */
    size_t start = 0;
    /** Byte length of this view. */
    size_t size = 0;
};

/**
 * @class RUVertexView
 * @brief Binary-backed mutable interleaved vertex data and its fixed layout.
 *
 * The layout is immutable after loading. Vertex bytes may be changed by the mutation methods, which are not thread-safe. A
 * renderer update is required before changes reach the GPU. The resource is a non-owning view into its resource chunk and is
 * copyable and movable with shallow chunk references.
 */
struct RUVertexView {
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
    RUBinaryView binary;
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
 * @class RUIndexView
 * @brief Binary-backed uint32 index view.
 */
struct RUIndexView {
    /** Backing byte range containing uint32 indices. */
    RUBinaryView binary;
    /** Number of entries in indices. */
    size_t index_count = 0;
};

/**
 * @class RUAABB3D
 * @brief Axis-aligned bounds of a submesh's vertex positions, in the same space as its vertices.
 *
 * Bounds describe the geometry before the submesh's instance transforms are applied.
 */
struct RUAABB3D {
    /** Box center. */
    glm::vec3 center{0.0F};
    /** Full X extent. */
    float width = 0.0F;
    /** Full Y extent. */
    float height = 0.0F;
};

/**
 * @class RUSubmesh3D
 * @brief One material, geometry range, and instance range of a mesh category.
 *
 * A non-owning view into its resource chunk; copyable and movable with shallow chunk references. Vertex mutation is not
 * thread-safe and requires recompiling the mesh's render resources to reach the GPU.
 */
struct RUSubmesh3D {
    /** Referenced material. */
    RefRes<RUMaterial> material;
    /** Axis-aligned bounds of the vertex positions. */
    RUAABB3D aabb;
    /** Binary-backed interleaved vertices and fixed layout. */
    RUVertexView vertices;
    /** Binary-backed uint32 indices. */
    RUIndexView indices;
    /**
     * @brief Per-vertex skeleton joint bindings, present only for meshes that reference a skeleton.
     *
     * Each entry stores the `ivec4` `joint_binding_ids` indexing the mesh skeleton joints and the matching `vec4`
     * `joint_binding_weights`. The view has the same vertex count as vertices.
     */
    std::optional<RUVertexView> joint_bindings;
    /** First transform entry used by this submesh. */
    size_t first_instance = 0;
    /** Number of transform entries used by this submesh. */
    size_t instance_count = 0;

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
 * @class RUInstance3D
 * @brief One instance transform of a mesh category.
 */
struct RUInstance3D {
    /** Column-major global transform. */
    glm::mat4 global_transform{1.0F};
};

/**
 * @class RUSubmeshInstance3D
 * @brief Submeshes of one mesh category and the instance transforms their instance ranges index.
 */
struct RUSubmeshInstance3D {
    /** Submeshes in declared draw order. */
    RefRes<RUSubmesh3D> submeshes;
    /** Number of entries in submeshes. */
    size_t submesh_count = 0;
    /** Instance transforms shared by the submeshes. */
    RefRes<RUInstance3D> instances;
    /** Number of entries in instances. */
    size_t instance_count = 0;
};

/**
 * @class RUMesh3D
 * @brief 3D mesh resource unit: opaque, masked, and double-sided opaque submesh categories.
 *
 * Static and dynamic meshes share this representation; the categories may share vertex and index binaries. The payload
 * lifetime belongs to the resource chunk.
 */
struct RUMesh3D : public ResourceUnit {
    /** Opaque submeshes drawn with face culling. */
    RUSubmeshInstance3D opaque_instances;
    /** Alpha-tested submeshes. */
    RUSubmeshInstance3D masked_instances;
    /** Opaque submeshes drawn without face culling. */
    RUSubmeshInstance3D double_sided_opaque_instances;
    /** Referenced skeleton, or null when the mesh is not skinned. */
    RefRes<RUSkeleton> skeleton;
};

} // namespace WhiteBirdEngine

#endif
