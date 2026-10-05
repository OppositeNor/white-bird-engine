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
#include "sort/radix_sort_utils.slang"

struct DescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "histogram_buffer", "read_write")] uint2 histogram_buffer;
};

[vk::push_constant]
ConstantBuffer<DescriptorIndices> descriptor_indices;

// TODO: Slang does not support globallycoherent descriptors fetched from the descriptor heap
// (https://github.com/shader-slang/slang/issues/13440), so under the Vulkan memory model, buffer accesses are private and
// barriers do not make them visible to other invocations. The invocations therefore never exchange values through the
// buffer.
// One invocation per bucket.
#define WORKGROUP_SIZE WBE_RADIX_SORT_BUCKET_COUNT
// The number of sub-histogram entries each invocation loads before summing them, so that the loads overlap.
#define ROWS_PER_BATCH 8

// Computes an in-place inclusive prefix sum across the sub-histograms, separately for every bucket, so that entry b of
// sub-histogram g becomes the sum of entry b of sub-histograms [0, g]. Invocation b owns bucket b and walks its entries
// from the first sub-histogram to the last with a running sum, so no invocation reads an entry another one writes; the
// invocations never communicate, which needs no shader-level memory synchronization. The invocations of a batch access
// adjacent entries of one sub-histogram at a time. Must be dispatched with exactly one workgroup.
[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void main(uint3 p_group_thread_id : SV_GroupThreadID) {
    RWStructuredBuffer<uint> buffer = ResourceDescriptorHeap[descriptor_indices.histogram_buffer.x];
    uint sub_histogram_num = buffer[0];
    uint lane_start = buffer[2];
    uint bucket = p_group_thread_id.x;
    uint running_sum = 0;
    for (uint batch_start = 0; batch_start < sub_histogram_num; batch_start += ROWS_PER_BATCH) {
        uint values[ROWS_PER_BATCH];
        [unroll] for (uint i = 0; i < ROWS_PER_BATCH; ++i) {
            uint sub_histogram = batch_start + i;
            values[i] = sub_histogram < sub_histogram_num
                ? buffer[lane_start + sub_histogram * WBE_RADIX_SORT_BUCKET_COUNT + bucket]
                : 0;
        }
        [unroll] for (uint i = 0; i < ROWS_PER_BATCH; ++i) {
            uint sub_histogram = batch_start + i;
            running_sum += values[i];
            if (sub_histogram < sub_histogram_num) {
                buffer[lane_start + sub_histogram * WBE_RADIX_SORT_BUCKET_COUNT + bucket] = running_sum;
            }
        }
    }
}
