// fragment
#include "forward_cook_torrance_pbr/forward_cook_torrance_pbr_utils.slang"

[[vk::binding(1, 0)]] Texture2D<float4> albedo;
[[vk::binding(2, 0)]] Texture2D<float4> normal;
[[vk::binding(3, 0)]] Texture2D<float4> roughness_metallic_ao;
[[vk::binding(4, 0)]] SamplerState material_sampler;

float4 main(VSOutput p_input) : SV_Target0 {
    float3 base_color = srgb_to_linear(albedo.Sample(material_sampler, p_input.uv).rgb);
    float3 sampled_normal = normal.Sample(material_sampler, p_input.uv).rgb;
    float3 rma = roughness_metallic_ao.Sample(material_sampler, p_input.uv).rgb;
    float roughness = clamp(rma.r, 0.04, 1.0);
    float metallic = clamp(rma.g, 0.0, 1.0);
    float ambient_occlusion = clamp(rma.b, 0.0, 1.0);

    float3 n = normal_from_texture(sampled_normal, p_input.normal, p_input.tangent, p_input.bitangent);
    float3 v = normalize(float3(0.0, 0.0, 3.0) - p_input.world_position);
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