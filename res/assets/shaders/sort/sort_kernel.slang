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
 * @file sort_kernel.slang
 * @brief The radix sort compute kernels, whose entry points the ACP generates; their building blocks are in
 * radix_sort_utils.slang and radix_sort_uint2_utils.slang.
 * @details The kernels that sort by one digit (the workgroup histograms and the scatters) exist only when
 * WBE_RADIX_HISTOGRAM_BIT_REGION, the first bit of the digit, is defined, so a resource selecting one without it fails.
 * radix_sort_histogram_workgroup_process reads the key buffer through two views, so it binds input_buffer twice.
 */
#ifndef WBE_FILE_SORT_KERNEL_SLANG
#define WBE_FILE_SORT_KERNEL_SLANG

#include "utils/utils.slang"
#include "sort/radix_sort_utils.slang"
#include "sort/radix_sort_uint2_utils.slang"

// The workgroup size of the uint key histogram and scatter kernels.
#define WBE_RADIX_SORT_KEY_WORKGROUP_SIZE 64
// The number of elements radix_sort_scatter sorts in groupshared memory at once.
#define WBE_RADIX_SORT_SCATTER_BATCH_SIZE 256

groupshared uint radix_sort_histogram_prefix_sum_histograms[WBE_RADIX_SORT_SCAN_ENTRY_NUM];

/**
 * @brief First step of the global prefix sum of the sub-histograms.
 * @details Workgroup w scans the WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM sub-histograms starting at
 * w * WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM in groupshared memory, so that entry b of each becomes the sum of entry b over the
 * earlier sub-histograms of the same scan group and itself. Sub-histograms past the last one count as zero. The last
 * scanned entry of every bucket is the total of the scan group, written to overflow histogram w at
 * overflow_start + w * WBE_RADIX_SORT_BUCKET_COUNT. radix_sort_histogram_overflow_prefix_sum and
 * radix_sort_histogram_add_overflow then add the totals of the earlier scan groups. Must be dispatched with the histogram
 * prefix scan dispatch: ceil(sub_histogram_num / WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM) workgroups. One invocation per bucket.
 * @param p_group_id The workgroup, whose x is the scan group.
 * @param p_group_thread_id The invocation in its workgroup, whose x is the bucket.
 * @param p_histogram_buffer The histogram buffer, with its metadata, sub-histograms, and overflow histograms.
 */
[wbe::kernel(WBE_RADIX_SORT_BUCKET_COUNT)]
void radix_sort_histogram_prefix_sum(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("histogram_buffer")] RWStructuredBuffer<uint> p_histogram_buffer) {
    RWStructuredBuffer<uint> buffer = p_histogram_buffer;
    uint sub_histogram_num = buffer[WBE_RADIX_SORT_SUB_HISTOGRAM_NUM_INDEX];
    uint overflow_start = buffer[WBE_RADIX_SORT_OVERFLOW_START_INDEX];
    uint lane_start = buffer[WBE_RADIX_SORT_LANE_START_INDEX];
    uint group_id = p_group_id.x;
    uint bucket = p_group_thread_id.x;
    uint first_sub_histogram = group_id * WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM;
    [unroll] for (uint i = 0; i < WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM; ++i) {
        uint sub_histogram = first_sub_histogram + i;
        radix_sort_histogram_prefix_sum_histograms[bucket + i * WBE_RADIX_SORT_BUCKET_COUNT] =
            sub_histogram < sub_histogram_num
            ? buffer[lane_start + sub_histogram * WBE_RADIX_SORT_BUCKET_COUNT + bucket]
            : 0;
    }
    GroupMemoryBarrierWithGroupSync();
    workgroup_inclusive_histogram_scan(radix_sort_histogram_prefix_sum_histograms, bucket);
    [unroll] for (uint i = 0; i < WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM; ++i) {
        uint sub_histogram = first_sub_histogram + i;
        if (sub_histogram < sub_histogram_num) {
            buffer[lane_start + sub_histogram * WBE_RADIX_SORT_BUCKET_COUNT + bucket] =
                radix_sort_histogram_prefix_sum_histograms[bucket + i * WBE_RADIX_SORT_BUCKET_COUNT];
        }
    }
    buffer[overflow_start + group_id * WBE_RADIX_SORT_BUCKET_COUNT + bucket] = radix_sort_histogram_prefix_sum_histograms[
        bucket + (WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM - 1) * WBE_RADIX_SORT_BUCKET_COUNT];
}

