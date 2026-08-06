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
#include "deferred_composition/deferred_composition_utils.slang"
#include "defs.slang"

/**
 * @brief Emit one vertex of a fullscreen triangle covering the viewport for the deferred composition pass.
 * @param p_vertex_id The SV_VertexID for the current vertex (0..2).
 * @return The vertex output containing clip-space position and screen-space uv coordinates.
 */
[shader("vertex")]
VSOutput vertex_main(uint p_vertex_id : SV_VertexID) {
    VSOutput output;

    // Fullscreen triangle covering the entire viewport.
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    float2 uvs[3] = {
        float2(0.0, 0.0),
        float2(0.0, 2.0),
        float2(2.0, 0.0)
    };

    output.position = float4(positions[p_vertex_id], 0.0, 1.0);
    output.uv       = uvs[p_vertex_id];

    return output;
}

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
 * @brief Constant buffer providing the per-frame composition camera data.
 */
ConstantBuffer<CompositionCamera> camera_info;
/**
 * @brief G-buffer albedo target: rgb = base color, a = material id.
 */
Texture2D<float4> gbuffer_albedo;
/**
 * @brief G-buffer normal target: rg = octahedron-encoded normal.
 */
Texture2D<float2> gbuffer_octa_normal;
/**
 * @brief G-buffer material parameter target: r = roughness, g = metallic, b = ambient occlusion, a = flags.
 */
Texture2D<float4> gbuffer_rma;
/**
 * @brief G-buffer depth target.
 */
Texture2D<float>  gbuffer_depth;
/**
 * @brief Sampler used to read all G-buffer attachments above.
 */
SamplerState gbuffer_sampler;

/**
 * @brief Evaluate the deferred lighting equation for one screen pixel using the G-buffer attachments and a hard-coded directional light.
 * @param p_input The interpolated vertex output from the fullscreen triangle.
 * @return The sRGB-encoded shaded color with alpha = 1.0. Pixels whose depth is at or past the far plane are replaced with a constant background.
 */
[shader("fragment")]
float4 fragment_main(VSOutput p_input) : SV_Target0 {
    float depth = gbuffer_depth.Sample(gbuffer_sampler, p_input.uv);
    if (depth >= 1.0) {
        // Background: sky-style clear color.
        return float4(linear_to_srgb(float3(0.05, 0.07, 0.10)), 1.0);
    }

    float4 albedo_matid = gbuffer_albedo.Sample(gbuffer_sampler, p_input.uv);
    float2 octa         = gbuffer_octa_normal.Sample(gbuffer_sampler, p_input.uv);
    float4 rma_flags    = gbuffer_rma.Sample(gbuffer_sampler, p_input.uv);

    float3 base_color = albedo_matid.rgb;
    float3 n          = octahedron_decode(octa);
    float roughness   = clamp(rma_flags.r, 0.04, 1.0);
    float metallic    = clamp(rma_flags.g, 0.0, 1.0);
    float ambient_occlusion = clamp(rma_flags.b, 0.0, 1.0);

    float3 world_pos = world_position_from_depth(p_input.uv, depth, camera_info.inv_view_proj);
    float3 v = normalize(camera_info.camera_world_pos.xyz - world_pos);
    float3 l = normalize(float3(-0.45, -0.65, -0.6));
    float3 h = normalize(v + l);
    float ndotl = max(dot(n, l), 0.0);

    float3 f0 = lerp(float3(0.04), base_color, metallic);
    float ndf = distribution_ggx(n, h, roughness);
    float geometry = geometry_smith(n, v, l, roughness);
    float3 fresnel = fresnel_schlick(max(dot(h, v), 0.0), f0);
    float3 specular = ndf * geometry * fresnel / max(4.0 * max(dot(n, v), 0.0) * ndotl, 0.0001);
    float3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * base_color / WBE_PI;
    float3 radiance = float3(4.0, 4.0, 4.0);
    float3 ambient = base_color * 0.03 * ambient_occlusion;
    float3 color = ambient + (diffuse + specular) * radiance * ndotl;
    color = color / (color + float3(1.0));
    return float4(linear_to_srgb(color), 1.0);
}