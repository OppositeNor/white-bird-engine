/* Copyright 2026 OppositeNor

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
/**
 * @file deferred_composition_kernel.slang
 * @brief The deferred composition compute kernels, whose entry points the ACP generates.
 * @details WBE_USE_SUMMED_AREA_TABLE_SHADOW_MOMENTS filters the shadow moments through their summed-area table, and
 * WBE_VOXEL_DEBUG_VIEW makes indirect_lighting_rsm() output the voxel debug view.
 */
#ifndef WBE_FILE_DEFERRED_COMPOSITION_KERNEL_SLANG
#define WBE_FILE_DEFERRED_COMPOSITION_KERNEL_SLANG

#include "deferred_composition/deferred_composition_utils.slang"
#include "utils.slang"

/**
 * @brief Build the horizontal prefix sums for a shadow-moment summed-area table.
 * @param p_dispatch_thread_id The dispatched row index, in x.
 * @param p_shadow_moments_input The shadow moments.
 * @param p_shadow_moments_horizontal_output Receives the horizontal prefix sums.
 */
[wbe::kernel(64, 1, 1)]
void shadow_moments_summed_area_table_horizontal(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("shadow_moments")] Texture2D<float2> p_shadow_moments_input,
    [wbe::bind("shadow_sat_horizontal")] RWTexture2D<float2> p_shadow_moments_horizontal_output) {
    uint width;
    uint height;
    p_shadow_moments_input.GetDimensions(width, height);
    uint row = p_dispatch_thread_id.x;
    if (row >= height) {
        return;
    }
    float2 moment_sum = float2(0.0);
    for (uint column = 0; column < width; ++column) {
        // H(x, y) = sum(M(i, y), i = 0..x).
        moment_sum += p_shadow_moments_input.Load(int3(column, row, 0));
        p_shadow_moments_horizontal_output[uint2(column, row)] = moment_sum;
    }
}

/**
 * @brief Complete a shadow-moment summed-area table from horizontal prefix sums.
 * @param p_dispatch_thread_id The dispatched column index, in x.
 * @param p_shadow_moments_horizontal_input The horizontal prefix sums.
 * @param p_shadow_moments_summed_area_table_output Receives the summed-area table.
 */
[wbe::kernel(64, 1, 1)]
void shadow_moments_summed_area_table_vertical(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("shadow_sat_horizontal")] Texture2D<float2> p_shadow_moments_horizontal_input,
    [wbe::bind("shadow_sat")] RWTexture2D<float2> p_shadow_moments_summed_area_table_output) {
    uint width;
    uint height;
    p_shadow_moments_horizontal_input.GetDimensions(width, height);
    uint column = p_dispatch_thread_id.x;
    if (column >= width) {
        return;
    }
    float2 moment_sum = float2(0.0);
    for (uint row = 0; row < height; ++row) {
        // SAT(x, y) = sum(H(x, j), j = 0..y).
        moment_sum += p_shadow_moments_horizontal_input.Load(int3(column, row, 0));
        p_shadow_moments_summed_area_table_output[uint2(column, row)] = moment_sum;
    }
}

/**
 * @brief Shade the G-buffer with the direct lighting of every spot light, one invocation per output texel.
 * @param p_group_id The workgroup of the invocation.
 * @param p_group_thread_id The invocation's coordinate in its workgroup, swizzled into Z order.
 * @param p_camera_info The camera the G-buffer was rendered from.
 * @param p_light_infos The spot light table.
 * @param p_lights The descriptors of every spot light's shadow resources.
 * @param p_gbuffer_albedo The G-buffer albedo.
 * @param p_gbuffer_octa_normal The G-buffer octahedron-encoded normal.
 * @param p_gbuffer_rma The G-buffer roughness, metallic, and ambient occlusion.
 * @param p_gbuffer_depth The G-buffer depth.
 * @param p_gbuffer_sampler The sampler the G-buffer is read with.
 * @param p_direct_lighting_output Receives the direct lighting; its extent bounds the dispatch.
 */
