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
 * @file svo_kernel.slang
 * @brief The SVO compute kernels, whose entry points the ACP generates; their building blocks are in svo_utils.slang and
 * svo.slang.
 * @details Every shader resource compiling this file compiles all of its kernels with that resource's variant macros:
 * WBE_DYNAMIC_BUILD selects dynamic allocation in svo_build_deferred_propagation(), WBE_USE_PING_WRITE its ping-write list
 * direction, and WBE_RESET_PING_COUNT the list svo_deferred_count_reset() clears.
 */
#ifndef WBE_FILE_SVO_KERNEL_SLANG
#define WBE_FILE_SVO_KERNEL_SLANG

#include "utils/utils.slang"
#include "svo/svo_utils.slang"
#include "svo/svo.slang"
#ifdef WBE_DYNAMIC_BUILD
#include "svo/svo_dynamic_build.slang"
#endif

/**
 * @brief One VkDrawIndexedIndirectCommand, written by the debug-draw kernels.
 */
struct DrawIndexedIndirectCommand {
    /**
     * @brief Number of indices drawn per instance.
     */
    uint index_count;
    /**
     * @brief Number of instances drawn.
     */
    uint instance_count;
    /**
     * @brief First index of the draw.
     */
    uint first_index;
    /**
     * @brief Value added to every vertex index.
     */
    int vertex_offset;
    /**
     * @brief First instance of the draw.
     */
    uint first_instance;
};

/**
 * @brief Resets mutable SVO state before construction begins.
 * @param p_svo_info The SVO info.
 * @param p_tree_nodes The SVO's nodes.
 * @param p_deferred_list_ping The ping deferred list, whose count is cleared.
 * @param p_deferred_list_pong The pong deferred list, whose count is cleared.
 */
