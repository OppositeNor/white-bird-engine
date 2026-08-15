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
ConstantBuffer<MaterialParamsBuffer> material_params;
ParameterBlock<LightParams> light;
ParameterBlock<MaterialTextures> material_textures;

/**
 * @brief Transform a textured vertex into clip space and pass per-vertex attributes through to the fragment stage.
 * @param p_input The incoming vertex attributes.
 * @return The vertex output for the forward Cook-Torrance PBR pass.
 */
[shader("vertex")]
VSOutput vertex_main(Vertex3DTexturedInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    VSOutput output;
    float4 world_position = float4(p_input.position.xyz, 1.0);
    output.position = mul(camera_info.proj, mul(camera_info.view, world_position));
    output.world_position = float4(p_input.position, float(p_instance_id));
    output.normal = p_input.normal;
    output.tangent = p_input.tangent;
    output.bitangent = p_input.bitangent;
    output.uv = p_input.uv;
    if (light.light_count > 0) {
        output.light_space_pos = mul(light.lights[0].info.proj, mul(light.lights[0].info.view, world_position));
    } else {
        output.light_space_pos = world_position;
    }
    return output;
}

/**
 * @brief Compute the unshadowed visibility term for the spot light at the given light-space position.
 * @param p_light_space_pos The fragment position transformed by the spot light's view/proj.
 * @return 1.0 if the fragment is fully lit, 0.0 if fully in shadow, or an interpolated PCF value.
 */
float compute_spot_shadow(uint p_spot_light_index, float4 p_light_space_pos) {
    float3 ndc = p_light_space_pos.xyz / max(p_light_space_pos.w, 0.0001);
    float2 uv = ndc.xy * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || ndc.z < 0.0 || ndc.z > 1.0) {
        return 1.0;
    }
    const float bias = 0.0015;
    float current_depth = ndc.z - bias;
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
 * @param p_masked Whether fragments below the alpha-mask cutoff should be discarded.
 * @return The sRGB-encoded shaded color with alpha = 1.0.
 */
float4 shade_fragment(VSOutput p_input, bool p_masked) {
    // The instance id crosses the rasterizer in world_position.w. Round away interpolation error before using it as a table
    // index.
    uint material_index = uint(round(p_input.world_position.w));
    MaterialParams active_material = material_params.materials[material_index];
    uint albedo_index = uint(active_material.albedo_index >= 0 ? active_material.albedo_index : 0);
    uint normal_index = uint(active_material.normal_index >= 0 ? active_material.normal_index : 0);
    uint roughness_metallic_ao_index =
        uint(active_material.roughness_metallic_ao_index >= 0 ? active_material.roughness_metallic_ao_index : 0);
    uint albedo_descriptor_index = NonUniformResourceIndex(albedo_index);
    uint normal_descriptor_index = NonUniformResourceIndex(normal_index);
    uint roughness_metallic_ao_descriptor_index = NonUniformResourceIndex(roughness_metallic_ao_index);

    float4 albedo_sample =
        material_textures.textures[albedo_descriptor_index].Sample(material_textures.material_sampler, p_input.uv);
    if (p_masked && albedo_sample.a < 0.5) {
        discard;
    }
    float3 base_color = albedo_sample.rgb;
    float3 sampled_normal =
        material_textures.textures[normal_descriptor_index].Sample(material_textures.material_sampler, p_input.uv).rgb;
    float3 rma = material_textures.textures[roughness_metallic_ao_descriptor_index]
                     .Sample(material_textures.material_sampler, p_input.uv)
                     .rgb;
    float roughness = clamp(rma.r, 0.04, 1.0);
    float metallic = clamp(rma.g, 0.0, 1.0);
    float ambient_occlusion = clamp(rma.b, 0.0, 1.0);

    float3 n = normal_from_texture(sampled_normal, p_input.normal, p_input.tangent, p_input.bitangent);
    float3 v = normalize(float3(0.0, 0.0, 3.0) - p_input.world_position.xyz);

    float3 f0 = lerp(float3(0.04), base_color, metallic);

    float3 ambient = base_color * 0.03 * ambient_occlusion;
    float3 color = ambient;
    uint spot_light_count = min(light.light_count, WBE_MAX_SPOT_LIGHT_COUNT);
    for (uint spot_light_index = 0; spot_light_index < spot_light_count; ++spot_light_index) {
        SpotLightInfo spot_light_info = light.lights[spot_light_index].info;
        float3 to_light = spot_light_info.position - p_input.world_position.xyz;
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

[shader("fragment")]
float4 fragment_main(VSOutput p_input) : SV_Target0 {
    return shade_fragment(p_input, false);
}

[shader("fragment")]
float4 fragment_masked_main(VSOutput p_input) : SV_Target0 {
    return shade_fragment(p_input, true);
}
