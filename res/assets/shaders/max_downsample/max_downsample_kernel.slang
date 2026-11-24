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
 * @file max_downsample_kernel.slang
 * @brief The max-downsample compute kernels, whose entry points the ACP generates.
 */
#ifndef WBE_FILE_MAX_DOWNSAMPLE_KERNEL_SLANG
#define WBE_FILE_MAX_DOWNSAMPLE_KERNEL_SLANG

#include "max_downsample/max_downsample_utils.slang"
#include "utils.slang"

/**
 * @brief Write one mip level as the per-channel maximum of each 2x2 region of the previous level.
 * @details RTMaxDownsampleVK binds the logical resources itself for every generated mip level: max_downsample_source is a
 * single-level view of the previous mip, and max_downsample_destination is a single-level view of the generated mip. The
 * dispatch covers the destination in 16x16 workgroups. See max_downsample() for the footprint of odd source extents.
 * @param p_group_id The workgroup of the invocation.
 * @param p_group_thread_id The invocation's coordinate in its workgroup, swizzled into Z order.
 * @param p_source Previous mip level, read with texel loads.
 * @param p_destination Generated mip level.
 */
[wbe::kernel(16, 16, 1)]
void max_downsample_level(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("max_downsample_source")] Texture2D<float4> p_source,
    [wbe::bind("max_downsample_destination", "write")] RWTexture2D<float4> p_destination) {
    uint2 destination_extent;
    p_destination.GetDimensions(destination_extent.x, destination_extent.y);
    uint2 dispatch_thread_id = z_order_swizzle_16x16(p_group_id.xy, p_group_thread_id.xy);
    if (dispatch_thread_id.x >= destination_extent.x || dispatch_thread_id.y >= destination_extent.y) {
        return;
    }
    uint2 source_extent;
    p_source.GetDimensions(source_extent.x, source_extent.y);
    p_destination[dispatch_thread_id] = max_downsample(p_source, source_extent, destination_extent, dispatch_thread_id);
}

#endif
