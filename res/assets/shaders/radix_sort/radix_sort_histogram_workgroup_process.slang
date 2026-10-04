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
    RWStructuredBuffer<uint> output_buffer = decriptor_indices.buffer.x;
    ConstantBuffer<BufferInfo> buffer_info = decriptor_indices.buffer_info.x;
    uint group_thread_id = p_group_thread_id.x;
    uint group_id = p_group_id.x;
    uint group_num = (buffer_info.element_count + 63) / 64;
    if (p_group_id.x == 0 && group_thread_id == 0) {
        // The first entry of output_buffer stores the number of workgroups.
        output_buffer[0] = group_num;
    }
    uint data_start = p_group_id.x * 64;
    // An extra is filled for zero clampping
    groupshared uint histogram[WBE_RADIX_SORT_BUCKET_SIZE + 1];
    if (group_thread_id == 0) {
        histogram[WBE_RADIX_SORT_BUCKET_SIZE] = 0;
    }
    // Each group holds a shared memory to calculate workgroup_scope_radix_sort_histogram
    workgroup_scope_radix_sort_histogram<uint, 64>(buffer, data_start, buffer_info.element_count, histogram, group_thread_id, BIT_REGION);
    uint thread_data_start = group_thread_id * 4;
    // Calculate the prefix sum of the invocation's subgroup.
    workgroup_inclusive_prefix_sum_64_256(histogram, thread_data_start);

    if (group_thread_id == 0) {
        // The first thread is responsible for writing its data to the output buffer's corresponding session.
        // Write it to the region after all the buckets.
        output_buffer[1 + group_num * WBE_RADIX_SORT_BUCKET_SIZE + group_id] = histogram[WBE_RADIX_SORT_BUCKET_SIZE - 1]
    }
    // Write the workgroup-wise prefix sum into the curresponding region
    [unroll] for (uint i = 0; i < 4; ++i) {
        // The first entry of output_buffer is the number of workgroups, and histogram's
        // index should - 1 because we're storing exclusive prefix sum.
        output_buffer[1 + thread_data_start + i] = histogram[min(thread_data_start + i - 1, WBE_RADIX_SORT_BUCKET_SIZE)];
    }
}
