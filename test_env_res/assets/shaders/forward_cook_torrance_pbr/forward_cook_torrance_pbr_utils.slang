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
#ifndef WBE_FILE_FORWARD_COOK_TORRANCE_PBR_UTILS_SLANG
#define WBE_FILE_FORWARD_COOK_TORRANCE_PBR_UTILS_SLANG

#include "utils.slang"

static const uint WBE_MAX_SPOT_LIGHT_COUNT = 4;
static const uint WBE_MAX_MATERIAL_COUNT = 32;

/**
 * @brief Vertex shader output / fragment shader input for the forward Cook-Torrance PBR pass.
 */
struct VSOutput {
    /**
 * @brief Clip-space position.
     */
    float4 position         : SV_Position;
    /**
 * @brief World-space position in xyz. The w component stores the draw instance id used as the material table location.
     */
    float4 world_position   : TEXCOORD0;
    /**
 * @brief Object-space normal interpolated from the vertex attribute.
     */
    float3 normal           : TEXCOORD1;
    /**
 * @brief Object-space tangent interpolated from the vertex attribute.
     */
    float3 tangent          : TEXCOORD2;
    /**
 * @brief Object-space bitangent interpolated from the vertex attribute.
     */
    float3 bitangent        : TEXCOORD3;
    /**
 * @brief Texture coordinate interpolated from the vertex attribute.
     */
    float2 uv               : TEXCOORD4;
    /**
 * @brief Position of the fragment in the spot light's clip space, used to sample the shadow map.
     */
    float4 light_space_pos  : TEXCOORD5;
};

/**
 * @brief Spot light parameters consumed by the forward Cook-Torrance PBR pass. Cone angles are stored as cosines for cheap evaluation.
 */
struct SpotLightInfo {
    /// World-to-light view matrix used to sample the shadow map.
    float4x4 view;
    /// Light-space projection matrix used to sample the shadow map.
    float4x4 proj;
    /// World-space position of the spot light.
    float3 position;
    /// Scalar intensity multiplier.
    float intensity;
    /// World-space direction the spot is pointing (unit vector).
    float3 direction;
    /// Cosine of the inner cone half-angle (full intensity inside).
    float inner_cone_cos;
    /// Linear-space color of the spot light.
    float3 color;
    /// Cosine of the outer cone half-angle (zero intensity outside).
    float outer_cone_cos;
    /// Maximum effective light distance.
    float range;
};

/**
 * @brief Spot light uniform payload consumed by the forward Cook-Torrance PBR pass.
 */
struct SpotLightInfoBuffer {
    /// Active spot lights. Entries beyond spot_light_count are ignored.
    SpotLightInfo spot_light_info[WBE_MAX_SPOT_LIGHT_COUNT];
    /// Number of active spot light entries in spot_light_info.
    uint spot_light_count;
};

struct MaterialTextures {
    SamplerState material_sampler;
    Texture2D<float4> textures[];
};

struct MaterialParams {
    int albedo_index;
    int normal_index;
    int roughness_metallic_ao_index;
};

struct MaterialParamsBuffer {
    MaterialParams materials[WBE_MAX_MATERIAL_COUNT];
};

struct LightParam {
    ConstantBuffer<SpotLightInfo> info;
    Texture2D<float> shadowmap;
    SamplerState shadowmap_sampler;
};

struct LightParams {
    LightParam lights[WBE_MAX_SPOT_LIGHT_COUNT];
    ConstantBuffer<uint> light_count;
};

/**
 * @brief GGX/Trowbridge-Reitz normal distribution function.
 * @param p_normal The surface normal (unit vector).
 * @param p_half_vector The half vector between view and light directions.
 * @param p_roughness Linear roughness in [0, 1].
 * @return The microfacet normal distribution term.
 */
float distribution_ggx(float3 p_normal, float3 p_half_vector, float p_roughness) {
    float alpha = p_roughness * p_roughness;
    float alpha2 = alpha * alpha;
    float ndoth = max(dot(p_normal, p_half_vector), 0.0);
    float ndoth2 = ndoth * ndoth;
    float denominator = ndoth2 * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(WBE_PI * denominator * denominator, 0.0001);
}

/**
 * @brief Schlick approximation of the GGX geometry term for a single direction.
 * @param p_ndotv Clamped dot product of the surface normal and the direction being evaluated (view or light).
 * @param p_roughness Linear roughness in [0, 1].
 * @return The single-direction geometry attenuation term.
 */
float geometry_schlick_ggx(float p_ndotv, float p_roughness) {
    float k = (p_roughness + 1.0) * (p_roughness + 1.0) / 8.0;
    return p_ndotv / max(p_ndotv * (1.0 - k) + k, 0.0001);
}

/**
 * @brief Smith geometry term combining the Schlick-GGX attenuation for the view and light directions.
 * @param p_normal The surface normal (unit vector).
 * @param p_view_dir The view direction (unit vector).
 * @param p_light_dir The light direction (unit vector).
 * @param p_roughness Linear roughness in [0, 1].
 * @return The combined geometry attenuation term.
 */
float geometry_smith(float3 p_normal, float3 p_view_dir, float3 p_light_dir, float p_roughness) {
    float ndotv = max(dot(p_normal, p_view_dir), 0.0);
    float ndotl = max(dot(p_normal, p_light_dir), 0.0);
    return geometry_schlick_ggx(ndotv, p_roughness) * geometry_schlick_ggx(ndotl, p_roughness);
}

/**
 * @brief Schlick approximation of the Fresnel reflectance term.
 * @param p_cos_theta The cosine of the angle between the half vector and the view direction, clamped to [0, 1].
 * @param p_f0 The reflectance at normal incidence.
 * @return The Fresnel reflectance for the given angle.
 */
float3 fresnel_schlick(float p_cos_theta, float3 p_f0) {
    return p_f0 + (1.0 - p_f0) * pow(1.0 - clamp(p_cos_theta, 0.0, 1.0), 5.0);
}

/**
 * @brief Compute a world-space surface normal from a tangent-space normal map sample, falling back to a derived tangent frame when the supplied tangent or bitangent is degenerate.
 * @param p_sampled_normal The normal-map sample with components in [0, 1].
 * @param p_normal The interpolated surface normal.
 * @param p_tangent The interpolated surface tangent (may be degenerate).
 * @param p_bitangent The interpolated surface bitangent (may be degenerate).
 * @return The unit-length world-space normal.
 */
float3 normal_from_texture(float3 p_sampled_normal, float3 p_normal, float3 p_tangent, float3 p_bitangent) {
    float3 normal = normalize(p_normal);
    float3 tangent = p_tangent;
    float3 bitangent = p_bitangent;
    if (dot(tangent, tangent) < 0.0001 || dot(bitangent, bitangent) < 0.0001) {
        float3 up = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
        tangent = normalize(cross(up, normal));
        bitangent = normalize(cross(normal, tangent));
    } else {
        tangent = normalize(tangent - normal * dot(normal, tangent));
        bitangent = normalize(bitangent);
    }

    float3 texture_normal = p_sampled_normal * 2.0 - 1.0;
    return normalize(tangent * texture_normal.x + bitangent * texture_normal.y + normal * texture_normal.z);
}

#endif