[wbe::kernel(1, 1, 1)]
void svo_reset([wbe::bind("svo_info")] RWStructuredBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_node_buffer")] RWStructuredBuffer<SVONode> p_tree_nodes,
    [wbe::bind("svo_deferred_list_ping", "write")] RWStructuredBuffer<uint> p_deferred_list_ping,
    [wbe::bind("svo_deferred_list_pong", "write")] RWStructuredBuffer<uint> p_deferred_list_pong) {
    p_svo_info[0].node_count = 8;
    p_svo_info[0].leaves_count = 0;
    p_svo_info[0].data_count = 8;
    p_deferred_list_ping[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;
    p_deferred_list_pong[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;

    [unroll] for (uint i = 0; i < 8; ++i) {
        // Reset the top most portion of the tree.
        p_tree_nodes[i].children_data = uint(SVONode::ChildrenState::EMPTY) << SVO_CHILDREN_STATE_SHIFT;
        p_tree_nodes[i].data_index = i;
        p_tree_nodes[i].parent_data = make_svo_root_parent_data();
    }
}

// The deferred list svo_deferred_count_reset() clears: the ping list with WBE_RESET_PING_COUNT, the pong list otherwise.
#ifdef WBE_RESET_PING_COUNT
#define SVO_DEFERRED_COUNT_RESET_LIST_ID "svo_deferred_list_ping"
#else
#define SVO_DEFERRED_COUNT_RESET_LIST_ID "svo_deferred_list_pong"
#endif

/**
 * @brief Clears the deferred-list count selected by the shader variant.
 * @param p_deferred_list The deferred list whose count is cleared.
 */
[wbe::kernel(1, 1, 1)]
void svo_deferred_count_reset(
    [wbe::bind(SVO_DEFERRED_COUNT_RESET_LIST_ID, "write")] RWStructuredBuffer<uint> p_deferred_list) {
    p_deferred_list[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;
}

// The deferred lists svo_build_deferred_propagation() reads and writes: it reads pong and writes ping with
// WBE_USE_PING_WRITE, and reads ping and writes pong otherwise.
#ifdef WBE_USE_PING_WRITE
#define SVO_BUILD_DEFERRED_PROPAGATION_READ_LIST_ID "svo_deferred_list_pong"
#define SVO_BUILD_DEFERRED_PROPAGATION_WRITE_LIST_ID "svo_deferred_list_ping"
#else
#define SVO_BUILD_DEFERRED_PROPAGATION_READ_LIST_ID "svo_deferred_list_ping"
#define SVO_BUILD_DEFERRED_PROPAGATION_WRITE_LIST_ID "svo_deferred_list_pong"
#endif

/**
 * @brief Retries the allocations of the deferred list read this pass, deferring the contended ones to the written list.
 * @details The list may exceed one dispatch's thread count, so each invocation strides over the whole dispatch.
 * WBE_DYNAMIC_BUILD allocates dynamic children instead of static ones.
 * @param p_dispatch_thread_id The invocation's index in the dispatch.
 * @param p_svo_info The SVO info.
 * @param p_tree_nodes The SVO's nodes.
 * @param p_tree_leaves The SVO's leaves.
 * @param p_deferred_list_read The deferred list retried this pass.
 * @param p_deferred_list_write The deferred list receiving the allocations that fail again.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(128, 1, 1)]
void svo_build_deferred_propagation(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("svo_info")] RWStructuredBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_node_buffer")] RWStructuredBuffer<SVONode> p_tree_nodes,
    [wbe::bind("svo_leaf_buffer")] RWStructuredBuffer<SVOLeaf> p_tree_leaves,
    [wbe::bind(SVO_BUILD_DEFERRED_PROPAGATION_READ_LIST_ID)] StructuredBuffer<uint> p_deferred_list_read,
    [wbe::bind(SVO_BUILD_DEFERRED_PROPAGATION_WRITE_LIST_ID)] RWStructuredBuffer<uint> p_deferred_list_write,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    uint task_stride = WorkgroupCount().x * 128;
    uint deferred_task_count = p_deferred_list_read[SVO_DEFERRED_LIST_COUNT_INDEX];
    for (uint task_index = p_dispatch_thread_id.x; task_index < deferred_task_count; task_index += task_stride) {
        uint task = p_deferred_list_read[SVO_DEFERRED_LIST_ENTRY_OFFSET + task_index];

        SVONodeTraceInfo trace_info = to_trace_info(task, p_svo_info[0].max_depth);
        // Try to allocate the node.
#ifdef WBE_DYNAMIC_BUILD
        if (!try_dynamic_voxel_allocate_atomic(p_tree_nodes, p_tree_leaves, p_svo_info, trace_info, p_frame_slot)) {
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
 * @brief Restores static parents and discards the previous frame's dynamic allocation ranges.
 * @details Leaf restoration uses its saved block directly, never the concurrently reset dynamic count.
 * A graph dependency must complete this entire dispatch before any dynamic allocation begins.
 * @param p_id The invocation's index in the dispatch, one per node.
 * @param p_info The SVO info.
 * @param p_nodes The SVO's nodes.
 * @param p_deferred_list_ping The ping deferred list, whose count is cleared.
 * @param p_deferred_list_pong The pong deferred list, whose count is cleared.
 */
[wbe::kernel(128, 1, 1)]
void svo_clear_dynamic(uint3 p_id : SV_DispatchThreadID,
    [wbe::bind("svo_info")] RWStructuredBuffer<SVOInfo> p_info,
    [wbe::bind("svo_node_buffer")] RWStructuredBuffer<SVONode> p_nodes,
    [wbe::bind("svo_deferred_list_ping", "write")] RWStructuredBuffer<uint> p_deferred_list_ping,
    [wbe::bind("svo_deferred_list_pong", "write")] RWStructuredBuffer<uint> p_deferred_list_pong) {
    uint index = p_id.x;
    if (index < p_info[0].node_count) {
        uint description = p_nodes[index].children_data;
        uint state = description >> SVO_CHILDREN_STATE_SHIFT;
        if (state == uint(SVONode::ChildrenState::NODE_DYNAMIC)) {
            p_nodes[index].children_data = uint(SVONode::ChildrenState::EMPTY) << SVO_CHILDREN_STATE_SHIFT;
        } else if (state == uint(SVONode::ChildrenState::LEAF_DYNAMIC)) {
            p_nodes[index].children_data = p_nodes[index].original_children_data;
        }
    }
    if (index == 0) {
        p_info[0].dynamic_node_count = 0;
        p_info[0].dynamic_leaves_count = 0;
        p_deferred_list_ping[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;
        p_deferred_list_pong[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;
    }
}

/**
 * @brief Captures the SVO state produced by the raster passes before deferred propagation changes it.
 * @details The list may exceed one dispatch's thread count, so each invocation strides over the whole dispatch.
 * @param p_dispatch_thread_id The invocation's index in the dispatch.
 * @param p_svo_info The SVO info.
 * @param p_deferred_list The ping deferred list captured.
 * @param p_captured_svo_info Receives the SVO info.
 * @param p_captured_deferred_list Receives the deferred list's entries.
 * @param p_indirect_draw Receives the indexed-indirect command drawing the captured leaves and deferred tasks.
 */
[wbe::kernel(128, 1, 1)]
void svo_pre_deferred_debug_capture(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("svo_info")] ConstantBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_deferred_list_ping")] StructuredBuffer<uint> p_deferred_list,
    [wbe::bind("svo_pre_deferred_debug_info_buffer")] RWStructuredBuffer<SVOInfo> p_captured_svo_info,
    [wbe::bind("svo_pre_deferred_debug_list_buffer")] RWStructuredBuffer<uint> p_captured_deferred_list,
    [wbe::bind("svo_pre_deferred_debug_draw_indirect_buffer")]
    RWStructuredBuffer<DrawIndexedIndirectCommand> p_indirect_draw) {
    SVOInfo info = p_svo_info;
    uint deferred_task_count = p_deferred_list[SVO_DEFERRED_LIST_COUNT_INDEX];
    if (p_dispatch_thread_id.x == 0) {
        p_captured_svo_info[0] = info;
        DrawIndexedIndirectCommand command;
        command.index_count = 24;
        command.instance_count = info.leaves_count + info.dynamic_leaves_count + deferred_task_count;
        command.first_index = 0;
        command.vertex_offset = 0;
        command.first_instance = 0;
        p_indirect_draw[0] = command;
    }
    uint task_stride = WorkgroupCount().x * 128;
    for (uint task_index = p_dispatch_thread_id.x; task_index < deferred_task_count; task_index += task_stride) {
        p_captured_deferred_list[task_index] = p_deferred_list[SVO_DEFERRED_LIST_ENTRY_OFFSET + task_index];
    }
}

/**
 * @brief Builds the indexed-indirect command of the post-construction debug view at the drawn depth.
 * @details At the leaves, every leaf instance is followed by the tree's bounds. At a node depth, instance zero draws the
 * bounds and each further instance a node of the node list: the root's sectors with children, whose indices are fixed,
 * are written here, and svo_debug_draw_find_nodes() appends the nodes of the other depths.
 * @param p_svo_info The SVO info.
 * @param p_indirect_draw Receives the indexed-indirect command.
 * @param p_tree_nodes The SVO's nodes.
 * @param p_settings The debug view's settings.
 * @param p_node_list Receives the root's sectors with children at node depths.
 */
[wbe::kernel(1, 1, 1)]
void svo_debug_draw_setup([wbe::bind("svo_info")] ConstantBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_debug_draw_indirect_buffer")] RWStructuredBuffer<DrawIndexedIndirectCommand> p_indirect_draw,
    [wbe::bind("svo_node_buffer")] StructuredBuffer<SVONode> p_tree_nodes,
    [wbe::bind("svo_debug_draw_settings")] ConstantBuffer<SVODebugDrawSettings> p_settings,
    [wbe::bind("svo_debug_draw_node_list", "write")] RWStructuredBuffer<uint> p_node_list) {
    DrawIndexedIndirectCommand command;
    // The edges of VOXEL_DEBUG_CUBE_INDICES, or the faces of VOXEL_DEBUG_CUBE_SOLID_INDICES.
    command.index_count = p_settings.style == 0 ? 24 : 36;
    if (p_settings.depth + 1 >= p_svo_info.max_depth) {
        command.instance_count = p_svo_info.leaves_count + p_svo_info.dynamic_leaves_count + 1;
    } else {
        command.instance_count = 1;
        if (p_settings.depth == 0) {
            [unroll] for (uint i = 0; i < 8; ++i) {
                if (p_tree_nodes[i].get_children_state() != SVONode::ChildrenState::EMPTY) {
                    p_node_list[command.instance_count - 1] = i;
                    ++command.instance_count;
                }
            }
        }
    }
    command.first_index = 0;
    command.vertex_offset = 0;
    command.first_instance = 0;
    p_indirect_draw[0] = command;
}

/**
 * @brief Whether a node has children: stored, dynamic, or leaf ones.
 * @param p_node The node.
 * @return True when the node's children state is NODE, NODE_DYNAMIC, LEAF, or LEAF_DYNAMIC.
 */
bool svo_debug_draw_has_children(SVONode p_node) {
    SVONode::ChildrenState state = p_node.get_children_state();
    return state == SVONode::ChildrenState::NODE || state == SVONode::ChildrenState::NODE_DYNAMIC ||
        state == SVONode::ChildrenState::LEAF || state == SVONode::ChildrenState::LEAF_DYNAMIC;
}

/**
 * @brief Appends every node at the drawn depth to the debug draw's node list, after the instance svo_debug_draw_setup()
 * reserves for the bounds.
 * @details One invocation per block of eight sibling leaves starts at their shared parent and walks up one level at a
 * time. At each level only the invocation of the first sibling with children continues to the parent; every node with
 * children has a leaf block below it, so exactly one invocation reaches each node and appends it. Blocks the parent does not
 * reference, such as static blocks superseded by dynamic ones, are dropped. The root's sectors, whose indices are fixed,
 * and the leaves are drawn without this search.
 * @param p_thread_id The invocation's index in the dispatch, one per leaf block.
 * @param p_svo_info The SVO info.
 * @param p_tree_nodes The SVO's nodes.
 * @param p_tree_leaves The SVO's leaves.
 * @param p_settings The debug view's settings.
 * @param p_indirect_draw The indexed-indirect command, whose instance count is incremented per appended node.
 * @param p_node_list Receives the nodes at the drawn depth.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(64, 1, 1)]
void svo_debug_draw_find_nodes(uint p_thread_id : SV_DispatchThreadID,
    [wbe::bind("svo_info")] ConstantBuffer<SVOInfo> p_svo_info,
    [wbe::bind("svo_node_buffer")] StructuredBuffer<SVONode> p_tree_nodes,
    [wbe::bind("svo_leaf_buffer")] StructuredBuffer<SVOLeaf> p_tree_leaves,
    [wbe::bind("svo_debug_draw_settings")] ConstantBuffer<SVODebugDrawSettings> p_settings,
    [wbe::bind("svo_debug_draw_indirect_buffer")] RWStructuredBuffer<DrawIndexedIndirectCommand> p_indirect_draw,
    [wbe::bind("svo_debug_draw_node_list", "write")] RWStructuredBuffer<uint> p_node_list,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    SVO svo = SVO(p_tree_nodes, p_tree_leaves, p_svo_info, true);

    uint target_depth = p_settings.depth + 1;
    if (target_depth <= 1 || target_depth >= p_svo_info.max_depth) {
        return;
    }
    uint leaf_block = p_thread_id * 8;
    if (leaf_block >= svo.get_total_leaves()) {
        return;
    }
    int parent_index = svo.get_leaf(leaf_block).parent_index;
    if (parent_index < 0) {
        return;
    }
    SVONode parent = svo.get_node(uint(parent_index));
    SVONode::ChildrenState expected_leaf_state =
        leaf_block >= p_svo_info.leaves_count ? SVONode::ChildrenState::LEAF_DYNAMIC : SVONode::ChildrenState::LEAF;
    if (parent.get_children_state() != expected_leaf_state || parent.get_children_index() != leaf_block) {
        return;
    }
    // Every leaf block has its own parent, at depth max_depth - 1.
    uint current = uint(parent_index);
    for (uint depth = p_svo_info.max_depth - 1; depth > target_depth; --depth) {
        uint sibling_block = current & ~7u;
        for (uint sibling = sibling_block; sibling < current; ++sibling) {
            if (svo_debug_draw_has_children(svo.get_node(sibling))) {
                return;
            }
        }
        SVONode current_node = svo.get_node(current);
        WBE_SHADER_DEBUG_ASSERT(current_node.has_parent(), ShaderDebugError::SVO_SHALLOWER_THAN_EXPECTED, p_frame_slot);
        if (!current_node.has_parent()) {
            return;
        }
        uint next_index = current_node.get_parent_index();
        SVONode next = svo.get_node(next_index);
        SVONode::ChildrenState expected_node_state =
            sibling_block >= p_svo_info.node_count ? SVONode::ChildrenState::NODE_DYNAMIC : SVONode::ChildrenState::NODE;
        if (next.get_children_state() != expected_node_state || next.get_children_index() != sibling_block) {
            return;
        }
        current = next_index;
    }
    uint instance;
    InterlockedAdd(p_indirect_draw[0].instance_count, 1, instance);
    // Instance zero draws the bounds.
    p_node_list[instance - 1] = current;
}

#endif
