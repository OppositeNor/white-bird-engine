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

struct PresentPostProcessingDescIndices {
    [wbe::desc("sampled_image", "result_image")]
    uint2 result_image;
    [wbe::desc("sampled_image", "bloom_ping")]
    uint2 bloom_image;
    [wbe::desc("sampler", "bloom_sampler")]
    uint2 result_sampler;
};

[vk::push_constant]
ConstantBuffer<PresentPostProcessingDescIndices> di;

[shader("fragment")]
float4 fragment_main(VSOutput p_input) : SV_Target0 {
    Texture2D<float4> result_image = ResourceDescriptorHeap[di.result_image.x];
    Texture2D<float4> bloom_image = ResourceDescriptorHeap[di.bloom_image.x];
    SamplerState result_sampler = SamplerDescriptorHeap[di.result_sampler.x];
    float3 color = result_image.SampleLevel(result_sampler, p_input.uv, 0.0).rgb;
    color += bloom_image.SampleLevel(result_sampler, p_input.uv, 0.0).rgb;
    color = color / sqrt(color * color + 1.5);
    return float4(color, 1.0);
}
