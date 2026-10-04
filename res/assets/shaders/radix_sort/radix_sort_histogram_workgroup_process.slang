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
    uint group_id = p_group_id.x;
    uint total_group_task_num = max((buffer_info.element_count + 63) / 64, 1);
    if (group_id > total_group_task_num) {
        return;
    }
    uint total_group_num = min(WBE_RADIX_SORT_MAX_WORKGROUP_COUNT, total_group_task_num);

    uint group_thread_id = p_group_thread_id.x;
    // The starting position of a thread in a 256 chunk.
    uint thread_data_start = group_thread_id * 4;

    RWStructuredBuffer<uint> buffer = decriptor_indices.buffer.x;
    RWStructuredBuffer<uint> output_buffer = decriptor_indices.buffer.x;
    ConstantBuffer<BufferInfo> buffer_info = decriptor_indices.buffer_info.x;
    if (p_group_id.x == 0 && group_thread_id == 0) {
        // The first entry of output_buffer stores the number of workgroups.
        output_buffer[0] = total_group_num;
    }
    groupshared uint histogram[WBE_RADIX_SORT_BUCKET_SIZE + 1];
    [unroll] for (uint i = 0; i < 4; ++i) {
        // Initialize the original value in the histogram to 0.
        histogram[thread_data_start + i] = 0;
    }
    if (group_thread_id == 0) {
        // An extra is filled for zero clampping
        histogram[WBE_RADIX_SORT_BUCKET_SIZE] = 0;
    }

    uint group_data_stride = total_group_num * 64;
    uint group_data_offset = p_group_id.x * 64;
    for (uint i = 0; i < max(total_group_task_num / total_group_num, 1)) {
        // Each group holds a shared memory to calculate workgroup_scope_radix_sort_histogram
        workgroup_scope_radix_sort_histogram<uint, 64>(
            buffer,
            group_data_stride * i + group_data_offset,
            buffer_info.element_count,
            histogram,
            group_thread_id,
            WBE_RADIX_HISTOGRAM_BIT_REGION);
    }
    // Calculate the prefix sum of the invocation's subgroup.
    workgroup_inclusive_prefix_sum_64_256(histogram, thread_data_start);

    if (group_thread_id == 0) {
        // The first thread is responsible for writing its data to the output buffer's corresponding session.
        // Write it to the region after all the buckets.
        output_buffer[1 + total_group_num * WBE_RADIX_SORT_BUCKET_SIZE + group_id] = histogram[WBE_RADIX_SORT_BUCKET_SIZE - 1]
    }
    // Write the workgroup-wise prefix sum into the curresponding region
    [unroll] for (uint i = 0; i < 4; ++i) {
        // The first entry of output_buffer is the number of workgroups, and histogram's
        // index should - 1 because we're storing exclusive prefix sum.
        output_buffer[1 + thread_data_start + i] = histogram[min(thread_data_start + i - 1, WBE_RADIX_SORT_BUCKET_SIZE)];
    }
}
