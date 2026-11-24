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
 * @file svo_2_kernel.slang
 * @brief The svo_2 compute kernels, whose entry points the ACP generates; their building blocks are in svo_utils.slang.
 * @details WBE_DYNAMIC_BUILD selects the dynamic allocation of svo_build_deferred_propagation, and WBE_USE_PING_WRITE makes
 * it read the pong list and write the ping list.
 */
#ifndef WBE_FILE_SVO_2_KERNEL_SLANG
#define WBE_FILE_SVO_2_KERNEL_SLANG

#include "utils/utils.slang"
#include "svo_2/svo_utils.slang"
#ifdef WBE_DYNAMIC_BUILD
#include "svo_2/svo_dynamic_build.slang"
#endif

/**
 * @brief Resets mutable SVO state before construction begins.
 * @details Depth and capacity are immutable configuration; the first eight invocations reset the root sectors, and
 * invocation zero resets the counters and both deferred lists.
 * @param p_dispatch_thread_id The invocation's index in the dispatch.
 * @param p_svo_info The SVO configuration and counters.
 * @param p_tree_nodes The SVO's nodes.
 * @param p_deferred_list_ping The ping deferred list.
 * @param p_deferred_list_pong The pong deferred list.
 */
[wbe::kernel(64)]
void svo_reset(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("svo_info")] RWStructuredBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_node_buffer")] RWStructuredBuffer<SVONode> p_tree_nodes,
    [wbe::bind("svo_deferred_list_ping", "write")] RWStructuredBuffer<uint> p_deferred_list_ping,
    [wbe::bind("svo_deferred_list_pong", "write")] RWStructuredBuffer<uint> p_deferred_list_pong) {
    uint index = p_dispatch_thread_id.x;
    if (index < 8) {
        p_tree_nodes[index].children_data = uint(SVONode::ChildrenState::EMPTY) << SVO_CHILDREN_STATE_SHIFT;
        p_tree_nodes[index].data_index = index;
        p_tree_nodes[index].parent_data = make_svo_root_parent_data();
    }
    if (index == 0) {
        p_svo_info[0].node_count = 8;
        p_svo_info[0].leaves_count = 0;
        p_svo_info[0].data_count = 8;
        p_deferred_list_ping[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;
        p_deferred_list_pong[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;
    }
}

#ifdef WBE_USE_PING_WRITE
#define WBE_SVO_2_DEFERRED_LIST_READ_ID "svo_deferred_list_pong"
#define WBE_SVO_2_DEFERRED_LIST_WRITE_ID "svo_deferred_list_ping"
#else
#define WBE_SVO_2_DEFERRED_LIST_READ_ID "svo_deferred_list_ping"
#define WBE_SVO_2_DEFERRED_LIST_WRITE_ID "svo_deferred_list_pong"
#endif

/**
 * @brief Retraces each retained voxel from the root and queues it again in the opposite list on contention.
 * @details The list may exceed one dispatch's thread count, so each invocation strides over the whole dispatch. With
 * WBE_DYNAMIC_BUILD, voxels are allocated as dynamic children.
 * @param p_dispatch_thread_id The invocation's index in the dispatch.
 * @param p_svo_info The SVO configuration and counters.
 * @param p_tree_nodes The SVO's nodes.
 * @param p_tree_leaves The SVO's leaves.
 * @param p_deferred_list_read The deferred list holding this pass's voxels.
 * @param p_deferred_list_write The deferred list receiving the voxels that failed to allocate.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(128)]
void svo_build_deferred_propagation(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("svo_info")] RWStructuredBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_node_buffer")] RWStructuredBuffer<SVONode> p_tree_nodes,
    [wbe::bind("svo_leaf_buffer")] RWStructuredBuffer<SVOLeaf> p_tree_leaves,
    [wbe::bind(WBE_SVO_2_DEFERRED_LIST_READ_ID)] StructuredBuffer<uint> p_deferred_list_read,
    [wbe::bind(WBE_SVO_2_DEFERRED_LIST_WRITE_ID)] RWStructuredBuffer<uint> p_deferred_list_write,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    uint task_stride = WorkgroupCount().x * 128;
    uint deferred_task_count = p_deferred_list_read[SVO_DEFERRED_LIST_COUNT_INDEX];
    for (uint task_index = p_dispatch_thread_id.x; task_index < deferred_task_count; task_index += task_stride) {
        uint task = p_deferred_list_read[SVO_DEFERRED_LIST_ENTRY_OFFSET + task_index];

        SVONodeTraceInfo trace_info = to_trace_info(task, p_svo_info[0].max_depth);
        // Try to allocate the node.
#ifdef WBE_DYNAMIC_BUILD
        if (!try_dynamic_voxel_allocate(p_tree_nodes, p_tree_leaves, p_svo_info, trace_info, p_frame_slot)) {
#else
        if (!try_voxel_allocate(p_tree_nodes, p_tree_leaves, p_svo_info, trace_info, p_frame_slot)) {
#endif
            // If allocation failed, push this to the deferred list.
            uint new_task_index;
            InterlockedAdd(p_deferred_list_write[SVO_DEFERRED_LIST_COUNT_INDEX], 1, new_task_index);
            WBE_SHADER_DEBUG_ASSERT(new_task_index < p_svo_info[0].max_deferred_task_count,
                ShaderDebugError::SVO_DEFERRED_TASK_INDEX_OVERFLOW, p_frame_slot);
            p_deferred_list_write[SVO_DEFERRED_LIST_ENTRY_OFFSET + new_task_index] = task;
        }
    }
}

/**
 * @brief Publishes completed allocations between construction passes.
 * @details The producer must finish and make all child initialization visible before this dispatch. Each invocation
 * exclusively updates one node. A memory dependency must precede the next consumer.
 * @param p_dispatch_thread_id The invocation's index in the dispatch, which is the index of its node.
 * @param p_svo_info The SVO configuration and counters.
 * @param p_tree_nodes The SVO's nodes.
 */
[wbe::kernel(64)]
void svo_update_states(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("svo_info")] ConstantBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_node_buffer")] RWStructuredBuffer<SVONode> p_tree_nodes) {
    uint node_index = p_dispatch_thread_id.x;
    if (node_index >= p_svo_info.node_count) {
        return;
    }
    uint children_data = p_tree_nodes[node_index].children_data;
    if ((children_data >> SVO_CHILDREN_STATE_SHIFT) != uint(SVONode::ChildrenState::ALLOCATING)) {
        return;
    }
    uint state = (children_data & SVO_CHILDREN_OCCUPANCY_MASK) != 0 ? uint(SVONode::ChildrenState::LEAF)
                                                               : uint(SVONode::ChildrenState::NODE);
    p_tree_nodes[node_index].children_data =
        (children_data & SVO_CHILDREN_PAYLOAD_MASK) | (state << SVO_CHILDREN_STATE_SHIFT);
}

/**
 * @brief Publishes initialized dynamic child blocks at a pass boundary.
 * @details The node buffer holds the static nodes followed by the dynamic nodes.
 * @param p_dispatch_thread_id The invocation's index in the dispatch, which is the index of its node.
 * @param p_svo_info The SVO configuration and counters.
 * @param p_nodes The SVO's nodes.
 */
[wbe::kernel(128)]
void svo_dynamic_update_states(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("svo_info")] ConstantBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_node_buffer")] RWStructuredBuffer<SVONode> p_nodes) {
    uint index = p_dispatch_thread_id.x;
    if (index >= p_svo_info.node_count + p_svo_info.dynamic_node_count) {
        return;
    }
    uint description = p_nodes[index].children_data;
    if ((description >> SVO_CHILDREN_STATE_SHIFT) != uint(SVONode::ChildrenState::ALLOCATING_DYNAMIC)) {
        return;
    }
    uint state = (description & SVO_CHILDREN_OCCUPANCY_MASK) != 0 ? uint(SVONode::ChildrenState::LEAF_DYNAMIC)
                                                                  : uint(SVONode::ChildrenState::NODE_DYNAMIC);
    p_nodes[index].children_data = (description & SVO_CHILDREN_PAYLOAD_MASK) | (state << SVO_CHILDREN_STATE_SHIFT);
}