[wbe::kernel(16, 16, 1)]
void direct_lighting(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("composition_camera_info")] ConstantBuffer<CompositionCamera> p_camera_info,
    [wbe::bind("spot_light_infos")] StructuredBuffer<SpotLightInfoBuffer> p_light_infos,
    [wbe::push_data] LightParamDescIndices p_lights[WBE_MAX_SPOT_LIGHT_COUNT],
    [wbe::bind("gbuffer_albedo")] Texture2D<float4> p_gbuffer_albedo,
    [wbe::bind("gbuffer_normal")] Texture2D<float2> p_gbuffer_octa_normal,
    [wbe::bind("gbuffer_rma")] Texture2D<float4> p_gbuffer_rma,
    [wbe::bind("gbuffer_depth")] Texture2D<float> p_gbuffer_depth,
    [wbe::bind("gbuffer_sampler")] SamplerState p_gbuffer_sampler,
    [wbe::bind("direct_lighting")] RWTexture2D<float4> p_direct_lighting_output) {
    uint width;
    uint height;
    uint2 dispatch_thread_id = z_order_swizzle_16x16(p_group_id.xy, p_group_thread_id.xy);
    p_direct_lighting_output.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) {
        return;
    }
    float2 uv = (float2(dispatch_thread_id) + 0.5) / float2(width, height);
    float depth = p_gbuffer_depth.SampleLevel(p_gbuffer_sampler, uv, 0.0);
    if (depth <= 0.0) {
        p_direct_lighting_output[dispatch_thread_id] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float4 albedo_matid = p_gbuffer_albedo.SampleLevel(p_gbuffer_sampler, uv, 0.0);
    float2 octa = p_gbuffer_octa_normal.SampleLevel(p_gbuffer_sampler, uv, 0.0);
    float4 rma_flags = p_gbuffer_rma.SampleLevel(p_gbuffer_sampler, uv, 0.0);
    float3 base_color = albedo_matid.rgb;
    float3 normal = octahedron_decode(octa);
    float roughness = clamp(rma_flags.r, 0.04, 1.0);
    float metallic = clamp(rma_flags.g, 0.0, 1.0);
    float3 world_pos = world_position_from_depth(uv, depth, WBE_SOFTWARE_MATRIX_ROWS(p_camera_info.inv_view_proj));
    float3 view_direction = normalize(p_camera_info.camera_world_pos.xyz - world_pos);
    float3 f0 = lerp(float3(0.04), base_color, metallic);
    float3 direct_radiance = float3(0.0);
    uint spot_light_count = get_spot_light_count(p_light_infos);
    for (uint spot_light_index = 0; spot_light_index < spot_light_count; ++spot_light_index) {
        SpotLight spot_light = SpotLight(p_light_infos, p_lights[spot_light_index], spot_light_index);
        float3 to_light = spot_light.info.position - world_pos;
        float light_distance = length(to_light);
        float3 light_direction = to_light / max(light_distance, 0.0001);
        float3 spot_axis = normalize(spot_light.info.direction);
        float spot_cos = dot(-light_direction, spot_axis);
        float cone_falloff = saturate((spot_cos - spot_light.info.outer_cone_cos) /
            max(spot_light.info.inner_cone_cos - spot_light.info.outer_cone_cos, 0.0001));
        float distance_falloff = saturate(1.0 - light_distance / max(spot_light.info.range, 0.0001));
        distance_falloff *= distance_falloff;
        float3 half_vector = normalize(view_direction + light_direction);
        float ndotl = max(dot(normal, light_direction), 0.0);
        float ndf = distribution_ggx(normal, half_vector, roughness);
        float geometry = geometry_smith(normal, view_direction, light_direction, roughness);
        float3 fresnel = fresnel_schlick(max(dot(half_vector, view_direction), 0.0), f0);
        float3 specular = ndf * geometry * fresnel / max(4.0 * max(dot(normal, view_direction), 0.0) * ndotl, 0.0001);
        float3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * base_color / WBE_PI;
        float3 radiance = spot_light.info.color * spot_light.info.intensity * cone_falloff * distance_falloff;
        float shadow = spot_light.compute_shadow(world_pos);
        direct_radiance += (diffuse + specular) * radiance * ndotl * shadow;
    }
    p_direct_lighting_output[dispatch_thread_id] = float4(direct_radiance, 1.0);
}

/**
 * @brief Shade the G-buffer with the reflective-shadow-map indirect lighting of every spot light, one invocation per
 * output texel.
 * @details With WBE_VOXEL_DEBUG_VIEW defined, a constant share of the base color is added for the voxel debug view.
 * @param p_group_id The workgroup of the invocation.
 * @param p_group_thread_id The invocation's coordinate in its workgroup, swizzled into Z order.
 * @param p_camera_info The camera the G-buffer was rendered from.
 * @param p_light_infos The spot light table.
 * @param p_lights The descriptors of every spot light's shadow resources.
 * @param p_gbuffer_albedo The G-buffer albedo.
 * @param p_gbuffer_octa_normal The G-buffer octahedron-encoded normal.
 * @param p_gbuffer_rma The G-buffer roughness, metallic, and ambient occlusion.
 * @param p_gbuffer_depth The G-buffer depth.
 * @param p_gbuffer_sampler The sampler the G-buffer is read with.
 * @param p_indirect_lighting_output Receives the indirect lighting; its extent bounds the dispatch.
 */
