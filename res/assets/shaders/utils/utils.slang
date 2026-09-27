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
#ifndef WBE_FILE_UTILS_SLANG
#define WBE_FILE_UTILS_SLANG

/**
 * @brief Reports an error when the condition is false, only in shader-debug builds.
 * @details Disabled builds do not evaluate any argument or guard the following operation.
 * @param p_condition Required condition, evaluated once when shader debugging is enabled.
 * @param p_error Stable shader error code.
 * @param p_frame_slot Renderer frame slot receiving the error.
 */
#ifdef WBE_SHADER_DEBUG
#define WBE_SHADER_DEBUG_ASSERT(p_condition, p_error, p_frame_slot) \
    do { \
        if (!(p_condition)) { \
            report_shader_debug_error(p_error, p_frame_slot); \
        } \
    } while (0)
#else
#define WBE_SHADER_DEBUG_ASSERT(p_condition, p_error, p_frame_slot)
#endif

/** Stable error codes shared with the CPU shader-debug reader. */
enum class ShaderDebugError {
    DEBUG_BUFFER_FULL = 1,
    OCT_TREE_NODE_INDEX_OVERFLOW = 2,
    OCT_TREE_LEAF_INDEX_OVERFLOW = 3,
    OCT_TREE_DEFERRED_TASK_INDEX_OVERFLOW = 4,
    OCT_TREE_DEEPER_THAN_EXPECTED = 5,
    OCT_TREE_SHALLOWER_THAN_EXPECTED = 6,
};

/** Shader-visible buffer descriptor reserved by the renderer. */
static const uint WBE_SHADER_DEBUG_DESCRIPTOR_INDEX = 0;

#ifdef WBE_SHADER_DEBUG
/**
 * @brief Atomically append one error to the renderer-owned shader-debug buffer.
 * @param p_error Error code to append.
 * @param p_frame_slot Renderer frame slot whose independent report region receives the error.
 */
void report_shader_debug_error(ShaderDebugError p_error, uint p_frame_slot) {
    RWStructuredBuffer<uint> debug_buffer = ResourceDescriptorHeap[WBE_SHADER_DEBUG_DESCRIPTOR_INDEX];
    static const uint FRAME_ENTRY_CAPACITY = 512;
    static const uint FRAME_WORD_COUNT = FRAME_ENTRY_CAPACITY + 2;
    uint frame_offset = p_frame_slot * FRAME_WORD_COUNT;
    uint capacity = debug_buffer[frame_offset + 1];
    uint entry_index;
    InterlockedAdd(debug_buffer[frame_offset], 1, entry_index);

    // Reserve the last physical entry for an unambiguous overflow marker. No normal writer can race with this store.
    if (capacity > 0 && entry_index < capacity - 1) {
        debug_buffer[frame_offset + entry_index + 2] = uint(p_error);
        return;
    }
    if (capacity > 0) {
        InterlockedMin(debug_buffer[frame_offset], capacity);
        InterlockedExchange(debug_buffer[frame_offset + capacity + 1], uint(ShaderDebugError::DEBUG_BUFFER_FULL));
    }
}

#endif

/** Marks a vertex entry-point field and selects a resource attribute by role. */
[__AttributeUsage(_AttributeTargets.Var)]
struct WBE_VERTEX_INPUTAttribute {
    /** Vertex attribute role; an empty value selects the annotated field name. */
    string vertex_role = "";
};

/** Marks a push-data uint2 as an index into one descriptor-heap arena. */
[__AttributeUsage(_AttributeTargets.Var)]
struct WBE_DESCRIPTORAttribute {
    /** Descriptor category interpreted by the Vulkan pipeline compiler. */
    string descriptor_type;
    /** Logical render-graph resource identifier resolved by the owning task. */
    string resource_id;
};

/** Marks a push-data scalar whose value is supplied automatically by the renderer. */
[__AttributeUsage(_AttributeTargets.Var)]
struct WBE_DEFAULT_PUSHAttribute {
    /** Renderer default value identifier. */
    string value;
};

/** Marks a struct as an engine-discoverable data type in reflection metadata. */
[__AttributeUsage(_AttributeTargets.Struct)]
struct WBE_DATA_TYPEAttribute {
    /** Optional logical type name; an empty value means to use the struct name. */
    string type_name = "";
};

/**
 * @brief The input data from the vertex buffer
 */
struct Vertex3DTexturedInput {
    /**
     * @brief The position of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 position;
    /**
     * @brief The normal of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 normal;
    /**
     * @brief The tanglent vector of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 tangent;
    /**
     * @brief The bitangent vector of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 bitangent;
    /**
     * @brief The uv value.
     */
    [WBE_VERTEX_INPUT]
    float2 uv;
};

/**
 * @brief The information of the camera.
 */
struct CameraInfo {
    /**
     * @brief The view matrix.
     */
    float4x4 view;
    /**
     * @brief The projection matrix.
     */
    float4x4 proj;
    /**
     * @brief The world-space camera position.
     */
    float4 world_position;
};

