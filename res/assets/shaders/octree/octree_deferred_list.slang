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
#ifndef WBE_FILE_OCTREE_DEFERRED_LIST_SLANG
#define WBE_FILE_OCTREE_DEFERRED_LIST_SLANG

/**
 * @brief Traversal position retained across deferred construction rounds.
 */
struct OctreeNodeTraceInfo {
    /**
     * @brief Index of the node where traversal resumes.
     */
    uint node_index;
    /**
     * @brief Depth of the current stored node; root sectors have depth one.
     */
    uint curr_depth;
    /**
     * @brief Remaining traversal steps; negative means unbounded traversal.
     */
    int remain_depth;
    /**
     * @brief Voxel position in the current node's local unit cube.
     */
    float3 svo_position;
};

/**
 * @brief Compact traversal state registered in the deferred list.
 */
[WBE_DATA_TYPE]
struct OctreeDeferredListEntry {
    static const uint CURR_DEPTH_MASK = 0xffff0000;
    static const uint REMAIN_DEPTH_MASK = 0x0000ffff;
    static const uint CURR_DEPTH_SHIFT = 16;
    /**
     * @brief Index of the node where traversal resumes.
     */
    uint node_index;
    /**
     * @brief The packed depth. The upper 16 bits store the current depth, and the lower 16 bits store the remaining
     * depth as a signed 16-bit value; negative means unbounded traversal.
     */
    uint packed_depth;
    /**
     * @brief Flattened voxel coordinate in the current node's local grid of extent 2^(max_depth - curr_depth).
     * @details Each axis occupies max_depth bits, x in the lowest bits and z in the highest, so max_depth must not
     * exceed 10.
     */
    uint local_voxel_index;
};

/**
 * @brief Packs traversal state into a deferred-list entry.
 * @param p_max_depth The octree's max_depth; p_trace_info.curr_depth must not exceed it.
 */
OctreeDeferredListEntry to_deferred_list_entry(OctreeNodeTraceInfo p_trace_info, uint p_max_depth) {
    OctreeDeferredListEntry result;
    result.node_index = p_trace_info.node_index;
    result.packed_depth = ((p_trace_info.curr_depth << OctreeDeferredListEntry::CURR_DEPTH_SHIFT) & OctreeDeferredListEntry::CURR_DEPTH_MASK)
        | (uint(p_trace_info.remain_depth) & OctreeDeferredListEntry::REMAIN_DEPTH_MASK);
    // Truncation keeps every remaining subregion bit of the node-local position.
    uint3 local_voxel = uint3(p_trace_info.svo_position * float(1u << (p_max_depth - p_trace_info.curr_depth)));
    result.local_voxel_index = local_voxel.x | (local_voxel.y << p_max_depth) | (local_voxel.z << (2 * p_max_depth));
    return result;
}

/**
 * @brief Unpacks a deferred-list entry into traversal state.
 * @details The position is restored to the center of its local voxel.
 * @param p_max_depth The octree's max_depth used when packing p_entry.
 */
OctreeNodeTraceInfo to_trace_info(OctreeDeferredListEntry p_entry, uint p_max_depth) {
    OctreeNodeTraceInfo result;
    result.node_index = p_entry.node_index;
    result.curr_depth = (p_entry.packed_depth & OctreeDeferredListEntry::CURR_DEPTH_MASK) >> OctreeDeferredListEntry::CURR_DEPTH_SHIFT;
    // Sign-extend the lower 16 bits so negative remaining depths survive the round trip.
    result.remain_depth = int(p_entry.packed_depth << OctreeDeferredListEntry::CURR_DEPTH_SHIFT) >> OctreeDeferredListEntry::CURR_DEPTH_SHIFT;
    uint axis_mask = (1u << p_max_depth) - 1;
    uint3 local_voxel = uint3(p_entry.local_voxel_index & axis_mask,
        (p_entry.local_voxel_index >> p_max_depth) & axis_mask,
        p_entry.local_voxel_index >> (2 * p_max_depth));
    result.svo_position = (float3(local_voxel) + 0.5) / float(1u << (p_max_depth - result.curr_depth));
    return result;
}

#endif
