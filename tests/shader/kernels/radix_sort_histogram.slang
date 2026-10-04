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
/**
 * @file radix_sort_histogram.slang
 * @brief Test kernel that runs workgroup_scope_radix_sort_histogram on one workgroup and writes the histogram back.
 * @details Required defines: ELEMENT_TYPE, KERNEL_WORKGROUP_SIZE, DATA_START, DATA_COUNT, BIT_REGION, and
 * HISTOGRAM_GARBAGE, the value the groupshared histogram holds before the call so that a missing clear is observable.
 */
#include "radix_sort/radix_sort_utils.slang"

RWStructuredBuffer<ELEMENT_TYPE> data;
RWStructuredBuffer<uint> histogram_output;
groupshared uint histogram[WBE_RADIX_SORT_BUCKET_SIZE];

[shader("compute")]
[numthreads(KERNEL_WORKGROUP_SIZE, 1, 1)]
void main(uint3 p_group_thread_id : SV_GroupThreadID) {
    for (uint i = p_group_thread_id.x; i < WBE_RADIX_SORT_BUCKET_SIZE; i += KERNEL_WORKGROUP_SIZE) {
        histogram[i] = HISTOGRAM_GARBAGE;
    }
    GroupMemoryBarrierWithGroupSync();
    workgroup_scope_radix_sort_histogram<ELEMENT_TYPE, KERNEL_WORKGROUP_SIZE>(data, DATA_START, DATA_COUNT, histogram,
        p_group_thread_id.x, WBE_RADIX_HISTOGRAM_BIT_REGION);
    // Each invocation writes back counters accumulated by other invocations, which relies on the trailing barrier.
    for (uint i = p_group_thread_id.x; i < WBE_RADIX_SORT_BUCKET_SIZE; i += KERNEL_WORKGROUP_SIZE) {
        histogram_output[i] = histogram[i];
    }
}
