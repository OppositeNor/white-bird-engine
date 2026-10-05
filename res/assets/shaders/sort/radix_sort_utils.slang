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
#include "prefix_sum/prefix_sum_utils.slang"

#define WBE_RADIX_SORT_BUCKET_BIT_SIZE 8
#define WBE_RADIX_SORT_BUCKET_COUNT (1 << WBE_RADIX_SORT_BUCKET_BIT_SIZE)
#define WBE_RADIX_SORT_BUCKET_MASK (WBE_RADIX_SORT_BUCKET_COUNT - 1)
#define WBE_RADIX_SORT_MAX_WORKGROUP_COUNT 16384
// Workgroup radix sorting packs one counter per digit value into a uint, so its digit is narrower than the bucket digit.
#define WBE_RADIX_SORT_WG_BIT_SIZE 2
#define WBE_RADIX_SORT_WG_BUCKET_COUNT (1 << WBE_RADIX_SORT_WG_BIT_SIZE)
#define WBE_RADIX_SORT_WG_BUCKET_MASK (WBE_RADIX_SORT_WG_BUCKET_COUNT - 1)
#define WBE_RADIX_SORT_WG_COUNTER_BIT_SIZE (32 / WBE_RADIX_SORT_WG_BUCKET_COUNT)
#define WBE_RADIX_SORT_WG_COUNTER_MASK ((1u << WBE_RADIX_SORT_WG_COUNTER_BIT_SIZE) - 1)

// The number of elements each radix sort workgroup aims to own, matching the scatter batch size.
#define WBE_RADIX_SORT_ELEMENTS_PER_WORKGROUP 256
// The first entries of the radix sort key buffers are metadata; the first one is the element count.
#define WBE_RADIX_SORT_DATA_START 4

/**
 * @brief Get the number of sub-histograms, and so of histogram and scatter workgroups, of a radix sort pass.
 * @details Host code dispatching the passes must use the same formula.
 * @param p_element_count The number of keys sorted.
 * @return min(WBE_RADIX_SORT_MAX_WORKGROUP_COUNT, max(ceil(p_element_count / WBE_RADIX_SORT_ELEMENTS_PER_WORKGROUP), 1)).
 */
uint radix_sort_sub_histogram_num(uint p_element_count) {
    uint group_num = (p_element_count + WBE_RADIX_SORT_ELEMENTS_PER_WORKGROUP - 1) / WBE_RADIX_SORT_ELEMENTS_PER_WORKGROUP;
    return clamp(group_num, 1u, uint(WBE_RADIX_SORT_MAX_WORKGROUP_COUNT));
}

/**
 * @brief Get the WBE_RADIX_SORT_BUCKET_BIT_SIZE-bit digit of a key.
 * @param p_key The key.
 * @param p_bit_region The bit offset of the digit.
 * @return The digit, in [0, WBE_RADIX_SORT_BUCKET_COUNT).
 */
uint radix_sort_digit(uint p_key, uint p_bit_region) {
    return (p_key >> p_bit_region) & WBE_RADIX_SORT_BUCKET_MASK;
}

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

/**
 * @brief Stably sorts a 256-element groupshared array in place by a WBE_RADIX_SORT_WG_BIT_SIZE-bit digit with a
 * 64-invocation workgroup.
 * @details Invocation p_thread_id owns the 4 contiguous elements starting at 4 * p_thread_id. Each element is marked in
 * p_scratch with one WBE_RADIX_SORT_WG_COUNTER_BIT_SIZE-bit counter per digit value packed into a uint, and an inclusive
 * prefix sum turns the marks into per-element digit counts. Each element reads the counts of the entry before it as its
 * rank among the elements with the same digit, and is placed after every element with a smaller digit. Elements with
 * equal digits keep their relative order. A packed counter can overflow only when all 256 elements have the same digit,
 * in which case every element keeps its position. Every invocation of the 64-invocation workgroup must call this function
 * with the same arrays and bit offset. The function does not synchronize before its first read, so callers must issue a
 * workgroup barrier after filling p_data; it synchronizes after the final write, so the sorted array is visible to the
 * whole workgroup on return.
 * @param p_data The groupshared array to sort in place.
 * @param p_bit_start The bit offset of the WBE_RADIX_SORT_WG_BIT_SIZE-bit digit to sort by; in
 * [0, 32 - WBE_RADIX_SORT_WG_BIT_SIZE].
 * @param p_thread_id The local invocation index in [0, 64).
 * @param p_scratch Scratch array of 256 elements followed by one element that the function sets to zero; its contents are
 * overwritten.
 */
