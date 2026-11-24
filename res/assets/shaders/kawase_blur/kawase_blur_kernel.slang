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
 * @file kawase_blur_kernel.slang
 * @brief The Kawase blur compute kernels, whose entry points the ACP generates.
 * @details Define WBE_KAWASE_BLUR_WRITE_PING to swap the ping-pong images, and WBE_KAWASE_BLUR_OFFSET to select the tap
 * offset in source texels.
 */
#ifndef WBE_FILE_KAWASE_BLUR_KERNEL_SLANG
#define WBE_FILE_KAWASE_BLUR_KERNEL_SLANG

#include "kawase_blur/kawase_blur_utils.slang"
#include "utils.slang"

#ifndef WBE_KAWASE_BLUR_OFFSET
#define WBE_KAWASE_BLUR_OFFSET 1.5
#endif

#ifdef WBE_KAWASE_BLUR_WRITE_PING
#define WBE_KAWASE_BLUR_SOURCE_ID "kawase_blur_pong"
#define WBE_KAWASE_BLUR_DESTINATION_ID "kawase_blur_ping"
#else
#define WBE_KAWASE_BLUR_SOURCE_ID "kawase_blur_ping"
#define WBE_KAWASE_BLUR_DESTINATION_ID "kawase_blur_pong"
#endif

/**
 * @brief Write one Kawase blur iteration between two ping-pong images.
 * @details The logical resources are placeholders; alias them to graph resources with RendTaskDescVK::alias(). The default
 * variant samples kawase_blur_ping and writes kawase_blur_pong; defining WBE_KAWASE_BLUR_WRITE_PING swaps them, so a
 * multi-iteration blur alternates the two variants in one ping-pong task. The dispatch covers the destination in 16x16
 * workgroups, and the source is sampled with normalized coordinates, so the two images may differ in extent.
 * @param p_group_id The workgroup of the invocation.
 * @param p_group_thread_id The invocation's coordinate in its workgroup, swizzled into Z order.
 * @param p_source Image sampled by the blur.
 * @param p_destination Image receiving the blurred result.
 * @param p_blur_sampler Linear-filtering sampler used for every tap.
 */
[wbe::kernel(16, 16, 1)]
void kawase_blur_iteration(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind(WBE_KAWASE_BLUR_SOURCE_ID)] Texture2D<float4> p_source,
    [wbe::bind(WBE_KAWASE_BLUR_DESTINATION_ID, "write")] RWTexture2D<float4> p_destination,
    [wbe::bind("kawase_blur_sampler")] SamplerState p_blur_sampler) {
    uint destination_width;
    uint destination_height;
    p_destination.GetDimensions(destination_width, destination_height);
    uint2 dispatch_thread_id = z_order_swizzle_16x16(p_group_id.xy, p_group_thread_id.xy);
    if (dispatch_thread_id.x >= destination_width || dispatch_thread_id.y >= destination_height) {
        return;
    }
    uint source_width;
    uint source_height;
    p_source.GetDimensions(source_width, source_height);
    float2 sample_uv = (float2(dispatch_thread_id.xy) + 0.5) / float2(destination_width, destination_height);
    float2 source_inverse_extent = 1.0 / float2(source_width, source_height);
    p_destination[dispatch_thread_id.xy] =
        kawase_blur(p_source, p_blur_sampler, sample_uv, source_inverse_extent, WBE_KAWASE_BLUR_OFFSET);
}

#endif
