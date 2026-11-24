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
 * @file svo_3_kernel.slang
 * @brief The svo_3 compute kernels, whose entry points the ACP generates.
 */
#ifndef WBE_FILE_SVO_3_KERNEL_SLANG
#define WBE_FILE_SVO_3_KERNEL_SLANG

#include "utils/utils.slang"
#include "svo/svo_common.slang"
#include "sort/radix_sort_utils.slang"

/**
 * @brief Clamps the leaf count of the deferred list filled by svo_gather_leaves to its capacity.
 * @details svo_gather_leaves reports the overflow to shader validation, so that the deduplication reads only registered
 * leaves. Must be dispatched with exactly one workgroup.
 * @param p_svo_info The SVO configuration.
 * @param p_deferred_list The counted deferred list of gathered leaves.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(1)]
void svo_3_prepare_leaves([wbe::bind("svo_info", "read")] RWStructuredBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_deferred_list")] RWByteAddressBuffer p_deferred_list,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<uint> deferred_list = CountedBuffer<uint>(p_deferred_list, p_frame_slot);
    deferred_list.set_size(min(deferred_list.size(), p_svo_info[0].max_deferred_task_count));
}

#endif
