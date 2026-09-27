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
#ifndef WBE_FILE_OCT_TREE_2_UTILS_SLANG
#define WBE_FILE_OCT_TREE_2_UTILS_SLANG

/**
 * @brief Three scalar fields with a 12-byte storage-buffer array stride.
 */
[WBE_DATA_TYPE]
struct OctTreeNode {
    enum class ChildrenState {
        NODE = 0,
        EMPTY = 1,
        ALLOCATING = 2,
        LEAF = 3
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
     * @brief Child description published only between construction passes.
     * @details Bits 31:30 encode NODE=00, EMPTY=01, ALLOCATING=10, LEAF=11.
     * LEAF uses bits 29:22 for occupancy (bit 22 is sector zero) and bits 21:0 for an eight-leaf block index.
     * NODE uses bits 21:0 for an eight-node block index; bits 29:22 are unused.
     * EMPTY ignores the lower thirty bits. In the pending buffer, ALLOCATING retains the block index and
     * occupancy; zero occupancy identifies a node block and nonzero occupancy identifies a leaf block.
     */
    uint children_data;
    /**
     * @brief Index of the data in the data buffer.
     */
    uint data_index;
    /**
     * @brief The index to the parent node. < 0 if no parent present, which represents
     * this node as one of the implicit root's eight sectors.
     */
    int parent_index;
};

/**
 * @brief One initialized leaf slot; occupancy is published in its parent node.
 */
[WBE_DATA_TYPE]
struct OctTreeLeaf {
    /**
     * @brief Index of the data in the data buffer.
     */
    uint data_index;
    /**
     * @brief Index of the stored node owning this eight-leaf block; always nonnegative.
     */
    int parent_index;
};

/**
 * @brief Immutable spatial configuration and mutable construction counters.
 */
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
     * @brief The maximum depth of the oct tree, at least two; stored nodes start at depth one.
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
     * @brief The number of stored nodes, including the first eight sectors but excluding the implicit root.
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

/**
 * @brief One voxel traversal retained until the next publication boundary.
 */
[WBE_DATA_TYPE]
struct OctTreeDeferredListEntry {
    /**
     * @brief Parent-local traversal state to resume after publication.
     */
    OctTreeNodeTraceInfo unfinished_trace_info;
};

/**
 * @brief Selects one of eight sectors in parent-local coordinates.
 */
uint get_oct_tree_node_subregion(float3 p_position)
{
    return (uint(p_position.x >= 0.5) << 2) |
        (uint(p_position.y >= 0.5) << 1) |
        uint(p_position.z >= 0.5);
}

/**
 * @brief Traversal position retained across deferred construction rounds.
 */
struct OctTreeNodeTraceInfo {
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
 * @brief Decodes the state from a pass-stable node description.
 */
OctTreeNode::ChildrenState get_oct_tree_children_state(OctTreeNode p_node) {
    return OctTreeNode::ChildrenState(p_node.children_data >> 30);
}

/**
 * @brief Decodes the first child index for a NODE or LEAF description.
 */
uint get_oct_tree_children_index(OctTreeNode p_node) {
    return (p_node.children_data & 0x003fffffu) * 8;
}

/**
 * @brief Tests an allocated leaf slot's occupancy for debug drawing after construction.
 * @details Invalid parent links report a shader validation error.
 */
bool is_oct_tree_leaf_occupied(uint p_leaf_index,
    StructuredBuffer<OctTreeNode> p_nodes,
    StructuredBuffer<OctTreeLeaf> p_leaves,
    OctTreeInfo p_tree_info,
    uint p_frame_slot) {
    int parent_index = p_leaves[p_leaf_index].parent_index;
    if (parent_index < 0 || uint(parent_index) >= p_tree_info.node_count) {
        report_shader_debug_error(ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        return false;
    }
    return (p_nodes[parent_index].children_data & (1u << (22 + p_leaf_index % 8))) != 0;
}

/**
 * @brief Reads a description that remains immutable throughout a construction pass.
 * @details Newly initialized children become reachable only after the state-update dispatch and its memory dependency.
 */
OctTreeNode load_oct_tree_node(RWStructuredBuffer<OctTreeNode> p_nodes, uint p_node_index) {
    return p_nodes[p_node_index];
}

/**
 * @brief Marks occupancy in pending storage without modifying the published traversal description.
 */
void occupy_oct_tree_leaf(RWStructuredBuffer<uint> p_pending, uint p_node_index, float3 p_position) {
    uint original_data;
    InterlockedOr(p_pending[p_node_index], 1u << (22 + get_oct_tree_node_subregion(p_position)), original_data);
}

/**
 * @brief Traverses only published node blocks, stopping at empty nodes or leaves.
 */
void traverse_oct_tree(RWStructuredBuffer<OctTreeNode> p_nodes, inout OctTreeNodeTraceInfo p_trace_info) {
    uint node_index = p_trace_info.node_index;
    uint curr_depth = p_trace_info.curr_depth;
    for (int depth = 0; p_trace_info.remain_depth < 0 || depth < p_trace_info.remain_depth; ++depth) {
        OctTreeNode node = load_oct_tree_node(p_nodes, node_index);
        OctTreeNode::ChildrenState children_state = get_oct_tree_children_state(node);
        if (children_state != OctTreeNode::ChildrenState::NODE) {
            // Traced to an empty/allocating/leaf node, set p_state and return.
            p_trace_info.node_index = node_index;
            p_trace_info.curr_depth = curr_depth;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            return;
        }
        node_index = get_oct_tree_children_index(node) + get_oct_tree_node_subregion(p_trace_info.svo_position);
        ++curr_depth;
        // Remap from parent-local coordinates into child-local coordinates.
        p_trace_info.svo_position = frac(p_trace_info.svo_position * 2.0);
    }
    // We've traced a node.
    p_trace_info.curr_depth = curr_depth;
    p_trace_info.node_index = node_index;
    p_trace_info.remain_depth = 0;
}

/**
 * @brief Reserves an eight-node block or reports capacity exhaustion to shader validation.
 */
bool try_reserve_oct_tree_nodes(RWStructuredBuffer<OctTreeInfo> p_tree_info, uint p_frame_slot, out uint p_children_index) {
    uint current_count;
    InterlockedAdd(p_tree_info[0].node_count, 0, current_count);
    for (;;) {
        if (current_count / 8 > 0x003fffffu || current_count > p_tree_info[0].max_node_count ||
            p_tree_info[0].max_node_count - current_count < 8) {
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

/**
 * @brief Initializes an unreachable eight-node block and its pending descriptions.
 */
bool create_oct_tree_children_node(RWStructuredBuffer<OctTreeNode> p_nodes,
    RWStructuredBuffer<uint> p_pending,
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
        p_nodes[child_index].children_data = uint(OctTreeNode::ChildrenState::EMPTY) << 30;
        p_pending[child_index] = uint(OctTreeNode::ChildrenState::EMPTY) << 30;
        p_nodes[child_index].parent_index = p_parent_node_index;
    }
    uint new_data_index;
    InterlockedAdd(p_tree_info[0].data_count, 8, new_data_index);
    [unroll] for (uint i = 0; i < 8; ++i) {
        p_nodes[p_children_index + i].data_index = new_data_index + i;
    }
    return true;
}

/**
 * @brief Reserves an eight-leaf block or reports capacity exhaustion to shader validation.
 */
bool try_reserve_oct_tree_leaves(RWStructuredBuffer<OctTreeInfo> p_tree_info, uint p_frame_slot, out uint p_children_index) {
    uint current_count;
    InterlockedAdd(p_tree_info[0].leaves_count, 0, current_count);
    for (;;) {
        if (current_count / 8 > 0x003fffffu || current_count > p_tree_info[0].max_leaves_count ||
            p_tree_info[0].max_leaves_count - current_count < 8) {
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

/**
 * @brief Initializes an unreachable eight-leaf block before inter-pass publication.
 */
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

/**
 * @brief Claims an empty node and prepares children for the next state-update pass.
 * @details Only the CAS winner initializes children. Contenders never inspect pending payloads.
 * A leaf payload includes the creator's occupancy bit so zero occupancy unambiguously means a node block.
 */
bool try_create_oct_tree_entry(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeLeaf> p_leaves,
    RWStructuredBuffer<uint> p_pending, RWStructuredBuffer<OctTreeInfo> p_tree_info,
    uint p_node_index, bool p_is_leaf, float3 p_position, uint p_frame_slot) {
    uint original;
    InterlockedCompareExchange(p_pending[p_node_index],
        uint(OctTreeNode::ChildrenState::EMPTY) << 30,
        uint(OctTreeNode::ChildrenState::ALLOCATING) << 30, original);
    if (original != (uint(OctTreeNode::ChildrenState::EMPTY) << 30)) {
        return false;
    }
    uint children_index;
    bool allocated;
    if (p_is_leaf) {
        allocated = create_oct_tree_children_leaf(p_leaves, p_tree_info, p_node_index, p_frame_slot, children_index);
    }
    else {
        allocated = create_oct_tree_children_node(p_nodes, p_pending, p_tree_info, p_node_index, p_frame_slot, children_index);
    }
    if (!allocated) {
        InterlockedExchange(p_pending[p_node_index], uint(OctTreeNode::ChildrenState::EMPTY) << 30, original);
        return false;
    }
    uint occupancy = p_is_leaf ? (1u << (22 + get_oct_tree_node_subregion(p_position))) : 0;
    uint pending_data = (uint(OctTreeNode::ChildrenState::ALLOCATING) << 30) | occupancy | (children_index / 8);
    // Other invocations may still CAS this word. Publication and visibility are provided by the later dispatch boundary.
    InterlockedExchange(p_pending[p_node_index], pending_data, original);
    return true;
}

/**
 * @brief Marks a published leaf occupied or saves traversal for the next construction round.
 */
bool try_voxel_allocate(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeLeaf> p_leaves,
    RWStructuredBuffer<uint> p_pending, RWStructuredBuffer<OctTreeInfo> p_tree_info,
    inout OctTreeNodeTraceInfo p_trace_info, uint p_frame_slot) {
    uint node_index = p_trace_info.node_index;
    uint curr_depth = p_trace_info.curr_depth;
    if (p_tree_info[0].max_depth < 2 || p_tree_info[0].max_node_count < 8 || p_tree_info[0].max_leaves_count == 0) {
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
        if (get_oct_tree_children_state(node) == OctTreeNode::ChildrenState::EMPTY) {
            try_create_oct_tree_entry(p_nodes, p_leaves, p_pending, p_tree_info, node_index,
                curr_depth == p_tree_info[0].max_depth - 1, p_trace_info.svo_position, p_frame_slot);
            // Even the owner retries from this node after publication; no invocation descends into new children this pass.
            p_trace_info.node_index = node_index;
            p_trace_info.curr_depth = curr_depth;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            return false;
        }
        OctTreeNode::ChildrenState child_state = get_oct_tree_children_state(node);
        switch (child_state) {
        case OctTreeNode::ChildrenState::NODE:
            // A node, keep on looking deeper.
            if (curr_depth >= p_tree_info[0].max_depth - 1) {
                report_shader_debug_error(ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED, p_frame_slot);
                return false;
            }
            if (get_oct_tree_children_index(node) > p_tree_info[0].max_node_count ||
                p_tree_info[0].max_node_count - get_oct_tree_children_index(node) < 8) {
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
            if (get_oct_tree_children_index(node) > p_tree_info[0].max_leaves_count ||
                p_tree_info[0].max_leaves_count - get_oct_tree_children_index(node) < 8) {
                report_shader_debug_error(ShaderDebugError::OCT_TREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
                return false;
            }
            occupy_oct_tree_leaf(p_pending, node_index, p_trace_info.svo_position);
            return true;
        case OctTreeNode::ChildrenState::ALLOCATING:
            // Traced to an empty/allocating/leaf node, set p_state and return.
            p_trace_info.node_index = node_index;
            p_trace_info.curr_depth = curr_depth;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            return false;
        default:
            report_shader_debug_error(curr_depth < p_tree_info[0].max_depth - 1
                    ? ShaderDebugError::OCT_TREE_SHALLOWER_THAN_EXPECTED
                    : ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED,
                p_frame_slot);
            return false;
        }
        uint child_index = get_oct_tree_children_index(node) + get_oct_tree_node_subregion(p_trace_info.svo_position);
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
