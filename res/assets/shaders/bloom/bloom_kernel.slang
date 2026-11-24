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
 * @file bloom_kernel.slang
 * @brief The bloom compute kernels, whose entry points the ACP generates.
 */
#ifndef WBE_FILE_BLOOM_KERNEL_SLANG
#define WBE_FILE_BLOOM_KERNEL_SLANG

#include "utils.slang"

static const float BLOOM_THRESHOLD = 1.0;
static const float BLOOM_THRESHOLD_EPSILON = 0.0001;

/**
 * @brief Extract the bright part of an image for bloom, one invocation per output texel.
 * @details Each texel keeps the fraction of its color whose luminance exceeds BLOOM_THRESHOLD.
 * @param p_group_id The workgroup of the invocation.
 * @param p_group_thread_id The invocation's coordinate in its workgroup, swizzled into Z order.
 * @param p_bloom_input The image to extract from.
 * @param p_bloom_output Receives the extracted bright part; its extent bounds the dispatch.
 * @param p_bloom_sampler The sampler the input is read with.
 */
[wbe::kernel(16, 16, 1)]
void bloom_extract(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("result_image")] Texture2D<float4> p_bloom_input,
    [wbe::bind("bloom_ping")] RWTexture2D<float4> p_bloom_output,
    [wbe::bind("bloom_sampler")] SamplerState p_bloom_sampler) {
    uint width;
    uint height;
    p_bloom_output.GetDimensions(width, height);
    uint2 dispatch_thread_id = z_order_swizzle_16x16(p_group_id.xy, p_group_thread_id.xy);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) {
        return;
    }
    float2 sample_uv = (float2(dispatch_thread_id.xy) + 0.5) / float2(width, height);
    float3 sample_color = p_bloom_input.SampleLevel(p_bloom_sampler, sample_uv, 0).rgb;
    float brightness = dot(sample_color, float3(0.2126, 0.7152, 0.0722));
    float contribution = max(brightness - BLOOM_THRESHOLD, 0.0) / max(brightness, BLOOM_THRESHOLD_EPSILON);
    p_bloom_output[dispatch_thread_id.xy] = float4(sample_color * contribution, 1.0);
}

#endif
