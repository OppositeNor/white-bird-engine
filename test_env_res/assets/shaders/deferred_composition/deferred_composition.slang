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
VSOutput vertex_main(uint p_vertex_id : SV_VulkanVertexID) {
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
 * @brief Constant buffer providing the per-frame composition camera data.
 */
ConstantBuffer<CompositionCamera> camera_info;
/**
 * @brief Descriptor-backed spot lights and their active count.
 */
ParameterBlock<LightParams> light;
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
 * @brief Compute shadow visibility for one spot light at a reconstructed world-space position.
 * @param p_spot_light_index Descriptor-array index of the spot light.
 * @param p_world_position Reconstructed world-space fragment position.
 * @return The percentage-closer-filtered visibility in [0, 1].
 */
float compute_spot_shadow(uint p_spot_light_index, float3 p_world_position) {
    SpotLightInfo spot_light_info = light.lights[p_spot_light_index].info;
    float4 light_space_position =
        mul(spot_light_info.proj, mul(spot_light_info.view, float4(p_world_position, 1.0)));
    float3 ndc = light_space_position.xyz / max(light_space_position.w, 0.0001);
    float2 uv = ndc.xy * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || ndc.z < 0.0 || ndc.z > 1.0) {
        return 1.0;
    }
    const float bias = 0.0015;
    float current_depth = ndc.z - bias;
    uint shadow_width;
    uint shadow_height;
    light.lights[p_spot_light_index].shadowmap.GetDimensions(shadow_width, shadow_height);
    float2 texel_size = float2(1.0 / float(shadow_width), 1.0 / float(shadow_height));
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float sampled = light.lights[p_spot_light_index].shadowmap.Sample(
                light.lights[p_spot_light_index].shadowmap_sampler, uv + float2(x, y) * texel_size).r;
            visibility += current_depth <= sampled ? 1.0 : 0.0;
        }
    }
    return visibility / 9.0;
}

/**
 * @brief Evaluate deferred spot lighting for one screen pixel using the G-buffer and descriptor-backed light resources.
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
    float3 f0 = lerp(float3(0.04), base_color, metallic);
    float3 color = float3(0.0);
    uint spot_light_count = min(light.light_count, WBE_MAX_SPOT_LIGHT_COUNT);
    for (uint spot_light_index = 0; spot_light_index < spot_light_count; ++spot_light_index) {
        SpotLightInfo spot_light_info = light.lights[spot_light_index].info;
        float3 to_light = spot_light_info.position - world_pos;
        float light_distance = length(to_light);
        float3 l = to_light / max(light_distance, 0.0001);
        float3 spot_axis = normalize(spot_light_info.direction);
        float spot_cos = dot(-l, spot_axis);
        float cone_falloff = saturate(
            (spot_cos - spot_light_info.outer_cone_cos) /
            max(spot_light_info.inner_cone_cos - spot_light_info.outer_cone_cos, 0.0001));
        float distance_falloff = saturate(1.0 - light_distance / max(spot_light_info.range, 0.0001));
        distance_falloff *= distance_falloff;

        float3 h = normalize(v + l);
        float ndotl = max(dot(n, l), 0.0);
        float ndf = distribution_ggx(n, h, roughness);
        float geometry = geometry_smith(n, v, l, roughness);
        float3 fresnel = fresnel_schlick(max(dot(h, v), 0.0), f0);
        float3 specular = ndf * geometry * fresnel / max(4.0 * max(dot(n, v), 0.0) * ndotl, 0.0001);
        float3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * base_color / WBE_PI;
        float3 radiance = spot_light_info.color * spot_light_info.intensity * cone_falloff * distance_falloff;
        float shadow = compute_spot_shadow(spot_light_index, world_pos);
        color += (diffuse + specular) * radiance * ndotl * shadow;
    }
    color *= ambient_occlusion;
    color = color / (color + float3(1.0));
    return float4(linear_to_srgb(color), 1.0);
}
