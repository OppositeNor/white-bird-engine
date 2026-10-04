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

#ifndef WBE_FILE_PREFIX_SUM_UTILS_SLANG
#define WBE_FILE_PREFIX_SUM_UTILS_SLANG
/**
 * @brief Computes an in-place inclusive prefix sum over a 256-element groupshared array with a 64-invocation workgroup.
 * @details Uses a Hillis-Steele scan in which each invocation owns 4 contiguous elements, starting at p_start, and
 * updates them as one uint4 per step. The 64 invocations must together own every element of p_data[0, 256) exactly
 * once. Reads that would fall before the start of the array are redirected to p_data[256], which must hold zero and is
 * left unchanged. Every invocation of the 64-invocation workgroup must call this function with the same array. The
 * function does not synchronize before its first read, so callers must issue a workgroup barrier after filling the
 * array; it synchronizes after the final write, so the results are visible to the whole workgroup on return.
 * @param p_data The groupshared array to sum in place: 256 elements followed by a zero element.
 * @param p_start The index of the first of the 4 contiguous elements this invocation owns; a multiple of 4 in
 * [0, 256).
 */
void workgroup_inclusive_prefix_sum_64_256(__ref groupshared uint p_data[256 + 1], uint p_start) {
    [unroll] for (uint i = 0; i < 8; ++i) {
        // We split the 256 entries in chunks of 4, where each thread performs SIMD on the 4 entries they're responsible of.
        uint offset = 1 << i;
        uint4 left_region_data = uint4(
            // It will trigger an unsigned overflow if the index has overflowed. So overflowed index could be captured by min.
            p_data[min(p_start - offset + 0, 256)],
            p_data[min(p_start - offset + 1, 256)],
            p_data[min(p_start - offset + 2, 256)],
            p_data[min(p_start - offset + 3, 256)]
        );
        uint4 curr_region_data = uint4(
            p_data[p_start + 0],
            p_data[p_start + 1],
            p_data[p_start + 2],
            p_data[p_start + 3]
        );
        curr_region_data += left_region_data;
        GroupMemoryBarrierWithGroupSync();
        p_data[p_start] = curr_region_data[0];
        p_data[p_start + 1] = curr_region_data[1];
        p_data[p_start + 2] = curr_region_data[2];
        p_data[p_start + 3] = curr_region_data[3];
        GroupMemoryBarrierWithGroupSync();
    }
}

/**
 * @brief Computes an in-place inclusive prefix sum over a buffer range of any length with one workgroup.
 * @details Sums p_buffer[p_start, p_start + p_element_count) with a Hillis-Steele scan. The range is split into chunks
 * of 4 * N elements; in each chunk, invocation p_thread_id owns the 4 contiguous elements starting at
 * 4 * p_thread_id and updates them as one uint4 per step. Chunks are processed from the last to the first within each
 * step, so every read of an element to the left sees the value from the previous step. Reads that would fall before
 * the start of the range are redirected to p_buffer[p_start + p_element_count], which must hold zero and is left
 * unchanged, as are all elements outside the range. Every invocation of the N-invocation workgroup must call this
 * function with the same buffer, start, and element count, and only one workgroup may operate on the range at a time.
 * The function synchronizes between reading and writing each chunk, so the results are visible to the whole workgroup
 * on return.
 * @tparam N The number of invocations in the workgroup.
 * @param p_buffer The buffer whose range is summed in place.
 * @param p_start The index of the first element of the range.
 * @param p_element_count The number of elements in the range.
 * @param p_thread_id The local invocation index in [0, N).
 */
void workgroup_general_prefix_sum<let N : uint>(RWStructuredBuffer<uint> p_buffer, uint p_start, uint p_element_count,
        uint p_thread_id) {
    // Each chunk covers 4 elements per invocation.
    uint chunk_count = (p_element_count + 4 * N - 1) / (4 * N);
    for (uint i = 1; i < p_element_count; i <<= 1) {
        for (uint j = chunk_count; j > 0; --j) {
            // We have to start from the right side, because we're reading values from the left side.
            uint subportion_start = (N * (j - 1) + p_thread_id) * 4;
            // Subportion of the work area.
            uint4 left_region_data = uint4(
                // An index before the range underflows, and an index past the range is too large; min redirects both
                // to the zero element after the range.
                p_buffer[p_start + min(subportion_start - i + 0, p_element_count)],
                p_buffer[p_start + min(subportion_start - i + 1, p_element_count)],
                p_buffer[p_start + min(subportion_start - i + 2, p_element_count)],
                p_buffer[p_start + min(subportion_start - i + 3, p_element_count)]
            );
            uint4 curr_region_data = uint4(
                p_buffer[p_start + min(subportion_start + 0, p_element_count)],
                p_buffer[p_start + min(subportion_start + 1, p_element_count)],
                p_buffer[p_start + min(subportion_start + 2, p_element_count)],
                p_buffer[p_start + min(subportion_start + 3, p_element_count)]
            );
            curr_region_data += left_region_data;
            // The buffer is device memory, so group-memory barriers would not order these accesses.
            AllMemoryBarrierWithGroupSync();
            // The last chunk may extend past the range, which must not be written.
            [unroll] for (uint k = 0; k < 4; ++k) {
                if (subportion_start + k < p_element_count) {
                    p_buffer[p_start + subportion_start + k] = curr_region_data[k];
                }
            }
            AllMemoryBarrierWithGroupSync();
        }
    }
}
#endif