groupshared uint radix_sort_histogram_overflow_prefix_sum_histograms[WBE_RADIX_SORT_SCAN_ENTRY_NUM];

/**
 * @brief Second step of the global prefix sum of the sub-histograms.
 * @details Computes the inclusive prefix sum across the overflow histograms from radix_sort_histogram_prefix_sum, separately
 * for every bucket, so that entry b of overflow histogram w becomes the sum of entry b of every sub-histogram of scan
 * groups [0, w]. The overflow histograms are scanned in segments of WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM in groupshared memory,
 * and each segment adds the running total of the earlier ones. Must be dispatched with exactly one workgroup. One
 * invocation per bucket.
 * @param p_group_thread_id The invocation in its workgroup, whose x is the bucket.
 * @param p_histogram_buffer The histogram buffer, with its metadata, sub-histograms, and overflow histograms.
 */
[wbe::kernel(WBE_RADIX_SORT_BUCKET_COUNT)]
void radix_sort_histogram_overflow_prefix_sum(uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("histogram_buffer")] RWStructuredBuffer<uint> p_histogram_buffer) {
    RWStructuredBuffer<uint> buffer = p_histogram_buffer;
    uint sub_histogram_num = buffer[WBE_RADIX_SORT_SUB_HISTOGRAM_NUM_INDEX];
    uint overflow_start = buffer[WBE_RADIX_SORT_OVERFLOW_START_INDEX];
    uint overflow_num = (sub_histogram_num + WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM - 1) / WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM;
    uint bucket = p_group_thread_id.x;
    // The sum of this bucket over the overflow histograms of the earlier segments.
    uint running_sum = 0;
    for (uint segment_start = 0; segment_start < overflow_num; segment_start += WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM) {
        [unroll] for (uint i = 0; i < WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM; ++i) {
            uint overflow = segment_start + i;
            radix_sort_histogram_overflow_prefix_sum_histograms[bucket + i * WBE_RADIX_SORT_BUCKET_COUNT] =
                overflow < overflow_num
                ? buffer[overflow_start + overflow * WBE_RADIX_SORT_BUCKET_COUNT + bucket]
                : 0;
        }
        GroupMemoryBarrierWithGroupSync();
        workgroup_inclusive_histogram_scan(radix_sort_histogram_overflow_prefix_sum_histograms, bucket);
        [unroll] for (uint i = 0; i < WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM; ++i) {
            uint overflow = segment_start + i;
            if (overflow < overflow_num) {
                buffer[overflow_start + overflow * WBE_RADIX_SORT_BUCKET_COUNT + bucket] = running_sum
                    + radix_sort_histogram_overflow_prefix_sum_histograms[bucket + i * WBE_RADIX_SORT_BUCKET_COUNT];
            }
        }
        running_sum += radix_sort_histogram_overflow_prefix_sum_histograms[
            bucket + (WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM - 1) * WBE_RADIX_SORT_BUCKET_COUNT];
        // Every invocation has read the scanned segment before the next one overwrites it.
        GroupMemoryBarrierWithGroupSync();
    }
}

