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
#ifndef WBE_FILE_OCT_TREE_UTILS_SLANG
#define WBE_FILE_OCT_TREE_UTILS_SLANG

[WBE_DATA_TYPE]
struct OctTreeNode {
    enum class ChildrenState {
        NODE,
        LEAF,
        EMPTY,
        ALLOCATING
    };
    enum Subregion {
        XYZ_000,
        XYZ_001,
        XYZ_010,
        XYZ_011,
        XYZ_100,
        XYZ_101,
        XYZ_110,
        XYZ_111,
        MAX_SUBREGION
    };
    /**
     * @brief The children's state.
     * We have to atomically operate on this, but Slang does not support
     * atomic operation on enums (I sincerrily don't understand why. Aren't
     * they integers basically?) So we couldn't use ChildrenState type but
     * have to use uint.
     */
    uint children_state;
    /**
     * @brief Children's chunk start index.
     */
    uint children;
    /**
     * @index of the data in the data buffer.
     */
    uint data_index;
    /**
     * @brief The index to the parent node. < 0 if no parent present, which represents
     * this node as the root node.
     */
    int parent_index;
};

[WBE_DATA_TYPE]
struct OctTreeLeaf {
    /**
     * @index of the data in the data buffer.
     */
    uint data_index;
    /**
     * @brief The index to the parent node. < 0 if no parent present, which represents
     * this node as the root node.
     */
    int parent_index;
};

[WBE_DATA_TYPE]
struct OctTreeInfo {
    /**
     * @brief The origin of the oct tree in world space.
     */
    float3 origin;
    /**
     * @brief The width of the region this oct tree is covering.
     */
    float width;
    /**
     * @brief The maximum depth of the oct tree.
     */
    uint max_depth;
    /**
     * @brief The capacity of the node buffer.
     */
    uint max_node_count;
    /**
     * @brief The capacity of the leaf buffer.
     */
    uint max_leaves_count;
    /**
     * @brief The number of nodes.
     */
    uint node_count;
    /**
     * @brief The number of leaves.
     */
    uint leaves_count;
    /**
     * @brief The number of data blocks.
     */
    uint data_count;
    /**
     * @brief The number of deferred tasks in the ping list.
     */
    uint deferred_task_count_ping;
    /**
     * @brief The number of deferred tasks in the pong list.
     */
    uint deferred_task_count_pong;
    /**
     * @brief The maximum number of deferred tasks the list can hold.
     */
    uint max_deferred_task_count;
};

[WBE_DATA_TYPE]
struct OctTreeDeferredListEntry {
    OctTreeNodeTraceInfo unfinished_trace_info;
};

uint get_oct_tree_node_subregion(float3 p_position)
{
    return (uint(p_position.x >= 0.5) << 2) |
        (uint(p_position.y >= 0.5) << 1) |
        uint(p_position.z >= 0.5);
}

struct OctTreeNodeTraceInfo {
    // The index of the node.
    uint node_index;
    // The current depth.
    uint curr_depth;
    // The remain depth.
    int remain_depth;
    // The position in the node's svo space.
    float3 svo_position;
};

/**
 * @brief Loads an octree node after acquiring its published child state.
 * @details Reserving a chunk of eight children prevents overlapping allocations, but does not make the initializing invocation's
 * writes visible to other invocations. Once NODE or LEAF is observed, the barrier makes the child chunk and children index written
 * before publication visible before they are read below.
 */
OctTreeNode load_oct_tree_node(RWStructuredBuffer<OctTreeNode> p_nodes, uint p_node_index) {
    uint children_state;
    InterlockedAdd(p_nodes[p_node_index].children_state, 0, children_state);
    if (children_state == uint(OctTreeNode::ChildrenState::NODE) || children_state == uint(OctTreeNode::ChildrenState::LEAF)) {
        DeviceMemoryBarrier();
    }
    OctTreeNode result = p_nodes[p_node_index];
    result.children_state = children_state;
    return result;
}

/**
 * @brief Publishes an octree node's initialized children to other shader invocations.
 * @details The barrier orders initialization of all eight children and the parent's children index before the atomic state update.
 * Readers may therefore treat an observed NODE or LEAF state as a guarantee that the entire child chunk is visible.
 */
void publish_oct_tree_children_state(
    RWStructuredBuffer<OctTreeNode> p_nodes, uint p_node_index, OctTreeNode::ChildrenState p_children_state) {
    DeviceMemoryBarrier();
    uint original_state;
    InterlockedExchange(p_nodes[p_node_index].children_state, uint(p_children_state), original_state);
}

