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
#include "radix_sort/radix_sort_utils.slang"

struct BufferInfo {
    uint element_count;
};

struct DescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "buffer")] uint2 buffer;
    [WBE_DESCRIPTOR("storage_buffer", "buffer")] uint2 output_buffer;
    [WBE_DESCRIPTOR("uniform_buffer", "buffer_info")] uint2 buffer_info;
};

[vk::push_constant]
ConstantBuffer<DescriptorIndices> decriptor_indices;

[numthreads(64, 1, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    RWStructuredBuffer<uint> buffer = decriptor_indices.buffer.x;
    RWStructuredBuffer<uint4> output_buffer = decriptor_indices.buffer.x;
    ConstantBuffer<BufferInfo> buffer_info = decriptor_indices.buffer_info.x;
    // The first entry of output_buffer stores the metadata of the histogram.
    if (p_group_id.x == 0 && p_group_thread_id.x == 0) {
        output_buffer[0] = uint4(arena_count, 0, 0, 0);
    }
    uint data_start = p_group_id.x * 64;
    groupshared uint histogram[WBE_RADIX_SORT_BUCKET_SIZE];
    // Each group holds a shared memory to calculate workgroup_scope_radix_sort_histogram
    workgroup_scope_radix_sort_histogram<uint, 64>(buffer, data_start, buffer_info.element_count, histogram, p_group_thread_id.x, BIT_REGION);
    // Then write the output buffer correspondingly.
    uint workgroup_output_region = WBE_RADIX_SORT_BUCKET_COUNT * p_group_id.x;
    uint thread_workgroup_portion = p_group_thread_id.x * 4;
    // We first pack it into a uint4 to perform SIMD write.
    uint4 result = uint4(
            histogram[thread_workgroup_portion],
            histogram[thread_workgroup_portion + 1],
            histogram[thread_workgroup_portion + 2],
            histogram[thread_workgroup_portion + 3]);
    output_buffer[(workgroup_output_region + thread_workgroup_portion) / 4 + 1] = result;
}
