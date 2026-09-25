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

struct BloomBlurDescriptorIndices {
#ifdef WBE_USE_PING_WRITE
    [WBE_DESCRIPTOR("storage_image", "bloom_ping")] uint2 blur_texture_write;
    [WBE_DESCRIPTOR("sampled_image", "bloom_pong")] uint2 blur_texture_read;
#else
    [WBE_DESCRIPTOR("storage_image", "bloom_pong")] uint2 blur_texture_write;
    [WBE_DESCRIPTOR("sampled_image", "bloom_ping")] uint2 blur_texture_read;
#endif
    [WBE_DESCRIPTOR("sampler", "bloom_sampler")] uint2 bloom_sampler;
};

static const float KAWASE_SAMPLE_OFFSET = 1.5;

[[vk::push_constant]]
ConstantBuffer<BloomBlurDescriptorIndices> descriptor_indices;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    Texture2D<float4> blur_texture_read = ResourceDescriptorHeap[descriptor_indices.blur_texture_read.x];
    RWTexture2D<float4> blur_texture_write = ResourceDescriptorHeap[descriptor_indices.blur_texture_write.x];
    SamplerState bloom_sampler = SamplerDescriptorHeap[descriptor_indices.bloom_sampler.x];
    uint width;
    uint height;
    blur_texture_write.GetDimensions(width, height);
    uint2 dispatch_thread_id = z_order_swizzle_8x8(p_group_id.xy, p_group_thread_id.xy);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) {
        return;
    }
    float2 inverse_extent = 1.0 / float2(width, height);
    float2 sample_uv = (float2(dispatch_thread_id.xy) + 0.5) * inverse_extent;
    float2 sample_offset = KAWASE_SAMPLE_OFFSET * inverse_extent;
    float3 accumulated_color =
        blur_texture_read.SampleLevel(bloom_sampler, sample_uv + float2(-sample_offset.x, -sample_offset.y), 0).rgb;
    accumulated_color +=
        blur_texture_read.SampleLevel(bloom_sampler, sample_uv + float2(sample_offset.x, -sample_offset.y), 0).rgb;
    accumulated_color +=
        blur_texture_read.SampleLevel(bloom_sampler, sample_uv + float2(-sample_offset.x, sample_offset.y), 0).rgb;
    accumulated_color +=
        blur_texture_read.SampleLevel(bloom_sampler, sample_uv + float2(sample_offset.x, sample_offset.y), 0).rgb;
    blur_texture_write[dispatch_thread_id.xy] = float4(accumulated_color * 0.25, 1.0);
}
