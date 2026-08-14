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
#ifndef WBE_FILE_DEFERRED_COMPOSITION_UTILS_SLANG
#define WBE_FILE_DEFERRED_COMPOSITION_UTILS_SLANG

#include "utils.slang"

static const uint WBE_MAX_SPOT_LIGHT_COUNT = 4;

/**
 * @brief Vertex shader output / fragment shader input for the deferred composition fullscreen pass.
 */
struct VSOutput {
    /**
 * @brief Clip-space position of the fullscreen triangle vertex.
     */
    float4 position : SV_Position;
    /**
 * @brief Screen-space uv used to sample the G-buffer attachments.
     */
    float2 uv       : TEXCOORD0;
};

/**
 * @brief Spot light parameters consumed by the deferred composition pass.
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
    /// World-space direction the spot is pointing.
    float3 direction;
    /// Cosine of the inner cone half-angle.
    float inner_cone_cos;
    /// Linear-space color of the spot light.
    float3 color;
    /// Cosine of the outer cone half-angle.
    float outer_cone_cos;
    /// Maximum effective light distance.
    float range;
};

/**
 * @brief Per-frame camera data used by the deferred composition pass to reconstruct world-space positions from depth and evaluate view directions.
 */
struct CompositionCamera {
    /**
 * @brief Inverse view-projection matrix used for world-space position reconstruction from screen-space uv and depth.
     */
    float4x4 inv_view_proj;
    /**
 * @brief Camera world-space position (xyz). The w component is unused.
     */
    float4   camera_world_pos;
};


/**
 * @brief Descriptor-backed resources for one spot light.
 */
struct LightParam {
    ConstantBuffer<SpotLightInfo> info;
    Texture2D<float> shadowmap;
    SamplerState shadowmap_sampler;
};

/**
 * @brief Variable-active-size spot light descriptor table.
 */
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
 * @brief Reconstruct world-space position from a screen-space uv and depth sample using the inverse view-projection matrix.
 *
 * The Vulkan clip-space y is flipped in the projection matrix used by the engine, so the y of the NDC sample matches the uv direction directly.
 *
 * @param p_uv Screen-space uv in [0, 1].
 * @param p_depth Depth value in [0, 1] sampled from the depth target.
 * @param p_inv_view_proj The inverse view-projection matrix.
 * @return The reconstructed world-space position.
 */
float3 world_position_from_depth(float2 p_uv, float p_depth, float4x4 p_inv_view_proj) {
    // uv is in [0, 1]. NDC for Vulkan: x in [-1, 1], y in [-1, 1], z in [0, 1].
    float2 ndc_xy = p_uv * 2.0 - 1.0;
    float4 ndc = float4(ndc_xy, p_depth, 1.0);
    float4 world = mul(p_inv_view_proj, ndc);
    return world.xyz / world.w;
}

#endif