/**
 * @brief Restores static parents and discards the previous frame's dynamic allocation ranges.
 * @details Leaf restoration uses its saved block directly, never the concurrently reset dynamic count. A graph dependency
 * must complete this entire dispatch before any dynamic allocation begins.
 * @param p_dispatch_thread_id The invocation's index in the dispatch, which is the index of its node.
 * @param p_svo_info The SVO configuration and counters.
 * @param p_nodes The SVO's nodes.
 * @param p_deferred_list_ping The ping deferred list.
 * @param p_deferred_list_pong The pong deferred list.
 */
[wbe::kernel(128)]
void svo_clear_dynamic(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("svo_info")] RWStructuredBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_node_buffer")] RWStructuredBuffer<SVONode> p_nodes,
    [wbe::bind("svo_deferred_list_ping", "write")] RWStructuredBuffer<uint> p_deferred_list_ping,
    [wbe::bind("svo_deferred_list_pong", "write")] RWStructuredBuffer<uint> p_deferred_list_pong) {
    uint index = p_dispatch_thread_id.x;
    if (index < p_svo_info[0].node_count) {
        uint description = p_nodes[index].children_data;
        uint state = description >> SVO_CHILDREN_STATE_SHIFT;
        if (state == uint(SVONode::ChildrenState::NODE_DYNAMIC)) {
            p_nodes[index].children_data = uint(SVONode::ChildrenState::EMPTY) << SVO_CHILDREN_STATE_SHIFT;
        } else if (state == uint(SVONode::ChildrenState::LEAF_DYNAMIC)) {
            p_nodes[index].children_data = p_nodes[index].original_children_data;
        }
    }
    if (index == 0) {
        p_svo_info[0].dynamic_node_count = 0;
        p_svo_info[0].dynamic_leaves_count = 0;
        p_deferred_list_ping[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;
        p_deferred_list_pong[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;
    }
}

#endif