void workgroup_radix_sort_step_64_256(__ref groupshared uint p_data[256],
        uint p_bit_start, uint p_thread_id, __ref groupshared uint p_scratch[256 + 1]) {
    if (p_thread_id == 0) {
        p_scratch[256] = 0;
    }
    uint4 values;
    uint4 slots;
    // For each location in p_data, we mark the corresponding value as 1
    [unroll] for (uint i = 0; i < 4; ++i) {
        values[i] = p_data[p_thread_id * 4 + i];
        slots[i] = (values[i] >> p_bit_start) & WBE_RADIX_SORT_WG_BUCKET_MASK;
        p_scratch[p_thread_id * 4 + i] = 1 << (slots[i] * WBE_RADIX_SORT_WG_COUNTER_BIT_SIZE);
    }
    GroupMemoryBarrierWithGroupSync();
    // Perform the prefix sum, to transform each entry of it to the histogram till the corresponding point.
    workgroup_inclusive_prefix_sum_64_256(p_scratch, p_thread_id * 4);
    // The last element is the workgroup histogram.
    uint histogram = p_scratch[255];
    // Exclusive prefix sum of the packed histogram
    uint slot_starts = 0;
    [unroll] for (uint j = 1; j < WBE_RADIX_SORT_WG_BUCKET_COUNT; ++j) {
        slot_starts += histogram << (j * WBE_RADIX_SORT_WG_COUNTER_BIT_SIZE);
    }
    // Move each entry one slot to the right, so that it holds the histogram before its element. The first entry reads
    // the zero at p_scratch[256] through the unsigned underflow.
    uint4 positions;
    [unroll] for (uint i = 0; i < 4; ++i) {
        uint rank = (p_scratch[min(p_thread_id * 4 + i - 1u, 256)] >> (slots[i] * WBE_RADIX_SORT_WG_COUNTER_BIT_SIZE))
            & WBE_RADIX_SORT_WG_COUNTER_MASK;
        uint slot_start = (slot_starts >> (slots[i] * WBE_RADIX_SORT_WG_COUNTER_BIT_SIZE)) & WBE_RADIX_SORT_WG_COUNTER_MASK;
        positions[i] = slot_start + rank;
    }
    // Every invocation has read its values before any of them is overwritten.
    GroupMemoryBarrierWithGroupSync();
    [unroll] for (uint i = 0; i < 4; ++i) {
        p_data[positions[i]] = values[i];
    }
    GroupMemoryBarrierWithGroupSync();
}

/**
 * @brief Sorts a 256-element groupshared array of uint keys in place in ascending order with a 64-invocation workgroup.
 * @details Runs workgroup_radix_sort_step_64_256 over every WBE_RADIX_SORT_WG_BIT_SIZE-bit digit, from the least
 * significant one up. Every invocation of the 64-invocation workgroup must call this function with the same arrays.
 * Callers must issue a workgroup barrier after filling p_data; the sorted array is visible to the whole workgroup on
 * return.
 * @param p_data The groupshared array to sort in place.
 * @param p_thread_id The local invocation index in [0, 64).
 * @param p_scratch Scratch array of 256 elements followed by one element; its contents are overwritten.
 */
void workgroup_radix_sort_64_256(__ref groupshared uint p_data[256], uint p_thread_id,
        __ref groupshared uint p_scratch[256 + 1]) {
    [unroll] for (uint i = 0; i < 32 / WBE_RADIX_SORT_WG_BIT_SIZE; ++i) {
        workgroup_radix_sort_step_64_256(p_data, i * WBE_RADIX_SORT_WG_BIT_SIZE, p_thread_id, p_scratch);
    }
}

#endif