/**
 * @brief Last step of the global prefix sum of the sub-histograms.
 * @details Workgroup g adds to sub-histogram g the exclusive prefix sum of the overflow histograms at its scan group
 * w = g / WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM: the inclusive one of overflow histogram w - 1 from
 * radix_sort_histogram_overflow_prefix_sum, or 0 for the first scan group. Entry b of sub-histogram g then holds the sum of
 * entry b of sub-histograms [0, g]. Must be dispatched with the local sort dispatch. One invocation per bucket.
 * @param p_group_id The workgroup, whose x is the sub-histogram.
 * @param p_group_thread_id The invocation in its workgroup, whose x is the bucket.
 * @param p_histogram_buffer The histogram buffer, with its metadata, sub-histograms, and overflow histograms.
 */
[wbe::kernel(WBE_RADIX_SORT_BUCKET_COUNT)]
void radix_sort_histogram_add_overflow(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("histogram_buffer")] RWStructuredBuffer<uint> p_histogram_buffer) {
    RWStructuredBuffer<uint> buffer = p_histogram_buffer;
    uint sub_histogram_num = buffer[WBE_RADIX_SORT_SUB_HISTOGRAM_NUM_INDEX];
    uint overflow_start = buffer[WBE_RADIX_SORT_OVERFLOW_START_INDEX];
    uint lane_start = buffer[WBE_RADIX_SORT_LANE_START_INDEX];
    uint sub_histogram = p_group_id.x;
    uint bucket = p_group_thread_id.x;
    uint scan_group = sub_histogram / WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM;
    if (sub_histogram >= sub_histogram_num || scan_group == 0) {
        return;
    }
    buffer[lane_start + sub_histogram * WBE_RADIX_SORT_BUCKET_COUNT + bucket] +=
        buffer[overflow_start + (scan_group - 1) * WBE_RADIX_SORT_BUCKET_COUNT + bucket];
}

/**
 * @brief Write the indirect dispatches of a radix sort from the local sort count in the header of the key buffer.
 * @details Writes the local sort dispatch, with one workgroup per local sort, at WBE_RADIX_SORT_LOCAL_SORT_DISPATCH_INDEX,
 * and the histogram prefix scan dispatch, with one workgroup per WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM local sorts, at
 * WBE_RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_INDEX. The element count does not change between the passes, so every pass uses
 * the same dispatches. Must be dispatched with exactly one workgroup.
 * @param p_input_buffer The counted key buffer, whose header holds the local sort count.
 * @param p_dispatch_buffer Receives the dispatches.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(1)]
void radix_sort_prepare_dispatch([wbe::bind("input_buffer", "read")] RWByteAddressBuffer p_input_buffer,
    [wbe::bind("dispatch_buffer", "write")] RWStructuredBuffer<DispatchIndirectCommand> p_dispatch_buffer,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<uint> keys = CountedBuffer<uint>(p_input_buffer, p_frame_slot);
    uint local_sort_num = keys.get_header(WBE_RADIX_SORT_LOCAL_SORT_NUM_INDEX);
    p_dispatch_buffer[WBE_RADIX_SORT_LOCAL_SORT_DISPATCH_INDEX] = DispatchIndirectCommand(local_sort_num);
    p_dispatch_buffer[WBE_RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_INDEX] = DispatchIndirectCommand(
        (local_sort_num + WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM - 1) / WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM);
}

#ifdef WBE_RADIX_HISTOGRAM_BIT_REGION

groupshared uint radix_sort_histogram_workgroup_process_histogram[WBE_RADIX_SORT_BUCKET_COUNT];

/**
 * @brief Count the digit at WBE_RADIX_HISTOGRAM_BIT_REGION of every key into one sub-histogram per workgroup.
 * @details Workgroup g owns the contiguous key range [g * elements_per_group, (g + 1) * elements_per_group), with
 * elements_per_group = ceil(element_count / sub_histogram_num), and writes the raw digit counts of that range to
 * sub-histogram g. The input buffer is a counted buffer of the keys whose header also holds the local sort count. The
 * histogram buffer receives its metadata: the sub-histogram count, equal to the local sort count; the start of the overflow
 * histograms, after the last sub-histogram; and WBE_RADIX_SORT_LANE_START, where sub-histogram g starts at
 * WBE_RADIX_SORT_LANE_START + g * WBE_RADIX_SORT_BUCKET_COUNT. Must be dispatched with the local sort dispatch.
 * @param p_group_id The workgroup, whose x is the sub-histogram.
 * @param p_group_thread_id The invocation in its workgroup.
 * @param p_input_buffer The counted key buffer.
 * @param p_histogram_buffer Receives the histogram metadata and the sub-histograms.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 * @param p_input_words A uint view of the input buffer, which is only read, for the range
 * workgroup_scope_radix_sort_histogram counts.
 */