[wbe::kernel(16, 16, 1)]
void indirect_lighting_rsm(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("composition_camera_info")] ConstantBuffer<CompositionCamera> p_camera_info,
    [wbe::bind("spot_light_infos")] StructuredBuffer<SpotLightInfoBuffer> p_light_infos,
    [wbe::push_data] LightParamDescIndices p_lights[WBE_MAX_SPOT_LIGHT_COUNT],
    [wbe::bind("gbuffer_albedo")] Texture2D<float4> p_gbuffer_albedo,
    [wbe::bind("gbuffer_normal")] Texture2D<float2> p_gbuffer_octa_normal,
    [wbe::bind("gbuffer_rma")] Texture2D<float4> p_gbuffer_rma,
    [wbe::bind("gbuffer_depth")] Texture2D<float> p_gbuffer_depth,
    [wbe::bind("gbuffer_sampler")] SamplerState p_gbuffer_sampler,
    [wbe::bind("indirect_lighting")] RWTexture2D<float4> p_indirect_lighting_output) {
    uint width;
    uint height;
    uint2 dispatch_thread_id = z_order_swizzle_16x16(p_group_id.xy, p_group_thread_id.xy);
    p_indirect_lighting_output.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) {
        return;
    }
    float2 uv = (float2(dispatch_thread_id) + 0.5) / float2(width, height);
    float depth = p_gbuffer_depth.SampleLevel(p_gbuffer_sampler, uv, 0.0);
    if (depth <= 0.0) {
        p_indirect_lighting_output[dispatch_thread_id] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float4 albedo_matid = p_gbuffer_albedo.SampleLevel(p_gbuffer_sampler, uv, 0.0);
    float2 octa = p_gbuffer_octa_normal.SampleLevel(p_gbuffer_sampler, uv, 0.0);
    float4 rma_flags = p_gbuffer_rma.SampleLevel(p_gbuffer_sampler, uv, 0.0);
    float3 base_color = albedo_matid.rgb;
    float3 normal = octahedron_decode(octa);
    float metallic = clamp(rma_flags.g, 0.0, 1.0);
    float3 world_pos = world_position_from_depth(uv, depth, WBE_SOFTWARE_MATRIX_ROWS(p_camera_info.inv_view_proj));
    float3 indirect_irradiance = float3(0.0);
    uint spot_light_count = get_spot_light_count(p_light_infos);
    for (uint spot_light_index = 0; spot_light_index < spot_light_count; ++spot_light_index) {
        SpotLight spot_light = SpotLight(p_light_infos, p_lights[spot_light_index], spot_light_index);
        indirect_irradiance += compute_rsm_indirect(spot_light, uv, world_pos, normal);
    }
    float3 indirect_lighting = ((1.0 - metallic) * base_color / WBE_PI) * indirect_irradiance;
#if WBE_VOXEL_DEBUG_VIEW
    indirect_lighting += base_color * 0.24;
#endif
    p_indirect_lighting_output[dispatch_thread_id] = float4(indirect_lighting, 1.0);
}

/**
 * @brief Combine direct lighting with indirect lighting attenuated by the G-buffer ambient occlusion.
 * @param p_group_id The workgroup of the invocation.
 * @param p_group_thread_id The invocation's coordinate in its workgroup, swizzled into Z order.
 * @param p_gbuffer_rma The G-buffer roughness, metallic, and ambient occlusion.
 * @param p_direct_lighting The direct lighting.
 * @param p_indirect_lighting The indirect lighting.
 * @param p_result_image Receives the combined lighting; its extent bounds the dispatch.
 */
[wbe::kernel(16, 16, 1)]
void combine_lighting(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("gbuffer_rma")] Texture2D<float4> p_gbuffer_rma,
    [wbe::bind("direct_lighting")] Texture2D<float4> p_direct_lighting,
    [wbe::bind("indirect_lighting")] Texture2D<float4> p_indirect_lighting,
    [wbe::bind("result_image")] RWTexture2D<float4> p_result_image) {
    uint width;
    uint height;
    uint2 dispatch_thread_id = z_order_swizzle_16x16(p_group_id.xy, p_group_thread_id.xy);
    p_result_image.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) {
        return;
    }
    float ambient_occlusion = clamp(p_gbuffer_rma.Load(int3(dispatch_thread_id, 0)).b, 0.0, 1.0);
    float3 direct = p_direct_lighting.Load(int3(dispatch_thread_id, 0)).rgb;
    float3 indirect = p_indirect_lighting.Load(int3(dispatch_thread_id, 0)).rgb;
    p_result_image[dispatch_thread_id] = float4(direct + indirect * ambient_occlusion, 1.0);
}

#endif
