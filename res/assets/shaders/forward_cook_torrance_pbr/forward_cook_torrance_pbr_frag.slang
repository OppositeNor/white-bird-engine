// fragment
#include "forward_cook_torrance_pbr/forward_cook_torrance_pbr_utils.slang"

/**
 * @brief Combined image+sampler texture providing the albedo material map.
 */
[[vk::binding(1, 0)]] Texture2D<float4> albedo;
/**
 * @brief Combined image+sampler texture providing the tangent-space normal map.
 */
[[vk::binding(2, 0)]] Texture2D<float4> normal;
/**
 * @brief Combined image+sampler texture providing the roughness (r), metallic (g), and ambient occlusion (b) channels packed together.
 */
[[vk::binding(3, 0)]] Texture2D<float4> roughness_metallic_ao;
/**
 * @brief Sampler used to read the material textures above.
 */
[[vk::binding(4, 0)]] SamplerState material_sampler;
/**
 * @brief Spot-light parameters consumed by the lighting evaluation and the shadow map sample.
 */
[[vk::binding(5, 0)]] ConstantBuffer<SpotLightInfoBuffer> spot_light_infos;
/**
 * @brief Depth texture rendered by the spot-light shadow map pass. Sampled in the spot light's clip space.
 */
[[vk::binding(6, 0)]] Texture2D<float> shadowmap;
/**
 * @brief Sampler used to read the shadow map. A simple bilinear, clamped sampler is sufficient for percentage-closer filtering done in the shader.
 */
[[vk::binding(7, 0)]] SamplerState shadowmap_sampler;

/**
 * @brief Compute the unshadowed visibility term for the spot light at the given light-space position.
 * @param p_light_space_pos The fragment position transformed by the spot light's view/proj.
 * @return 1.0 if the fragment is fully lit, 0.0 if fully in shadow, or an interpolated PCF value.
 */
float compute_spot_shadow(float4 p_light_space_pos) {
    float3 ndc = p_light_space_pos.xyz / max(p_light_space_pos.w, 0.0001);
    // Vulkan clip space: x,y in [-1,1] -> map to [0,1]; z already in [0,1].
    float2 uv = ndc.xy * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || ndc.z < 0.0 || ndc.z > 1.0) {
        return 1.0;
    }
    const float bias = 0.0015;
    float current_depth = ndc.z - bias;
    // 2x2 percentage-closer filter.
    float2 texel_size;
    uint shadow_width;
    uint shadow_height;
    shadowmap.GetDimensions(shadow_width, shadow_height);
    texel_size = float2(1.0 / float(shadow_width), 1.0 / float(shadow_height));
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float sampled = shadowmap.Sample(shadowmap_sampler, uv + float2(x, y) * texel_size).r;
            visibility += current_depth <= sampled ? 1.0 : 0.0;
        }
    }
    return visibility / 9.0;
}

/**
 * @brief Evaluate the Cook-Torrance BRDF for one forward-shaded fragment lit by a single shadow-mapped spot light.
 * @param p_input The interpolated vertex output.
 * @return The sRGB-encoded shaded color with alpha = 1.0.
 */
float4 main(VSOutput p_input) : SV_Target0 {
    float3 base_color = srgb_to_linear(albedo.Sample(material_sampler, p_input.uv).rgb);
    float3 sampled_normal = normal.Sample(material_sampler, p_input.uv).rgb;
    float3 rma = roughness_metallic_ao.Sample(material_sampler, p_input.uv).rgb;
    float roughness = clamp(rma.r, 0.04, 1.0);
    float metallic = clamp(rma.g, 0.0, 1.0);
    float ambient_occlusion = clamp(rma.b, 0.0, 1.0);

    float3 n = normal_from_texture(sampled_normal, p_input.normal, p_input.tangent, p_input.bitangent);
    float3 v = normalize(float3(0.0, 0.0, 3.0) - p_input.world_position);

    float3 f0 = lerp(float3(0.04), base_color, metallic);

    float3 ambient = base_color * 0.03 * ambient_occlusion;
    float3 color = ambient;
    uint spot_light_count = min(spot_light_infos.spot_light_count, WBE_MAX_SPOT_LIGHT_COUNT);
    for (uint spot_light_index = 0; spot_light_index < spot_light_count; ++spot_light_index) {
        SpotLightInfo spot_light_info = spot_light_infos.spot_light_info[spot_light_index];
        // Spot light direction: from surface to light.
        float3 to_light = spot_light_info.position - p_input.world_position;
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
        float shadow = spot_light_index == 0 ? compute_spot_shadow(p_input.light_space_pos) : 1.0;
        color += (diffuse + specular) * radiance * ndotl * shadow;
    }
    color = color / (color + float3(1.0));
    return float4(linear_to_srgb(color), 1.0);
}
