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
     * @brief Atomically published child description.
     * @details Bits 31:30 encode NODE=00, EMPTY=01, ALLOCATING=10, LEAF=11.
     * LEAF uses bits 29:22 for occupancy (bit 22 is sector zero) and bits 21:0 for an eight-leaf block index.
     * NODE uses bits 21:0 for an eight-node block index; bits 29:22 are unused.
     * EMPTY and ALLOCATING ignore the lower thirty bits.
     */
    uint children_data;
    /**
     * @index of the data in the data buffer.
     */
    uint data_index;
    /**
     * @brief The index to the parent node. < 0 if no parent present, which represents
     * this node as one of the implicit root's eight sectors.
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
     * @brief Index of the stored node owning this eight-leaf block; always nonnegative.
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
 * @brief Decodes the state from an atomic snapshot of a node's child description.
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
    WBE_SHADER_DEBUG_ASSERT(parent_index >= 0 && uint(parent_index) < p_tree_info.node_count,
        ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
    return (p_nodes[parent_index].children_data & (1u << (22 + p_leaf_index % 8))) != 0;
}

/**
 * @brief Loads an octree node after acquiring its published child description.
 * @details The barrier makes initialization preceding NODE or LEAF publication visible to the reader.
 */
OctTreeNode load_oct_tree_node(RWStructuredBuffer<OctTreeNode> p_nodes, uint p_node_index) {
    uint children_data;
    InterlockedAdd(p_nodes[p_node_index].children_data, 0, children_data);
    OctTreeNode::ChildrenState state = OctTreeNode::ChildrenState(children_data >> 30);
    if (state == OctTreeNode::ChildrenState::NODE || state == OctTreeNode::ChildrenState::LEAF) {
        DeviceMemoryBarrier();
    }
    OctTreeNode result;
    result.children_data = children_data;
    result.data_index = p_nodes[p_node_index].data_index;
    result.parent_index = p_nodes[p_node_index].parent_index;
    return result;
}

/**
 * @brief Publishes initialized children and their block index as one atomic description.
 * @details The barrier makes initialization of all eight children visible before NODE or LEAF publication.
 */
void publish_oct_tree_children_data(RWStructuredBuffer<OctTreeNode> p_nodes, uint p_node_index, uint p_children_data) {
    DeviceMemoryBarrier();
    uint original_data;
    InterlockedExchange(p_nodes[p_node_index].children_data, p_children_data, original_data);
}

/**
 * @brief Marks just the intersected leaf occupied, preserving concurrent insertions into the same block.
 */
void occupy_oct_tree_leaf(RWStructuredBuffer<OctTreeNode> p_nodes, uint p_node_index, float3 p_position) {
    uint original_data;
    InterlockedOr(p_nodes[p_node_index].children_data, 1u << (22 + get_oct_tree_node_subregion(p_position)), original_data);
}

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

