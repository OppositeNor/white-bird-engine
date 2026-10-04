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
#ifndef WBE_FILE_SORT_SLANG
#define WBE_FILE_SORT_SLANG

#include "utils/utils.slang"

#define WBE_RADIX_SORT_BUCKET_BIT_SIZE 8
#define WBE_RADIX_SORT_BUCKET_COUNT (1 << WBE_RADIX_SORT_BUCKET_BIT_SIZE)
#define WBE_RADIX_SORT_BUCKET_MASK (WBE_RADIX_SORT_BUCKET_COUNT - 1)
#define WBE_RADIX_SORT_MAX_WORKGROUP_COUNT 16384

/**
 * @brief Accumulates the radix sort digit histogram of up to one element per invocation.
 * @details Invocation p_invocation_id counts the digit of element p_data_start + p_invocation_id; invocations with an
 * index at or past p_data_count contribute nothing. The counts are added to the existing histogram values, which must
 * be visible to the workgroup. The function does not synchronize after counting, so callers must issue a workgroup
 * barrier before reading the histogram.
 * @tparam T The integer element type.
 * @param p_data The buffer whose elements are counted.
 * @param p_data_start The index of the first element of p_data to count.
 * @param p_data_count The number of elements to count, starting from p_data_start. Must not exceed the workgroup size.
 * @param p_histogram The groupshared histogram the counts are added to, with one counter per digit value.
 * @param p_invocation_id The local invocation index.
 * @param p_bit_region The bit offset of the WBE_RADIX_SORT_BUCKET_BIT_SIZE-bit digit to count.
 */
void workgroup_scope_per_thread_radix_sort_histogram<T : IInteger>(RWStructuredBuffer<T> p_data, uint p_data_start,
        uint p_data_count, __ref groupshared uint p_histogram[WBE_RADIX_SORT_BUCKET_COUNT], uint p_invocation_id,
        uint p_bit_region) {
    if (p_invocation_id >= p_data_count) {
        return;
    }
    uint bucket = p_data[p_data_start + p_invocation_id].shr(int(p_bit_region)).toUInt() & WBE_RADIX_SORT_BUCKET_MASK;
    InterlockedAdd(p_histogram[bucket], 1);
}

/**
 * @brief Accumulates the radix sort digit histogram of a buffer range within one workgroup.
 * @details Adds the digit count of every element in [p_data_start, p_data_start + p_data_count) to the existing
 * histogram values without clearing them, so a histogram can be built incrementally over several calls. Callers must
 * initialize the histogram, typically to zero, before the first call. When there are fewer elements than invocations,
 * each invocation counts at most one element and the remaining invocations are idle; otherwise each invocation counts a
 * contiguous section of that range. Every invocation of the workgroup must call this function with the same buffer,
 * start offset, element count, histogram, and bit region. The function synchronizes before counting, so histogram
 * writes made by any invocation before the call are visible, and after counting, so the histogram is visible to the
 * whole workgroup on return.
 * @tparam T The integer element type.
 * @tparam WORKGROUP_SIZE The number of invocations in the workgroup.
 * @param p_data The buffer whose elements are counted.
 * @param p_data_start The index of the first element of p_data to count.
 * @param p_data_count The number of elements to count, starting from p_data_start.
 * @param p_histogram The groupshared histogram the counts are added to, with one counter per digit value.
 * @param p_group_thread_id The local invocation index in [0, WORKGROUP_SIZE).
 * @param p_bit_region The bit offset of the WBE_RADIX_SORT_BUCKET_BIT_SIZE-bit digit to count.
 */
void workgroup_scope_radix_sort_histogram<T : IInteger, let WORKGROUP_SIZE : uint>(RWStructuredBuffer<T> p_data,
        uint p_data_start, uint p_data_count, __ref groupshared uint p_histogram[WBE_RADIX_SORT_BUCKET_COUNT],
        uint p_group_thread_id, uint p_bit_region) {
    GroupMemoryBarrierWithGroupSync();
    // If we have less than workgroup size elements, we assign each thread to an element and discard the overflowed threads.
    if (WORKGROUP_SIZE > p_data_count) {
        workgroup_scope_per_thread_radix_sort_histogram(p_data, p_data_start, p_data_count, p_histogram, p_group_thread_id,
            p_bit_region);
    } else {
        // Each invocation is responsible for a contiguous section of the buffer.
        const uint operate_stride = (p_data_count + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
        const uint section_begin = p_group_thread_id * operate_stride;
        const uint section_end = min(section_begin + operate_stride, p_data_count);
        for (uint i = section_begin; i < section_end; ++i) {
            uint bucket = p_data[p_data_start + i].shr(int(p_bit_region)).toUInt() & WBE_RADIX_SORT_BUCKET_MASK;
            InterlockedAdd(p_histogram[bucket], 1);
        }
    }
    GroupMemoryBarrierWithGroupSync();
}

#endif