/**
 * @brief Multiply a vector by matrix rows loaded individually from descriptor-heap memory.
 * This is a temporary work around for a bug of the NVIDIA driver when loading matrix rows.
 * @param p_row_0 First matrix row.
 * @param p_row_1 Second matrix row.
 * @param p_row_2 Third matrix row.
 * @param p_row_3 Fourth matrix row.
 * @param p_vector Vector to transform.
 * @return The matrix-vector product.
 */
float4 software_mul_float4x4_vector(
    float4 p_row_0, float4 p_row_1, float4 p_row_2, float4 p_row_3, float4 p_vector) {
    return float4(dot(p_row_0, p_vector), dot(p_row_1, p_vector), dot(p_row_2, p_vector), dot(p_row_3, p_vector));
}

// Temporary workaround for whole matrix loads ignoring MatrixStride through SPV_EXT_descriptor_heap.
#define WBE_SOFTWARE_MATRIX_ROWS(p_matrix)                                                                                 \
    float4((p_matrix)[0][0], (p_matrix)[1][0], (p_matrix)[2][0], (p_matrix)[3][0]),                                      \
        float4((p_matrix)[0][1], (p_matrix)[1][1], (p_matrix)[2][1], (p_matrix)[3][1]),                                  \
        float4((p_matrix)[0][2], (p_matrix)[1][2], (p_matrix)[2][2], (p_matrix)[3][2]),                                  \
        float4((p_matrix)[0][3], (p_matrix)[1][3], (p_matrix)[2][3], (p_matrix)[3][3])

/**
 * @brief Time information.
 */
struct Time {
    /**
     * @brief The duration from the start of the application until now.
     */
    float duration_sec;
};

/** Maximum number of bindless material entries addressable by one draw pass. */
static const uint WBE_MAX_MATERIAL_COUNT = 32;

/** Marks an integer material parameter as the descriptor index for one texture role. */
[__AttributeUsage(_AttributeTargets.Var)]
struct WBE_TEXTUREAttribute {
    /** Texture role; an empty value selects the annotated variable name. */
    string texture_role = "";
};

/** Descriptor-heap indices shared by material shaders. */
struct MaterialDescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer", "material_params")]
    uint2 material_params;
    [WBE_DESCRIPTOR("sampler", "material_sampler")]
    uint2 material_sampler;
};

#ifdef WBE_REFLECTION_METADATA
/** Reflection-only view of the material texture contract. */
[WBE_DATA_TYPE]
struct MaterialTextureContractMetadata {
    SamplerState material_sampler;
    Texture2D<float4> textures[];
};
#endif

/** Fixed descriptor indices for one material's textures. */
[WBE_DATA_TYPE]
struct MaterialParams {
    /** Albedo texture descriptor index. */
    [WBE_TEXTURE("albedo")]
    int albedo_index;
    /** Normal texture descriptor index. */
    [WBE_TEXTURE("normal")]
    int normal_index;
    /** RMA texture descriptor index. */
    [WBE_TEXTURE("rma")]
    int rma_index;
};

/** Fixed descriptor-index table for every material addressed by an indirect draw. */
[WBE_DATA_TYPE]
struct MaterialParamsBuffer {
    /** Material entries indexed by the indirect draw's material index. */
    MaterialParams materials[WBE_MAX_MATERIAL_COUNT];
};

/** Per-submesh metadata for one static-set draw entry. */
[WBE_DATA_TYPE]
struct SubmeshData {
    /** Material table entry used by this submesh. */
    uint material_id;
    uint padding_0;
    uint padding_1;
    uint padding_2;
};

/** Per-node transform payload referenced by static-set instances. */
[WBE_DATA_TYPE]
struct NodeData {
    /** Engine-space column-major global transform. */
    float4x4 transform;
    /** Precomputed inverse-transpose used for static normal transforms. */
    float4x4 normal_transform;
};

/** Per-drawn-instance metadata linking one submesh to one node transform. */
[WBE_DATA_TYPE]
struct InstanceData {
    /** Index into the static_set.submeshes storage buffer. */
    uint submesh_index;
    /** Index into the static_set.nodes storage buffer. */
    uint node_index;
    uint padding_0;
    uint padding_1;
};

#ifdef WBE_REFLECTION_METADATA
/** Reflection-only view of the static-set storage-buffer contract. */
[WBE_DATA_TYPE]
struct StaticSetContractMetadata {
    StructuredBuffer<SubmeshData> submeshes;
    StructuredBuffer<NodeData> nodes;
    StructuredBuffer<InstanceData> instances;
};
#endif

/**
 * @brief Static geometry storage buffers matching the renderer's static_set binding contract.
 */
struct StaticSetDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "static_set_submeshes")]
    uint2 submeshes;
    [WBE_DESCRIPTOR("storage_buffer", "static_set_nodes")]
    uint2 nodes;
    [WBE_DESCRIPTOR("storage_buffer", "static_set_instances")]
    uint2 instances;
};