[wbe::kernel(WBE_RADIX_SORT_KEY_WORKGROUP_SIZE)]
void radix_sort_histogram_workgroup_process(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("input_buffer", "read")] RWByteAddressBuffer p_input_buffer,
    [wbe::bind("histogram_buffer", "write")] RWStructuredBuffer<uint> p_histogram_buffer,
    [wbe::push("frame_slot")] uint p_frame_slot,
    [wbe::bind("input_buffer", "read")] RWStructuredBuffer<uint> p_input_words) {
    CountedBuffer<uint> keys = CountedBuffer<uint>(p_input_buffer, p_frame_slot);
    uint group_id = p_group_id.x;
    uint group_thread_id = p_group_thread_id.x;
    // The starting position of a thread in a 256 chunk.
    uint thread_data_start = group_thread_id * 4;
    uint element_count = keys.size();
    uint sub_histogram_num = keys.get_header(WBE_RADIX_SORT_LOCAL_SORT_NUM_INDEX);
    if (group_id >= sub_histogram_num) {
        return;
    }
    if (group_id == 0 && group_thread_id == 0) {
        p_histogram_buffer[WBE_RADIX_SORT_SUB_HISTOGRAM_NUM_INDEX] = sub_histogram_num;
        p_histogram_buffer[WBE_RADIX_SORT_OVERFLOW_START_INDEX] =
            WBE_RADIX_SORT_LANE_START + sub_histogram_num * WBE_RADIX_SORT_BUCKET_COUNT;
        p_histogram_buffer[WBE_RADIX_SORT_LANE_START_INDEX] = WBE_RADIX_SORT_LANE_START;
    }
    [unroll] for (uint i = 0; i < 4; ++i) {
        // Initialize the original value in the histogram to 0.
        radix_sort_histogram_workgroup_process_histogram[thread_data_start + i] = 0;
    }

    uint elements_per_group = radix_sort_elements_per_local_sort(element_count, sub_histogram_num);
    uint group_begin = min(group_id * elements_per_group, element_count);
    uint group_end = min(group_begin + elements_per_group, element_count);
    // Synchronizes before counting, so the zeroed histogram is visible.
    workgroup_scope_radix_sort_histogram<uint, WBE_RADIX_SORT_KEY_WORKGROUP_SIZE>(p_input_words,
        keys.get_byte_offset(group_begin) / 4, group_end - group_begin, radix_sort_histogram_workgroup_process_histogram,
        group_thread_id, WBE_RADIX_HISTOGRAM_BIT_REGION);

    uint sub_histogram_start = WBE_RADIX_SORT_LANE_START + group_id * WBE_RADIX_SORT_BUCKET_COUNT;
    [unroll] for (uint i = 0; i < 4; ++i) {
        p_histogram_buffer[sub_histogram_start + thread_data_start + i] =
            radix_sort_histogram_workgroup_process_histogram[thread_data_start + i];
    }
}

groupshared uint radix_sort_scatter_sort_keys[WBE_RADIX_SORT_SCATTER_BATCH_SIZE];
groupshared uint radix_sort_scatter_sort_scratch[WBE_RADIX_SORT_SCATTER_BATCH_SIZE];
// The output index the next element of each digit is written to.
groupshared uint radix_sort_scatter_digit_offsets[WBE_RADIX_SORT_BUCKET_COUNT];
// The index of the first element of each digit in the sorted batch.
groupshared uint radix_sort_scatter_batch_digit_starts[WBE_RADIX_SORT_BUCKET_COUNT];

