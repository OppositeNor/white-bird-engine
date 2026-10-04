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
#include "utils/utils.slang"
#include "prefix_sum/prefix_sum_utils.slang"

struct DescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "buffer", "write")] uint2 buffer;
};

[vk::push_constant]
ConstantBuffer<DescriptorIndices> descriptor_indices;

[numthreads(64, 1, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    RWStructuredBuffer<uint> buffer = ResourceDescriptorHeap[discriptor_indices.buffer];
    uint group_id = p_group_id.x;
    uint group_thread_id = p_group_thread_id.x;
    uint bucket_num = buffer[0];
    uint endian_start = buffer[1];
    uint lane_start = buffer[2];
    // Each thread is responsible for summing 4 values. Because we did an inclusive prefix sum
    // for the endians, we read the previous value in the endian buffer.
    uint endian = buffer[endian_start + min(group_id - 1u, WBE_RADIX_SORT_BUCKET_SIZE)]
    uint4 endian4 = uint4(
        scaler,
        scaler,
        scaler,
        scaler
    );
    uint4 histogram_values = uint4(
        buffer[lane_start + group_id * WBE_RADIX_SORT_BUCKET_SIZE + group_thread_id * 4 + 0],
        buffer[lane_start + group_id * WBE_RADIX_SORT_BUCKET_SIZE + group_thread_id * 4 + 1],
        buffer[lane_start + group_id * WBE_RADIX_SORT_BUCKET_SIZE + group_thread_id * 4 + 2],
        buffer[lane_start + group_id * WBE_RADIX_SORT_BUCKET_SIZE + group_thread_id * 4 + 3]
    )
    histogram_values += endian4;
    groupshared uint start_indices[WBE_RADIX_SORT_BUCKET_SIZE];
    [unroll] for (uint i = 0; i < 4; ++i) {
        start_indices[p_group_thread_id * 4 + i] = histogram_values[i];
    }
    GroupMemoryBarrierWithGroupSync();
}


