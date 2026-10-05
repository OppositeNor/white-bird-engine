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
    [WBE_DESCRIPTOR("storage_buffer", "buffer", "write")] uint2 buffer;
};

[vk::push_constant]
ConstantBuffer<DescriptorIndices> descriptor_indices;

// Each sub-histogram is summed by SUB_HISTOGRAM_THREAD_COUNT invocations, each owning 4 contiguous entries.
#define SUB_HISTOGRAM_THREAD_COUNT (WBE_RADIX_SORT_BUCKET_COUNT / 4)
#define WORKGROUP_SIZE 256
// The number of sub-histograms the workgroup processes at once.
#define SUB_HISTOGRAMS_PER_CHUNK (WORKGROUP_SIZE / SUB_HISTOGRAM_THREAD_COUNT)

// Computes an in-place inclusive prefix sum across the sub-histograms, separately for every bucket, so that entry b of
// sub-histogram g becomes the sum of entry b of sub-histograms [0, g]. Must be dispatched with exactly one workgroup.
[numthreads(WORKGROUP_SIZE, 1, 1)]
void main(uint3 p_group_thread_id : SV_GroupThreadID) {
    RWStructuredBuffer<uint> buffer = ResourceDescriptorHeap[descriptor_indices.buffer.x];
    uint sub_histogram_num = buffer[0];
    uint lane_start = buffer[2];
    uint group_thread_id = p_group_thread_id.x;
    // The maximum number of workgroup we have is 16384. The size of histogram is 256 entries.
    // We have 256 threads spawned, each 64 threads are responsible for doing one histogram addition, so 4 sub-histograms
    // are processed at once. We still have to split the sub-histograms into chunks, though, since 256 threads is not
    // enough for 16384 histogram summations.
    uint chunk_sub_histogram_offset = group_thread_id / SUB_HISTOGRAM_THREAD_COUNT;
    uint entry_start = (group_thread_id % SUB_HISTOGRAM_THREAD_COUNT) * 4;
    uint chunk_count = (sub_histogram_num + SUB_HISTOGRAMS_PER_CHUNK - 1) / SUB_HISTOGRAMS_PER_CHUNK;

    for (uint i = 1; i < sub_histogram_num; i <<= 1) {
        for (uint j = chunk_count; j > 0; --j) {
            // We have to start from the right side, because we're reading values from the left side, which must still
            // hold the values of the previous step.
            uint sub_histogram = (j - 1) * SUB_HISTOGRAMS_PER_CHUNK + chunk_sub_histogram_offset;
            // Sub-histograms past the end and sub-histograms with no left neighbor at this stride stay unchanged.
            bool active = sub_histogram < sub_histogram_num && sub_histogram >= i;
            uint curr_start = lane_start + sub_histogram * WBE_RADIX_SORT_BUCKET_COUNT + entry_start;
            uint left_start = curr_start - i * WBE_RADIX_SORT_BUCKET_COUNT;
            uint4 curr_region_data = uint4(0);
            if (active) {
                uint4 left_region_data = uint4(
                    buffer[left_start + 0],
                    buffer[left_start + 1],
                    buffer[left_start + 2],
                    buffer[left_start + 3]
                );
                curr_region_data = uint4(
                    buffer[curr_start + 0],
                    buffer[curr_start + 1],
                    buffer[curr_start + 2],
                    buffer[curr_start + 3]
                );
                curr_region_data += left_region_data;
            }
            // The left sub-histogram may be in the same chunk, so every read must finish before any write.
            // The buffer is device memory, so group-memory barriers would not order these accesses.
            DeviceMemoryBarrierWithGroupSync();
            if (active) {
                [unroll] for (uint k = 0; k < 4; ++k) {
                    buffer[curr_start + k] = curr_region_data[k];
                }
            }
            DeviceMemoryBarrierWithGroupSync();
        }
    }
}