/**
 * @brief Mathematical constant pi.
 */
static const float WBE_PI = 3.14159265359;

/**
 * @brief Decode a two-channel tangent-space normal map sample and reconstruct its positive Z component.
 * @param p_encoded_normal Encoded tangent-space X and Y components in [0, 1].
 * @return The decoded tangent-space normal on the positive-Z hemisphere.
 */
float3 decode_normal_map(float2 p_encoded_normal) {
    float2 normal_xy = p_encoded_normal * 2.0 - 1.0;
    float normal_z = sqrt(saturate(1.0 - dot(normal_xy, normal_xy)));
    return float3(normal_xy, normal_z);
}

float3 transform_static_normal(float4x4 p_normal_transform, float3 p_normal) {
    return normalize(mul((float3x3)p_normal_transform, p_normal));
}

/**
 * @brief Compute the Chebyshev upper bound for a value from its first two moments.
 * @param p_moments The distribution mean and second moment.
 * @param p_value The value whose probability bound is requested.
 * @param p_minimum_variance The lower variance bound used for numerical stability.
 * @return The probability upper bound clamped to [0, 1].
 */
float chebyshev_upper_bound(float2 p_moments, float p_value, float p_minimum_variance) {
    // var(x) = E[x^2] - E[x]^2.
    float variance = max(p_moments.y - p_moments.x * p_moments.x, p_minimum_variance);
    float value_delta = p_value - p_moments.x;
    // p_max(t) = var(x) / (var(x) + (t - E[x])^2).
    return saturate(variance / (variance + value_delta * value_delta));
}

// -----------------------------------------------------------------------------
// Octahedron normal encoding.
//
// Encodes a unit vector in R^3 into a 2D square in [-1, 1]. Suitable for
// storing into an R16G16_SNORM (or higher precision) render target as a
// compact normal representation for G-buffers and similar use cases.
// -----------------------------------------------------------------------------

/**
 * @brief Helper used by ::octahedron_encode to fold the lower hemisphere onto the unit square.
 * @param p_v The xy components of a normalized direction projected onto the octahedron.
 * @return The folded xy coordinates clamped to the [-1, 1] square.
 */
float2 octahedron_wrap(float2 p_v) {
    return (1.0 - abs(p_v.yx)) * (float2(p_v.x >= 0.0 ? 1.0 : -1.0, p_v.y >= 0.0 ? 1.0 : -1.0));
}

/**
 * @brief Encode a unit-length direction vector into a 2-component value in [-1, 1] using octahedron mapping.
 * @param p_n The unit-length direction to encode.
 * @return The encoded 2D coordinate, suitable for storage in an R16G16_SNORM (or higher precision) target.
 */
float2 octahedron_encode(float3 p_n) {
    float3 n = p_n / max(abs(p_n.x) + abs(p_n.y) + abs(p_n.z), 0.00001);
    float2 r = n.z >= 0.0 ? n.xy : octahedron_wrap(n.xy);
    return clamp(r, -1.0, 1.0);
}

/**
 * @brief Decode an octahedron-mapped 2D coordinate back into a unit-length direction vector.
 * @param p_f The encoded 2D coordinate, as produced by ::octahedron_encode.
 * @return The decoded unit-length direction.
 */
float3 octahedron_decode(float2 p_f) {
    float3 n = float3(p_f.x, p_f.y, 1.0 - abs(p_f.x) - abs(p_f.y));
    float t = saturate(-n.z);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

// -----------------------------------------------------------------------------
// Color space helpers.
// -----------------------------------------------------------------------------

/**
 * @brief Convert a linear-space color to sRGB using the approximate gamma 2.2 curve.
 * @param p_color The linear-space color (non-negative components).
 * @return The sRGB-encoded color.
 */
float3 linear_to_srgb(float3 p_color) {
    return pow(max(p_color, float3(0.0)), float3(1.0 / 2.2));
}

/**
 * @brief Convert an sRGB-encoded color to linear space using the approximate gamma 2.2 curve.
 * @param p_color The sRGB-encoded color (non-negative components).
 * @return The linear-space color.
 */
float3 srgb_to_linear(float3 p_color) {
    return pow(max(p_color, float3(0.0)), float3(2.2));
}

uint2 z_order_swizzle_8x8(uint2 p_group_id, uint2 p_group_thread_id) {
    uint linear_index = p_group_thread_id.y * 8 + p_group_thread_id.x;
    uint swizzled_x = ((linear_index >> 0) & 0x1) | ((linear_index >> 1) & 0x2) | ((linear_index >> 2) & 0x4);
    uint swizzled_y = ((linear_index >> 1) & 0x1) | ((linear_index >> 2) & 0x2) | ((linear_index >> 3) & 0x4);
    return p_group_id * 8 + uint2(swizzled_x, swizzled_y);
}

float random(float2 p_seed) {
    return frac(sin(dot(p_seed, float2(12.9898, 78.233))) * 43758.5453123);
}

#endif
