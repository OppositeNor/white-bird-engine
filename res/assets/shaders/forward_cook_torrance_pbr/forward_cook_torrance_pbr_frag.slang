// fragment
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
#include "forward_cook_torrance_pbr/forward_cook_torrance_pbr_utils.slang"
#include "defs.slang"

ConstantBuffer<CameraInfo> camera_info;
ConstantBuffer<MaterialParams> material_params;
ParameterBlock<LightParams> light;
ParameterBlock<MaterialTextures> material_textures;

/**
 * @brief Compute the unshadowed visibility term for the spot light at the given light-space position.
 * @param p_light_space_pos The fragment position transformed by the spot light's view/proj.
 * @return 1.0 if the fragment is fully lit, 0.0 if fully in shadow, or an interpolated PCF value.
 */
float compute_spot_shadow(uint p_spot_light_index, float4 p_light_space_pos) {
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
    light.lights[p_spot_light_index].shadowmap.GetDimensions(shadow_width, shadow_height);
    texel_size = float2(1.0 / float(shadow_width), 1.0 / float(shadow_height));
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
 * @brief Evaluate the Cook-Torrance BRDF for one forward-shaded fragment lit by a single shadow-mapped spot light.
 * @param p_input The interpolated vertex output.
 * @return The sRGB-encoded shaded color with alpha = 1.0.
 */
float4 main(VSOutput p_input) : SV_Target0 {
    uint albedo_index = uint(material_params.albedo_index >= 0 ? material_params.albedo_index : 0);
    uint normal_index = uint(material_params.normal_index >= 0 ? material_params.normal_index : 0);
    uint roughness_metallic_ao_index = uint(material_params.roughness_metallic_ao_index >= 0 ? material_params.roughness_metallic_ao_index : 0);
    uint albedo_descriptor_index = NonUniformResourceIndex(albedo_index);
    uint normal_descriptor_index = NonUniformResourceIndex(normal_index);
    uint roughness_metallic_ao_descriptor_index = NonUniformResourceIndex(roughness_metallic_ao_index);

    float3 base_color =
        material_textures.textures[albedo_descriptor_index].Sample(material_textures.material_sampler, p_input.uv).rgb;
    float3 sampled_normal =
        material_textures.textures[normal_descriptor_index].Sample(material_textures.material_sampler, p_input.uv).rgb;
    float3 rma = material_textures.textures[roughness_metallic_ao_descriptor_index]
                     .Sample(material_textures.material_sampler, p_input.uv)
                     .rgb;
    float roughness = clamp(rma.r, 0.04, 1.0);
    float metallic = clamp(rma.g, 0.0, 1.0);
    float ambient_occlusion = clamp(rma.b, 0.0, 1.0);

    float3 n = normal_from_texture(sampled_normal, p_input.normal, p_input.tangent, p_input.bitangent);
    float3 v = normalize(float3(0.0, 0.0, 3.0) - p_input.world_position);

    float3 f0 = lerp(float3(0.04), base_color, metallic);

    float3 ambient = base_color * 0.03 * ambient_occlusion;
    float3 color = ambient;
    uint spot_light_count = min(light.light_count, WBE_MAX_SPOT_LIGHT_COUNT);
    for (uint spot_light_index = 0; spot_light_index < spot_light_count; ++spot_light_index) {
        SpotLightInfo spot_light_info = light.lights[spot_light_index].info;
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
        float shadow = spot_light_index == 0 ? compute_spot_shadow(spot_light_index, p_input.light_space_pos) : 1.0;
        color += (diffuse + specular) * radiance * ndotl * shadow;
    }
    color = color / (color + float3(1.0));
    return float4(linear_to_srgb(color), 1.0);
}
