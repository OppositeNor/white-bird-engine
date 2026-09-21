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
#include "utils.slang"

struct BloomDescriptorIndices {
    [WBE_DESCRIPTOR("sampled_image")] uint2 bloom_input;
    [WBE_DESCRIPTOR("storage_image")] uint2 bloom_output;
    [WBE_DESCRIPTOR("sampler")] uint2 gbuffer_sampler;
};

static const int KERNEL_RADIUS = 2;
static const float BLOOM_THRESHOLD = 1.0;
static const float BLOOM_THRESHOLD_EPSILON = 0.0001;
static const float KERNEL[5] = {
    1.0 / 16.0,
    4.0 / 16.0,
    6.0 / 16.0,
    4.0 / 16.0,
    1.0 / 16.0
};

[[vk::push_constant]]
ConstantBuffer<BloomDescriptorIndices> descriptor_indices;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    Texture2D<float4> bloom_input = ResourceDescriptorHeap[descriptor_indices.bloom_input.x];
    RWTexture2D<float4> bloom_output = ResourceDescriptorHeap[descriptor_indices.bloom_output.x];
    SamplerState gbuffer_sampler = SamplerDescriptorHeap[descriptor_indices.gbuffer_sampler.x];
    uint width;
    uint height;
    uint2 dispatch_thread_id = z_order_swizzle_8x8(p_group_id.xy, p_group_thread_id.xy);
    bloom_output.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) {
        return;
    }
#ifdef BLOOM_EXTRACT
    float2 sample_uv = (float2(dispatch_thread_id.xy) + 0.5) / float2(width, height);
    float3 sample_color = bloom_input.SampleLevel(gbuffer_sampler, sample_uv, 0).rgb;
    float brightness = dot(sample_color, float3(0.2126, 0.7152, 0.0722));
    float contribution = max(brightness - BLOOM_THRESHOLD, 0.0) / max(brightness, BLOOM_THRESHOLD_EPSILON);
    bloom_output[dispatch_thread_id.xy] = float4(sample_color * contribution, 1.0);
#else
    float3 accumulated_color = 0.0;
    for (int offset = -KERNEL_RADIUS; offset <= KERNEL_RADIUS; ++offset) {
#ifdef BLOOM_HORIZONTAL
        float2 sample_uv = (float2(int2(dispatch_thread_id) + int2(offset, 0)) + 0.5) / float2(width, height);
#else
        float2 sample_uv = (float2(int2(dispatch_thread_id) + int2(0, offset)) + 0.5) / float2(width, height);
#endif
        float3 sample_color = bloom_input.SampleLevel(gbuffer_sampler, sample_uv, 0).rgb;
        accumulated_color += sample_color * KERNEL[offset + KERNEL_RADIUS];
    }
    bloom_output[dispatch_thread_id.xy] = float4(accumulated_color, 1.0);
#endif
}
