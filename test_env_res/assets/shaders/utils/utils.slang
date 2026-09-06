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

/** Marks a vertex entry-point field and selects a resource attribute by role. */
[__AttributeUsage(_AttributeTargets.Var)]
struct WBE_VERTEX_INPUTAttribute {
    /** Vertex attribute role; an empty value selects the annotated field name. */
    string vertex_role = "";
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

/** Fixed bindless texture table contract. Applications may only vary the parameter's base variable name. */
struct MaterialTextures {
    /** Sampler shared by all material textures. */
    SamplerState material_sampler;
    /** Bindless texture array indexed through MaterialParams. */
    Texture2D<float4> textures[];
};

/** Fixed descriptor indices for one material's textures. */
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
struct MaterialParamsBuffer {
    /** Material entries indexed by the indirect draw's material index. */
    MaterialParams materials[WBE_MAX_MATERIAL_COUNT];
};

/** Per-submesh metadata for one static-set draw entry. */
struct SubmeshData {
    /** Material table entry used by this submesh. */
    uint material_id;
    uint padding_0;
    uint padding_1;
    uint padding_2;
};

/** Per-node transform payload referenced by static-set instances. */
struct NodeData {
    /** Engine-space column-major global transform. */
    float4x4 transform;
};

/** Per-drawn-instance metadata linking one submesh to one node transform. */
struct InstanceData {
    /** Index into the static_set.submeshes storage buffer. */
    uint submesh_index;
    /** Index into the static_set.nodes storage buffer. */
    uint node_index;
    uint padding_0;
    uint padding_1;
};

/**
 * @brief Static geometry storage buffers matching the renderer's static_set binding contract.
 */
struct StaticSet {
    /**
     * @brief Material table identifiers indexed by InstanceData.submesh_index.
     */
    StructuredBuffer<SubmeshData> submeshes;
    /**
     * @brief Object-to-world transforms indexed by InstanceData.node_index.
     */
    StructuredBuffer<NodeData> nodes;
    /**
     * @brief Draw instances indexed by SV_VulkanInstanceID, including the indirect firstInstance offset.
     */
    StructuredBuffer<InstanceData> instances;
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

/**
 * @brief Transform an object-space normal into world space for static geometry.
 * @param p_transform Object-to-world transform.
 * @param p_normal Object-space normal.
 * @return Unit-length world-space normal.
 */
float3x3 inverse_3x3(float3x3 p_matrix) {
    float3 row0 = p_matrix[0];
    float3 row1 = p_matrix[1];
    float3 row2 = p_matrix[2];
    float3 cofactor0 = cross(row1, row2);
    float3 cofactor1 = cross(row2, row0);
    float3 cofactor2 = cross(row0, row1);
    float determinant = dot(row0, cofactor0);
    if (abs(determinant) < 0.000001) {
        return float3x3(1.0);
    }
    return transpose(float3x3(cofactor0, cofactor1, cofactor2)) / determinant;
}

float3 transform_static_normal(float4x4 p_transform, float3 p_normal) {
    float3x3 normal_transform = transpose(inverse_3x3((float3x3)p_transform));
    return normalize(mul(normal_transform, p_normal));
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
