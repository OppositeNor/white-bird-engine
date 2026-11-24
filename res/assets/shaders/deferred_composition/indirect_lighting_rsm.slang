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

struct IndirectLightingDescIndices {
    [wbe::desc("uniform_buffer", "composition_camera_info")]
    DescriptorHandle<ConstantBuffer<CompositionCamera>> camera_info;
    [wbe::desc("storage_buffer", "spot_light_infos", "read")]
    DescriptorHandle<StructuredBuffer<SpotLightInfoBuffer>> light_infos;
    LightParamDescIndices lights[WBE_MAX_SPOT_LIGHT_COUNT];
    [wbe::desc("sampled_image", "gbuffer_albedo")]
    DescriptorHandle<Texture2D<float4>> gbuffer_albedo;
    [wbe::desc("sampled_image", "gbuffer_normal")]
    DescriptorHandle<Texture2D<float2>> gbuffer_octa_normal;
    [wbe::desc("sampled_image", "gbuffer_rma")]
    DescriptorHandle<Texture2D<float4>> gbuffer_rma;
    [wbe::desc("sampled_image", "gbuffer_depth")]
    DescriptorHandle<Texture2D<float>> gbuffer_depth;
    [wbe::desc("sampler", "gbuffer_sampler")]
    DescriptorHandle<SamplerState> gbuffer_sampler;
    [wbe::desc("storage_image", "indirect_lighting")]
    DescriptorHandle<RWTexture2D<float4>> indirect_lighting_output;
};

[vk::push_constant]
ConstantBuffer<IndirectLightingDescIndices> di;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    ConstantBuffer<CompositionCamera> camera_info = di.camera_info;
    Texture2D<float4> gbuffer_albedo = di.gbuffer_albedo;
    Texture2D<float2> gbuffer_octa_normal = di.gbuffer_octa_normal;
    Texture2D<float4> gbuffer_rma = di.gbuffer_rma;
    Texture2D<float> gbuffer_depth = di.gbuffer_depth;
    SamplerState gbuffer_sampler = di.gbuffer_sampler;
    RWTexture2D<float4> indirect_lighting_output = di.indirect_lighting_output;
    uint width;
    uint height;
    uint2 dispatch_thread_id = z_order_swizzle_16x16(p_group_id.xy, p_group_thread_id.xy);
    indirect_lighting_output.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) {
        return;
    }
    float2 uv = (float2(dispatch_thread_id) + 0.5) / float2(width, height);
    float depth = gbuffer_depth.SampleLevel(gbuffer_sampler, uv, 0.0);
    if (depth <= 0.0) {
        indirect_lighting_output[dispatch_thread_id] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float4 albedo_matid = gbuffer_albedo.SampleLevel(gbuffer_sampler, uv, 0.0);
    float2 octa = gbuffer_octa_normal.SampleLevel(gbuffer_sampler, uv, 0.0);
    float4 rma_flags = gbuffer_rma.SampleLevel(gbuffer_sampler, uv, 0.0);
    float3 base_color = albedo_matid.rgb;
    float3 normal = octahedron_decode(octa);
    float metallic = clamp(rma_flags.g, 0.0, 1.0);
    float3 world_pos = world_position_from_depth(uv, depth, WBE_SOFTWARE_MATRIX_ROWS(camera_info.inv_view_proj));
    float3 indirect_irradiance = float3(0.0);
    uint spot_light_count = get_spot_light_count(di.light_infos);
    for (uint spot_light_index = 0; spot_light_index < spot_light_count; ++spot_light_index) {
        SpotLight spot_light = SpotLight(di.light_infos, di.lights[spot_light_index], spot_light_index);
        indirect_irradiance += compute_rsm_indirect(spot_light, uv, world_pos, normal);
    }
    float3 indirect_lighting = ((1.0 - metallic) * base_color / WBE_PI) * indirect_irradiance;
#if WBE_VOXEL_DEBUG_VIEW
    indirect_lighting += base_color * 0.24;
#endif
    indirect_lighting_output[dispatch_thread_id] = float4(indirect_lighting, 1.0);
}