void traverse_oct_tree(RWStructuredBuffer<OctTreeNode> p_nodes, inout OctTreeNodeTraceInfo p_trace_info) {
    uint node_index = p_trace_info.node_index;
    uint curr_depth = p_trace_info.curr_depth;
    for (int depth = 0; p_trace_info.remain_depth < 0 || depth < p_trace_info.remain_depth; ++depth) {
        OctTreeNode node = load_oct_tree_node(p_nodes, node_index);
        OctTreeNode::ChildrenState children_state = OctTreeNode::ChildrenState(node.children_state);
        if (children_state != OctTreeNode::ChildrenState::NODE) {
            // Traced to an empty/allocating/leaf node, set p_state and return.
            p_trace_info.node_index = node_index;
            p_trace_info.curr_depth = curr_depth;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            return;
        }
        node_index = node.children + get_oct_tree_node_subregion(p_trace_info.svo_position);
        ++curr_depth;
        // Remap from parent-local coordinates into child-local coordinates.
        p_trace_info.svo_position = frac(p_trace_info.svo_position * 2.0);
    }
    // We've traced a node.
    p_trace_info.curr_depth = curr_depth;
    p_trace_info.node_index = node_index;
    p_trace_info.remain_depth = 0;
}

bool try_reserve_oct_tree_nodes(RWStructuredBuffer<OctTreeInfo> p_tree_info, uint p_frame_slot, out uint p_children_index) {
    uint current_count = p_tree_info[0].node_count;
    for (;;) {
        if (current_count > p_tree_info[0].max_node_count || p_tree_info[0].max_node_count - current_count < 8) {
            report_shader_debug_error(ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
            return false;
        }
        uint original_count;
        InterlockedCompareExchange(p_tree_info[0].node_count, current_count, current_count + 8, original_count);
        if (original_count == current_count) {
            p_children_index = current_count;
            return true;
        }
        current_count = original_count;
    }
}

bool create_oct_tree_children_node(RWStructuredBuffer<OctTreeNode> p_nodes,
    RWStructuredBuffer<OctTreeInfo> p_tree_info,
    uint p_parent_node_index,
    uint p_frame_slot,
    out uint p_children_index) {
    if (!try_reserve_oct_tree_nodes(p_tree_info, p_frame_slot, p_children_index)) {
        return false;
    }
    [unroll] for (uint i = 0; i < 8; ++i) {
        // Initialize all of the children to empty
        uint child_index = p_children_index + i;
        p_nodes[child_index].children_state = uint(OctTreeNode::ChildrenState::EMPTY);
        p_nodes[child_index].parent_index = p_parent_node_index;
    }
    uint new_data_index;
    InterlockedAdd(p_tree_info[0].data_count, 8, new_data_index);
    [unroll] for (uint i = 0; i < 8; ++i) {
        p_nodes[p_children_index + i].data_index = new_data_index + i;
    }
    return true;
}

bool try_reserve_oct_tree_leaves(RWStructuredBuffer<OctTreeInfo> p_tree_info, uint p_frame_slot, out uint p_children_index) {
    uint current_count = p_tree_info[0].leaves_count;
    for (;;) {
        if (current_count > p_tree_info[0].max_leaves_count || p_tree_info[0].max_leaves_count - current_count < 8) {
            report_shader_debug_error(ShaderDebugError::OCT_TREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
            return false;
        }
        uint original_count;
        InterlockedCompareExchange(p_tree_info[0].leaves_count, current_count, current_count + 8, original_count);
        if (original_count == current_count) {
            p_children_index = current_count;
            return true;
        }
        current_count = original_count;
    }
}

bool create_oct_tree_children_leaf(RWStructuredBuffer<OctTreeLeaf> p_leaves,
    RWStructuredBuffer<OctTreeInfo> p_tree_info,
    uint p_parent_node_index,
    uint p_frame_slot,
    out uint p_children_index) {
    if (!try_reserve_oct_tree_leaves(p_tree_info, p_frame_slot, p_children_index)) {
        return false;
    }
    [unroll] for (uint i = 0; i < 8; ++i) {
        uint child_index = p_children_index + i;
        p_leaves[child_index].parent_index = p_parent_node_index;
    }
    uint new_data_index;
    InterlockedAdd(p_tree_info[0].data_count, 8, new_data_index);
    [unroll] for (uint i = 0; i < 8; ++i) {
        p_leaves[p_children_index + i].data_index = new_data_index + i;
    }
    return true;
}

bool try_create_oct_tree_entry(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeLeaf> p_leaves,
    RWStructuredBuffer<OctTreeInfo> p_tree_info,
    uint p_node_index, bool p_is_leaf, uint p_frame_slot) {
    if (p_node_index >= p_tree_info[0].max_node_count) {
        report_shader_debug_error(ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        return false;
    }
    uint original;
    // Try swaping to ALLOCATE state
    InterlockedCompareExchange(
        p_nodes[p_node_index].children_state,
        uint(OctTreeNode::ChildrenState::EMPTY),
        uint(OctTreeNode::ChildrenState::ALLOCATING),
        original
    );
    if (original != uint(OctTreeNode::ChildrenState::EMPTY)) {
        // CAS Failed. Allocation failed.
        return false;
    }
    // CAS succeeded. Allocate the child node:
    uint children_index;
    if (p_is_leaf) {
        if (!create_oct_tree_children_leaf(p_leaves, p_tree_info, p_node_index, p_frame_slot, children_index)) {
            publish_oct_tree_children_state(p_nodes, p_node_index, OctTreeNode::ChildrenState::EMPTY);
            return false;
        }
        p_nodes[p_node_index].children = children_index;
        publish_oct_tree_children_state(p_nodes, p_node_index, OctTreeNode::ChildrenState::LEAF);
    }
    else {
        if (!create_oct_tree_children_node(p_nodes, p_tree_info, p_node_index, p_frame_slot, children_index)) {
            publish_oct_tree_children_state(p_nodes, p_node_index, OctTreeNode::ChildrenState::EMPTY);
            return false;
        }
        p_nodes[p_node_index].children = children_index;
        publish_oct_tree_children_state(p_nodes, p_node_index, OctTreeNode::ChildrenState::NODE);
    }
    return true;
}

bool try_voxel_allocate(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeLeaf> p_leaves,
    RWStructuredBuffer<OctTreeInfo> p_tree_info, inout OctTreeNodeTraceInfo p_trace_info, uint p_frame_slot) {
    uint node_index = p_trace_info.node_index;
    uint curr_depth = p_trace_info.curr_depth;
    if (p_tree_info[0].max_depth == 0 || p_tree_info[0].max_node_count == 0 || p_tree_info[0].max_leaves_count == 0) {
        report_shader_debug_error(ShaderDebugError::OCT_TREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
        return false;
    }
    for (int depth = 0; p_trace_info.remain_depth < 0 || depth < p_trace_info.remain_depth; ++depth) {
        if (curr_depth >= p_tree_info[0].max_depth) {
            report_shader_debug_error(ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED, p_frame_slot);
            return false;
        }
        if (node_index >= p_tree_info[0].max_node_count) {
            report_shader_debug_error(ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
            return false;
        }
        OctTreeNode node = load_oct_tree_node(p_nodes, node_index);
        OctTreeNode::ChildrenState child_state = OctTreeNode::ChildrenState(node.children_state);
        switch (child_state) {
        case OctTreeNode::ChildrenState::NODE:
            // A node, keep on looking deeper.
            if (curr_depth >= p_tree_info[0].max_depth - 1) {
                report_shader_debug_error(ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED, p_frame_slot);
                return false;
            }
            if (node.children > p_tree_info[0].max_node_count || p_tree_info[0].max_node_count - node.children < 8) {
                report_shader_debug_error(ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
                return false;
            }
            break;
        case OctTreeNode::ChildrenState::LEAF:
            // Found a leaf, allocation success.
            if (curr_depth < p_tree_info[0].max_depth - 1) {
                report_shader_debug_error(ShaderDebugError::OCT_TREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
                return false;
            }
            if (curr_depth > p_tree_info[0].max_depth - 1) {
                report_shader_debug_error(ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED, p_frame_slot);
                return false;
            }
            if (node.children > p_tree_info[0].max_leaves_count || p_tree_info[0].max_leaves_count - node.children < 8) {
                report_shader_debug_error(ShaderDebugError::OCT_TREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
                return false;
            }
            return true;
        case OctTreeNode::ChildrenState::ALLOCATING:
            // Traced to an empty/allocating/leaf node, set p_state and return.
            p_trace_info.node_index = node_index;
            p_trace_info.curr_depth = curr_depth;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            return false;
        case OctTreeNode::ChildrenState::EMPTY:
            // Try to allocate if found child empty.
            if (!try_create_oct_tree_entry(
                    p_nodes, p_leaves, p_tree_info, node_index, curr_depth == p_tree_info[0].max_depth - 1, p_frame_slot)) {
                // If allocation failed, add this task to the deferred list.
                p_trace_info.node_index = node_index;
                p_trace_info.curr_depth = curr_depth;
                p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
                return false;
            }
            node = load_oct_tree_node(p_nodes, node_index);
            break;
        default:
            report_shader_debug_error(curr_depth < p_tree_info[0].max_depth - 1
                    ? ShaderDebugError::OCT_TREE_SHALLOWER_THAN_EXPECTED
                    : ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED,
                p_frame_slot);
            return false;
        }
        uint child_index = node.children + get_oct_tree_node_subregion(p_trace_info.svo_position);
        node_index = child_index;
        ++curr_depth;
        // Remap from parent-local coordinates into child-local coordinates.
        p_trace_info.svo_position = frac(p_trace_info.svo_position * 2.0);
    }
    // We've traced a node.
    p_trace_info.node_index = node_index;
    p_trace_info.curr_depth = curr_depth;
    p_trace_info.remain_depth = 0;
    return true;
}

#endif
