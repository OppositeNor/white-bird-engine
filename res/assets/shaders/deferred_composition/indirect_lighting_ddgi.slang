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
#include "ddgi/ddgi_common.slang"

struct IndirectLightingDescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer")]
    uint2 camera_info;
    [WBE_DESCRIPTOR("uniform_buffer")]
    uint2 ddgi_config;
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 irradiance_atlas;
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 distance_atlas;
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 gbuffer_albedo;
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 gbuffer_octa_normal;
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 gbuffer_rma;
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 gbuffer_depth;
    [WBE_DESCRIPTOR("sampler")]
    uint2 gbuffer_sampler;
    [WBE_DESCRIPTOR("storage_image")]
    uint2 indirect_lighting_output;
};

[[vk::push_constant]]
ConstantBuffer<IndirectLightingDescriptorIndices> descriptor_indices;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    ConstantBuffer<CompositionCamera> camera_info = ResourceDescriptorHeap[descriptor_indices.camera_info.x];
    ConstantBuffer<DDGIConfig> ddgi_config = ResourceDescriptorHeap[descriptor_indices.ddgi_config.x];
    Texture2D<float4> irradiance_atlas = ResourceDescriptorHeap[descriptor_indices.irradiance_atlas.x];
    Texture2D<float2> distance_atlas = ResourceDescriptorHeap[descriptor_indices.distance_atlas.x];
    Texture2D<float4> gbuffer_albedo = ResourceDescriptorHeap[descriptor_indices.gbuffer_albedo.x];
    Texture2D<float2> gbuffer_octa_normal = ResourceDescriptorHeap[descriptor_indices.gbuffer_octa_normal.x];
    Texture2D<float4> gbuffer_rma = ResourceDescriptorHeap[descriptor_indices.gbuffer_rma.x];
    Texture2D<float> gbuffer_depth = ResourceDescriptorHeap[descriptor_indices.gbuffer_depth.x];
    SamplerState gbuffer_sampler = SamplerDescriptorHeap[descriptor_indices.gbuffer_sampler.x];
    RWTexture2D<float4> indirect_lighting_output = ResourceDescriptorHeap[descriptor_indices.indirect_lighting_output.x];
    uint width;
    uint height;
    uint2 dispatch_thread_id = z_order_swizzle_8x8(p_group_id.xy, p_group_thread_id.xy);
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
    float ambient_occlusion = clamp(rma_flags.b, 0.0, 1.0);
    float metallic = clamp(rma_flags.g, 0.0, 1.0);
    float3 world_position = world_position_from_depth(uv, depth, WBE_SOFTWARE_MATRIX_ROWS(camera_info.inv_view_proj));
    float3 view_direction = normalize(camera_info.camera_world_pos.xyz - world_position);
    float3 indirect_irradiance =
        ddgi_sample_irradiance(ddgi_config, irradiance_atlas, distance_atlas, world_position, normal, view_direction);
    indirect_irradiance *= ambient_occlusion * ddgi_indirect_scale(ddgi_config);
    float3 indirect_lighting = ((1.0 - metallic) * base_color / WBE_PI) * indirect_irradiance;
    indirect_lighting_output[dispatch_thread_id] = float4(indirect_lighting, 1.0);
}
