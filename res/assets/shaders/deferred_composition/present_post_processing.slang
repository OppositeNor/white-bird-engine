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

[shader("vertex")]
VSOutput vertex_main(uint p_vertex_id : SV_VulkanVertexID) {
    return build_fullscreen_triangle(p_vertex_id);
}

struct PresentPostProcessingDescriptorIndices {
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 gbuffer_rma;
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 gbuffer_depth;
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 direct_lighting_texture;
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 indirect_lighting_texture;
    [WBE_DESCRIPTOR("sampler")]
    uint2 gbuffer_sampler;
};

[[vk::push_constant]]
ConstantBuffer<PresentPostProcessingDescriptorIndices> descriptor_indices;

[shader("fragment")]
float4 fragment_main(VSOutput p_input) : SV_Target0 {
    Texture2D<float4> gbuffer_rma = ResourceDescriptorHeap[descriptor_indices.gbuffer_rma.x];
    Texture2D<float> gbuffer_depth = ResourceDescriptorHeap[descriptor_indices.gbuffer_depth.x];
    Texture2D<float4> direct_lighting_texture = ResourceDescriptorHeap[descriptor_indices.direct_lighting_texture.x];
    Texture2D<float4> indirect_lighting_texture = ResourceDescriptorHeap[descriptor_indices.indirect_lighting_texture.x];
    SamplerState gbuffer_sampler = SamplerDescriptorHeap[descriptor_indices.gbuffer_sampler.x];
    float depth = gbuffer_depth.SampleLevel(gbuffer_sampler, p_input.uv, 0.0);
    if (depth <= 0.0) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    float4 rma_flags = gbuffer_rma.SampleLevel(gbuffer_sampler, p_input.uv, 0.0);
    float ambient_occlusion = clamp(rma_flags.b, 0.0, 1.0);
    uint2 pixel = uint2(p_input.position.xy);
    float3 direct_lighting = direct_lighting_texture.Load(int3(pixel, 0)).rgb;
    float3 indirect_lighting = indirect_lighting_texture.Load(int3(pixel, 0)).rgb;
    float3 color = (direct_lighting + indirect_lighting) * ambient_occlusion;
    color = color / (color + float3(1.0));
    return float4(color, 1.0);
}
