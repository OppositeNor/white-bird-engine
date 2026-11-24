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
 * @file svogi_radiance_sample.slang
 * @brief Test kernel that runs try_locate_svogi_subvoxel() and make_svogi_subvoxel_radiance() once per world-space flux
 * sample, as the SVOGI radiance injection does for each shadow-map texel.
 * @details Dispatch one thread per position; no defines are required. Each result is (located, key), each sub-voxel
 * position is written in xyz, and the three rows of each radiance matrix are written in xyz of three consecutive elements.
 */
#include "svogi/svogi_utils.slang"

StructuredBuffer<SVONode> nodes;
StructuredBuffer<SVOLeaf> leaves;
StructuredBuffer<SVOInfo> tree_info;
StructuredBuffer<float4> positions;
StructuredBuffer<float4> fluxes;
RWStructuredBuffer<uint2> results;
RWStructuredBuffer<float4> subvoxel_positions;
RWStructuredBuffer<float4> radiance_rows;

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    uint position_count;
    uint position_stride;
    positions.GetDimensions(position_count, position_stride);
    uint index = p_dispatch_thread_id.x;
    if (index >= position_count) {
        return;
    }
    SVO svo = SVO(nodes, leaves, tree_info[0], true);
    uint key;
    float3 subvoxel_position;
    bool located = try_locate_svogi_subvoxel(svo, positions[index].xyz, key, subvoxel_position);
    results[index] = uint2(located ? 1 : 0, key);
    subvoxel_positions[index] = float4(subvoxel_position, 0.0);
    float3x3 radiance = make_svogi_subvoxel_radiance(subvoxel_position, fluxes[index].xyz);
    [unroll] for (uint row = 0; row < 3; ++row) {
        radiance_rows[index * 3 + row] = float4(radiance[row], 0.0);
    }
}
