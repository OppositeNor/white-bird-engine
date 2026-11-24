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
 * @file prefix_scan_kernel.slang
 * @brief The large prefix scan compute kernels, whose entry points the ACP generates; their building blocks are in
 * large_prefix_scan_utils.slang.
 * @details The kernels scan WBE_PREFIX_SCAN_TYPE elements with WBE_PREFIX_SCAN_OPERATION, uint with Add by default. A
 * scan runs prepare_dispatch, then blocks, sections, totals, and add. The workgroup kernels' size, 64, is
 * WBE_LARGE_PREFIX_SCAN_WORKGROUP_SIZE: a kernel attribute needs a literal, so the two must change together.
 */
#ifndef WBE_FILE_PREFIX_SCAN_KERNEL_SLANG
#define WBE_FILE_PREFIX_SCAN_KERNEL_SLANG

#include "utils/utils.slang"
#include "prefix_scan/large_prefix_scan_utils.slang"

/**
 * @brief Write the indirect dispatch of the block passes of a large prefix scan, with one workgroup per block of the
 * element count of the scanned buffer.
 * @details Must be dispatched with exactly one workgroup.
 * @param p_data_buffer The counted buffer to scan.
 * @param p_dispatch_buffer Receives the dispatch of the block passes.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(1, 1, 1)]
void large_prefix_scan_prepare_dispatch(
    [wbe::bind("data_buffer", "read")] RWByteAddressBuffer p_data_buffer,
    [wbe::bind("dispatch_buffer", "write")] RWStructuredBuffer<DispatchIndirectCommand> p_dispatch_buffer,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<WBE_PREFIX_SCAN_TYPE> data_buffer = CountedBuffer<WBE_PREFIX_SCAN_TYPE>(p_data_buffer, p_frame_slot);
    p_dispatch_buffer[0] = DispatchIndirectCommand(large_prefix_scan_block_num(data_buffer.size()));
}

groupshared WBE_PREFIX_SCAN_TYPE large_prefix_scan_blocks_scratch[WBE_LARGE_PREFIX_SCAN_BLOCK_SIZE];

/**
 * @brief First pass of the large prefix scan; see large_prefix_scan_blocks() in large_prefix_scan_utils.slang.
 * @details Must be dispatched with one workgroup per block.
 * @param p_group_id The block of the workgroup in x.
 * @param p_group_thread_id The invocation's index in its workgroup in x.
 * @param p_data_buffer The counted buffer to scan, scanned in place within each block.
 * @param p_sections_buffer Receives the block totals.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(64, 1, 1)]
void large_prefix_scan_blocks_kernel(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("data_buffer")] RWByteAddressBuffer p_data_buffer,
    [wbe::bind("sections_buffer", "write")] RWStructuredBuffer<WBE_PREFIX_SCAN_TYPE> p_sections_buffer,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<WBE_PREFIX_SCAN_TYPE> data_buffer = CountedBuffer<WBE_PREFIX_SCAN_TYPE>(p_data_buffer, p_frame_slot);
    large_prefix_scan_blocks<WBE_PREFIX_SCAN_TYPE, WBE_PREFIX_SCAN_OPERATION>(
        data_buffer,
        p_sections_buffer,
        p_group_id.x,
        p_group_thread_id.x,
        large_prefix_scan_blocks_scratch);
}

groupshared WBE_PREFIX_SCAN_TYPE large_prefix_scan_sections_scratch[WBE_LARGE_PREFIX_SCAN_WORKGROUP_SIZE];

/**
 * @brief Second pass of the large prefix scan; see large_prefix_scan_sections() in large_prefix_scan_utils.slang.
 * @details Must be dispatched with exactly WBE_LARGE_PREFIX_SCAN_SECTION_NUM workgroups.
 * @param p_group_id The section of the workgroup in x.
 * @param p_group_thread_id The invocation's index in its workgroup in x.
 * @param p_data_buffer The counted buffer being scanned.
 * @param p_sections_buffer The section and block totals, scanned in place.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(64, 1, 1)]
void large_prefix_scan_sections_kernel(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("data_buffer", "read")] RWByteAddressBuffer p_data_buffer,
    [wbe::bind("sections_buffer")] RWStructuredBuffer<WBE_PREFIX_SCAN_TYPE> p_sections_buffer,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<WBE_PREFIX_SCAN_TYPE> data_buffer = CountedBuffer<WBE_PREFIX_SCAN_TYPE>(p_data_buffer, p_frame_slot);
    large_prefix_scan_sections<WBE_PREFIX_SCAN_TYPE, WBE_PREFIX_SCAN_OPERATION>(
        data_buffer,
        p_sections_buffer,
        p_group_id.x,
        p_group_thread_id.x,
        large_prefix_scan_sections_scratch);
}

groupshared WBE_PREFIX_SCAN_TYPE large_prefix_scan_totals_scratch[WBE_LARGE_PREFIX_SCAN_SECTION_NUM];

/**
 * @brief Third pass of the large prefix scan; see large_prefix_scan_totals() in large_prefix_scan_utils.slang.
 * @details Must be dispatched with exactly one workgroup.
 * @param p_group_thread_id The invocation's index in the workgroup in x.
 * @param p_sections_buffer The section totals, scanned in place.
 */
[wbe::kernel(64, 1, 1)]
void large_prefix_scan_totals_kernel(uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("sections_buffer")] RWStructuredBuffer<WBE_PREFIX_SCAN_TYPE> p_sections_buffer) {
    large_prefix_scan_totals<WBE_PREFIX_SCAN_TYPE, WBE_PREFIX_SCAN_OPERATION>(
        p_sections_buffer,
        p_group_thread_id.x,
        large_prefix_scan_totals_scratch);
}

/**
 * @brief Last pass of the large prefix scan; see large_prefix_scan_add() in large_prefix_scan_utils.slang.
 * @details Must be dispatched with one workgroup per block.
 * @param p_group_id The block of the workgroup in x.
 * @param p_group_thread_id The invocation's index in its workgroup in x.
 * @param p_data_buffer The counted buffer being scanned, completed in place.
 * @param p_sections_buffer The scanned section and block totals.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(64, 1, 1)]
void large_prefix_scan_add_kernel(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("data_buffer")] RWByteAddressBuffer p_data_buffer,
    [wbe::bind("sections_buffer", "read")] RWStructuredBuffer<WBE_PREFIX_SCAN_TYPE> p_sections_buffer,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    CountedBuffer<WBE_PREFIX_SCAN_TYPE> data_buffer = CountedBuffer<WBE_PREFIX_SCAN_TYPE>(p_data_buffer, p_frame_slot);
    large_prefix_scan_add<WBE_PREFIX_SCAN_TYPE, WBE_PREFIX_SCAN_OPERATION>(
        data_buffer,
        p_sections_buffer,
        p_group_id.x,
        p_group_thread_id.x);
}

#endif
