/* Copyright 2026 OppositeNor

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
 * @file dedup_kernel.slang
 * @brief The large deduplication compute kernels, whose entry points the ACP generates.
 */
#ifndef WBE_FILE_DEDUP_KERNEL_SLANG
#define WBE_FILE_DEDUP_KERNEL_SLANG

#include "utils/utils.slang"
#include "dedup/dedup_large_utils.slang"

/**
 * @brief Prepare the keys for the large deduplication.
 * @details Writes the number of local sorts of the radix sort to the metadata of the keys, and the indirect dispatch of
 * the per-key passes. Must be dispatched with exactly one workgroup.
 * @param p_keys The keys, a counted buffer of uint.
 * @param p_dispatch_buffer Receives the indirect dispatch of the per-key passes.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(1, 1, 1)]
void dedup_large_prepare_dispatch(
    [wbe::bind("keys")] RWByteAddressBuffer p_keys,
    [wbe::bind("dispatch_buffer", "write")] RWStructuredBuffer<DispatchIndirectCommand> p_dispatch_buffer,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<uint> keys = CountedBuffer<uint>(p_keys, p_frame_slot);
    uint key_count = keys.size();
    keys.set_header(WBE_RADIX_SORT_LOCAL_SORT_NUM_INDEX, radix_sort_local_sort_num(key_count));
    p_dispatch_buffer[0] = DispatchIndirectCommand(dedup_large_group_num(key_count));
}

/**
 * @brief Mark the first key of every value in the sorted keys; see dedup_large_mark().
 * @details Must be dispatched with the per-key dispatch.
 * @param p_dispatch_thread_id The invocation's key index in x.
 * @param p_keys The sorted keys, a counted buffer of uint.
 * @param p_marks Receives the marks, a counted buffer of uint.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(WBE_DEDUP_LARGE_WORKGROUP_SIZE, 1, 1)]
void dedup_large_mark_keys(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("keys", "read")] RWByteAddressBuffer p_keys,
    [wbe::bind("marks", "write")] RWByteAddressBuffer p_marks,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<uint> keys = CountedBuffer<uint>(p_keys, p_frame_slot);
    CountedBuffer<uint> marks = CountedBuffer<uint>(p_marks, p_frame_slot);
    dedup_large_mark(keys, marks, p_dispatch_thread_id.x);
}

/**
 * @brief Scatter the first key of every value in the sorted keys to the unique keys; see dedup_large_scatter().
 * @details The marks must hold their inclusive prefix sum. Must be dispatched with the per-key dispatch.
 * @param p_dispatch_thread_id The invocation's key index in x.
 * @param p_keys The sorted keys, a counted buffer of uint.
 * @param p_marks The inclusive prefix sum of the marks, a counted buffer of uint.
 * @param p_unique_keys Receives the unique keys, a counted buffer of uint.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(WBE_DEDUP_LARGE_WORKGROUP_SIZE, 1, 1)]
void dedup_large_scatter_keys(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("keys", "read")] RWByteAddressBuffer p_keys,
    [wbe::bind("marks", "read")] RWByteAddressBuffer p_marks,
    [wbe::bind("unique_keys", "write")] RWByteAddressBuffer p_unique_keys,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<uint> keys = CountedBuffer<uint>(p_keys, p_frame_slot);
    CountedBuffer<uint> marks = CountedBuffer<uint>(p_marks, p_frame_slot);
    CountedBuffer<uint> unique_keys = CountedBuffer<uint>(p_unique_keys, p_frame_slot);
    dedup_large_scatter(keys, marks, unique_keys, p_dispatch_thread_id.x);
}

#endif