/**
 * @brief Scatter the elements of one sub-histogram into their sorted positions for the digit at
 * WBE_RADIX_HISTOGRAM_BIT_REGION.
 * @details Workgroup g owns the contiguous input range that sub-histogram g counted:
 * [g * elements_per_group, (g + 1) * elements_per_group), with elements_per_group = ceil(element_count / sub_histogram_num).
 * The histogram buffer must hold the column-wise inclusive prefix sum from radix_sort_histogram_add_overflow. Each element
 * is written to: (number of elements with a smaller digit) + (number of elements with the same digit in earlier
 * sub-histograms) + (rank among the elements with the same digit in this sub-histogram). The workgroup sorts its range
 * stably by the digit in batches of 256, so the scatter is stable. The output buffer receives the element count and the
 * local sort count of the input buffer. Must be dispatched with the local sort dispatch.
 * @param p_group_id The workgroup, whose x is the sub-histogram.
 * @param p_group_thread_id The invocation in its workgroup.
 * @param p_input_buffer The counted key buffer.
 * @param p_histogram_buffer The scanned histogram buffer.
 * @param p_output_buffer Receives the counted, scattered keys.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(WBE_RADIX_SORT_KEY_WORKGROUP_SIZE)]
void radix_sort_scatter(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("input_buffer", "read")] RWByteAddressBuffer p_input_buffer,
    [wbe::bind("histogram_buffer", "read")] RWStructuredBuffer<uint> p_histogram_buffer,
    [wbe::bind("output_buffer", "write")] RWByteAddressBuffer p_output_buffer,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    RWStructuredBuffer<uint> histogram_buffer = p_histogram_buffer;
    CountedBuffer<uint> input_keys = CountedBuffer<uint>(p_input_buffer, p_frame_slot);
    CountedBuffer<uint> output_keys = CountedBuffer<uint>(p_output_buffer, p_frame_slot);
    uint group_id = p_group_id.x;
    uint group_thread_id = p_group_thread_id.x;
    uint thread_data_start = group_thread_id * 4;
    uint element_count = input_keys.size();
    uint sub_histogram_num = histogram_buffer[WBE_RADIX_SORT_SUB_HISTOGRAM_NUM_INDEX];
    uint lane_start = histogram_buffer[WBE_RADIX_SORT_LANE_START_INDEX];
    if (group_id >= sub_histogram_num) {
        return;
    }
    if (group_id == 0 && group_thread_id == 0) {
        output_keys.set_size(element_count);
        output_keys.set_header(
            WBE_RADIX_SORT_LOCAL_SORT_NUM_INDEX, input_keys.get_header(WBE_RADIX_SORT_LOCAL_SORT_NUM_INDEX));
    }

    // The last sub-histogram holds the total count of each digit after the column-wise inclusive prefix sum. An exclusive
    // prefix sum over the digits gives the output index of the first element of each digit.
    uint last_sub_histogram_start = lane_start + (sub_histogram_num - 1) * WBE_RADIX_SORT_BUCKET_COUNT;
    [unroll] for (uint i = 0; i < 4; ++i) {
        radix_sort_scatter_sort_scratch[thread_data_start + i] =
            histogram_buffer[last_sub_histogram_start + thread_data_start + i];
    }
    GroupMemoryBarrierWithGroupSync();
    workgroup_inclusive_prefix_scan_64_256<uint, Add<uint>>(radix_sort_scatter_sort_scratch, thread_data_start);
    [unroll] for (uint i = 0; i < 4; ++i) {
        uint digit = thread_data_start + i;
        // The index of the first digit underflows past the end, so it reads zero.
        uint previous_digit = digit - 1u;
        uint digit_start =
            previous_digit < WBE_RADIX_SORT_SCATTER_BATCH_SIZE ? radix_sort_scatter_sort_scratch[previous_digit] : 0;
        // The inclusive prefix sum of the previous sub-histogram counts this digit in all earlier sub-histograms.
        uint earlier_sub_histogram_count = 0;
        if (group_id > 0) {
            earlier_sub_histogram_count =
                histogram_buffer[lane_start + (group_id - 1) * WBE_RADIX_SORT_BUCKET_COUNT + digit];
        }
        radix_sort_scatter_digit_offsets[digit] = digit_start + earlier_sub_histogram_count;
    }

    uint elements_per_group = radix_sort_elements_per_local_sort(element_count, sub_histogram_num);
    uint group_begin = min(group_id * elements_per_group, element_count);
    uint group_end = min(group_begin + elements_per_group, element_count);
    for (uint batch_begin = group_begin; batch_begin < group_end; batch_begin += WBE_RADIX_SORT_SCATTER_BATCH_SIZE) {
        uint batch_count = min(group_end - batch_begin, WBE_RADIX_SORT_SCATTER_BATCH_SIZE);
        // Padding keys have the largest digit, so the stable sort keeps them after every valid element.
        [unroll] for (uint i = 0; i < 4; ++i) {
            uint index = thread_data_start + i;
            radix_sort_scatter_sort_keys[index] =
                index < batch_count ? input_keys.get(batch_begin + index) : 0xFFFFFFFFu;
        }
        // Also orders the sort_scratch reads above before the sort overwrites it.
        GroupMemoryBarrierWithGroupSync();
        // Stably sort the batch by the WBE_RADIX_SORT_BUCKET_BIT_SIZE-bit digit only.
        [unroll] for (uint i = 0; i < WBE_RADIX_SORT_BUCKET_BIT_SIZE / WBE_RADIX_SORT_WG_BIT_SIZE; ++i) {
            workgroup_radix_sort_step_64_256(radix_sort_scatter_sort_keys,
                WBE_RADIX_HISTOGRAM_BIT_REGION + i * WBE_RADIX_SORT_WG_BIT_SIZE, group_thread_id,
                radix_sort_scatter_sort_scratch);
        }

        uint4 keys;
        uint4 digits;
        [unroll] for (uint i = 0; i < 4; ++i) {
            uint index = thread_data_start + i;
            keys[i] = radix_sort_scatter_sort_keys[index];
            digits[i] = radix_sort_digit(keys[i], WBE_RADIX_HISTOGRAM_BIT_REGION);
            if (index < batch_count
                && (index == 0 || radix_sort_digit(radix_sort_scatter_sort_keys[index - 1], WBE_RADIX_HISTOGRAM_BIT_REGION)
                    != digits[i])) {
                radix_sort_scatter_batch_digit_starts[digits[i]] = index;
            }
        }
        GroupMemoryBarrierWithGroupSync();

        // The number of elements of a digit in the batch, held by the invocation owning the last element of that digit.
        uint4 batch_digit_counts = uint4(0);
        [unroll] for (uint i = 0; i < 4; ++i) {
            uint index = thread_data_start + i;
            if (index < batch_count) {
                uint rank = index - radix_sort_scatter_batch_digit_starts[digits[i]];
                output_keys.set(radix_sort_scatter_digit_offsets[digits[i]] + rank, keys[i]);
                if (index == batch_count - 1
                    || radix_sort_digit(radix_sort_scatter_sort_keys[index + 1], WBE_RADIX_HISTOGRAM_BIT_REGION)
                        != digits[i]) {
                    batch_digit_counts[i] = rank + 1;
                }
            }
        }
        // Every invocation has read digit_offsets before it is advanced.
        GroupMemoryBarrierWithGroupSync();
        [unroll] for (uint i = 0; i < 4; ++i) {
            if (batch_digit_counts[i] > 0) {
                radix_sort_scatter_digit_offsets[digits[i]] += batch_digit_counts[i];
            }
        }
        // The next batch overwrites sort_keys and reads digit_offsets.
        GroupMemoryBarrierWithGroupSync();
    }
}

groupshared uint radix_sort_uint2_histogram_workgroup_process_histogram[WBE_RADIX_SORT_BUCKET_COUNT];

/**
 * @brief Count the digit at WBE_RADIX_HISTOGRAM_BIT_REGION of every uint2 key into the sub-histogram of its local sort.
 * @details See radix_sort_uint2_histogram(). Must be dispatched with the local sort dispatch.
 * @param p_group_id The workgroup, whose x is the sub-histogram.
 * @param p_group_thread_id The invocation in its workgroup.
 * @param p_input_buffer The counted uint2 key buffer.
 * @param p_histogram_buffer Receives the metadata and the sub-histograms.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(WBE_RADIX_SORT_UINT2_WORKGROUP_SIZE)]
void radix_sort_uint2_histogram_workgroup_process(uint3 p_group_id : SV_GroupID,
    uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("input_buffer", "read")] RWByteAddressBuffer p_input_buffer,
    [wbe::bind("histogram_buffer", "write")] RWStructuredBuffer<uint> p_histogram_buffer,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<uint2> input_buffer = CountedBuffer<uint2>(p_input_buffer, p_frame_slot);
    radix_sort_uint2_histogram(input_buffer, p_histogram_buffer, radix_sort_uint2_histogram_workgroup_process_histogram,
        p_group_id.x, p_group_thread_id.x, WBE_RADIX_HISTOGRAM_BIT_REGION);
}

groupshared uint radix_sort_uint2_scatter_sort_entries[WBE_RADIX_SORT_UINT2_BATCH_SIZE];
groupshared uint radix_sort_uint2_scatter_sort_scratch[WBE_RADIX_SORT_UINT2_BATCH_SIZE];
groupshared uint2 radix_sort_uint2_scatter_batch_keys[WBE_RADIX_SORT_UINT2_BATCH_SIZE];
groupshared uint radix_sort_uint2_scatter_digit_offsets[WBE_RADIX_SORT_BUCKET_COUNT];
groupshared uint radix_sort_uint2_scatter_batch_digit_starts[WBE_RADIX_SORT_BUCKET_COUNT];

/**
 * @brief Scatter the uint2 keys of one sub-histogram into their sorted positions for the digit at
 * WBE_RADIX_HISTOGRAM_BIT_REGION.
 * @details See radix_sort_uint2_scatter(); named apart from it, since kernels must not be overloaded. Must be dispatched
 * with the local sort dispatch.
 * @param p_group_id The workgroup, whose x is the sub-histogram.
 * @param p_group_thread_id The invocation in its workgroup.
 * @param p_input_buffer The counted uint2 key buffer.
 * @param p_histogram_buffer The scanned histogram buffer.
 * @param p_output_buffer Receives the counted, scattered keys.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(WBE_RADIX_SORT_UINT2_WORKGROUP_SIZE)]
void radix_sort_uint2_scatter_keys(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("input_buffer", "read")] RWByteAddressBuffer p_input_buffer,
    [wbe::bind("histogram_buffer", "read")] RWStructuredBuffer<uint> p_histogram_buffer,
    [wbe::bind("output_buffer", "write")] RWByteAddressBuffer p_output_buffer,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<uint2> input_buffer = CountedBuffer<uint2>(p_input_buffer, p_frame_slot);
    CountedBuffer<uint2> output_buffer = CountedBuffer<uint2>(p_output_buffer, p_frame_slot);
    radix_sort_uint2_scatter(input_buffer, p_histogram_buffer, output_buffer, radix_sort_uint2_scatter_sort_entries,
        radix_sort_uint2_scatter_sort_scratch, radix_sort_uint2_scatter_batch_keys, radix_sort_uint2_scatter_digit_offsets,
        radix_sort_uint2_scatter_batch_digit_starts, p_group_id.x, p_group_thread_id.x, WBE_RADIX_HISTOGRAM_BIT_REGION);
}

#endif

#endif