bool try_reserve_oct_tree_nodes(RWStructuredBuffer<OctTreeInfo> p_tree_info, uint p_frame_slot, out uint p_children_index) {
    uint current_count = p_tree_info[0].node_count;
    for (;;) {
        WBE_SHADER_DEBUG_ASSERT(current_count / 8 <= 0x003fffffu &&
            current_count <= p_tree_info[0].max_node_count &&
            p_tree_info[0].max_node_count - current_count >= 8,
            ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
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
        p_nodes[child_index].children_data = uint(OctTreeNode::ChildrenState::EMPTY) << 30;
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
        WBE_SHADER_DEBUG_ASSERT(current_count / 8 <= 0x003fffffu &&
            current_count <= p_tree_info[0].max_leaves_count &&
            p_tree_info[0].max_leaves_count - current_count >= 8,
            ShaderDebugError::OCT_TREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
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
    WBE_SHADER_DEBUG_ASSERT(p_node_index < p_tree_info[0].max_node_count,
        ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
    uint original;
    // Try swaping to ALLOCATE state
    InterlockedCompareExchange(
        p_nodes[p_node_index].children_data,
        uint(OctTreeNode::ChildrenState::EMPTY) << 30,
        uint(OctTreeNode::ChildrenState::ALLOCATING) << 30,
        original
    );
    if (original != (uint(OctTreeNode::ChildrenState::EMPTY) << 30)) {
        // CAS Failed. Allocation failed.
        return false;
    }
    // CAS succeeded. Allocate the child node:
    uint children_index;
    if (p_is_leaf) {
        if (!create_oct_tree_children_leaf(p_leaves, p_tree_info, p_node_index, p_frame_slot, children_index)) {
            publish_oct_tree_children_data(p_nodes, p_node_index, uint(OctTreeNode::ChildrenState::EMPTY) << 30);
            return false;
        }
        publish_oct_tree_children_data(
            p_nodes, p_node_index, (uint(OctTreeNode::ChildrenState::LEAF) << 30) | (children_index / 8));
    }
    else {
        if (!create_oct_tree_children_node(p_nodes, p_tree_info, p_node_index, p_frame_slot, children_index)) {
            publish_oct_tree_children_data(p_nodes, p_node_index, uint(OctTreeNode::ChildrenState::EMPTY) << 30);
            return false;
        }
        publish_oct_tree_children_data(p_nodes, p_node_index, children_index / 8);
    }
    return true;
}

bool try_voxel_allocate(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeLeaf> p_leaves,
    RWStructuredBuffer<OctTreeInfo> p_tree_info, inout OctTreeNodeTraceInfo p_trace_info, uint p_frame_slot) {
    uint node_index = p_trace_info.node_index;
    uint curr_depth = p_trace_info.curr_depth;
    WBE_SHADER_DEBUG_ASSERT(p_tree_info[0].max_depth >= 2 &&
        p_tree_info[0].max_node_count >= 8 &&
        p_tree_info[0].max_leaves_count != 0,
        ShaderDebugError::OCT_TREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
    for (int depth = 0; p_trace_info.remain_depth < 0 || depth < p_trace_info.remain_depth; ++depth) {
        WBE_SHADER_DEBUG_ASSERT(curr_depth < p_tree_info[0].max_depth,
            ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED, p_frame_slot);
        WBE_SHADER_DEBUG_ASSERT(node_index < p_tree_info[0].max_node_count,
            ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        OctTreeNode node = load_oct_tree_node(p_nodes, node_index);
        if (get_oct_tree_children_state(node) == OctTreeNode::ChildrenState::EMPTY) {
            if (!try_create_oct_tree_entry(
                    p_nodes, p_leaves, p_tree_info, node_index, curr_depth == p_tree_info[0].max_depth - 1, p_frame_slot)) {
                p_trace_info.node_index = node_index;
                p_trace_info.curr_depth = curr_depth;
                p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
                return false;
            }
            node = load_oct_tree_node(p_nodes, node_index);
        }
        OctTreeNode::ChildrenState child_state = get_oct_tree_children_state(node);
        switch (child_state) {
        case OctTreeNode::ChildrenState::NODE:
            // A node, keep on looking deeper.
            WBE_SHADER_DEBUG_ASSERT(curr_depth < p_tree_info[0].max_depth - 1,
                ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED, p_frame_slot);
            WBE_SHADER_DEBUG_ASSERT(get_oct_tree_children_index(node) <= p_tree_info[0].max_node_count &&
                p_tree_info[0].max_node_count - get_oct_tree_children_index(node) >= 8,
                ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
            break;
        case OctTreeNode::ChildrenState::LEAF:
            // Found a leaf, allocation success.
            WBE_SHADER_DEBUG_ASSERT(curr_depth >= p_tree_info[0].max_depth - 1,
                ShaderDebugError::OCT_TREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
            WBE_SHADER_DEBUG_ASSERT(curr_depth <= p_tree_info[0].max_depth - 1,
                ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED, p_frame_slot);
            WBE_SHADER_DEBUG_ASSERT(get_oct_tree_children_index(node) <= p_tree_info[0].max_leaves_count &&
                p_tree_info[0].max_leaves_count - get_oct_tree_children_index(node) >= 8,
                ShaderDebugError::OCT_TREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
            occupy_oct_tree_leaf(p_nodes, node_index, p_trace_info.svo_position);
            return true;
        case OctTreeNode::ChildrenState::ALLOCATING:
            // Traced to an empty/allocating/leaf node, set p_state and return.
            p_trace_info.node_index = node_index;
            p_trace_info.curr_depth = curr_depth;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            return false;
        default:
            WBE_SHADER_DEBUG_ASSERT(false, curr_depth < p_tree_info[0].max_depth - 1
                    ? ShaderDebugError::OCT_TREE_SHALLOWER_THAN_EXPECTED
                    : ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED,
                p_frame_slot);
            break;
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
