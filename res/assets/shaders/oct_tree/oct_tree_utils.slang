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
    static const uint STATE_MASK = 0xC0000000u;
    static const uint INDEX_MASK = 0x3FFFFFFFu;
    static const uint32_t STATE_SHIFT = 30;
    enum class ChildState {
        NODE = 0b00 << STATE_SHIFT,
        EMPTY = 0b01 << STATE_SHIFT,
        ALLOCATING = 0b10 << STATE_SHIFT,
        LEAF = 0b11 << STATE_SHIFT,
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
     * @brief Children info. The 2 most significant bits are used as state tracking.
     * For EMPTY and ALLOCATING states, the remained bits must be 0s. For NODE state,
     * the rest of the bits represents the index in the corresponding node in the node
     * array. For LEAF, the rest of the bits represents the index of the corresponding
     * leaf in the leaf array.
     */
    uint children[Subregion::MAX_SUBREGION];
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
     * @brief three directional matrices for oct tree setup.
     */
    float4x4 matrices[3];
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
     * @brief The stride for data blocks. Must be a multiple of 4.
     */
    uint data_stride;
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
     * @brief The number of deferred tasks.
     */
    uint deferred_task_count;
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
    // The state of the next deeper node. If remain_depth is 0, this will be set to 0.
    OctTreeNode::ChildState state;
    // The index of the node.
    uint node_index;
    // The child index, if the traverse path is blocked.
    uint child_index;
    // The position in the node's svo space.
    float3 svo_position;
    // The current depth.
    uint curr_depth;
    // The remain depth.
    int remain_depth;
};

void traverse_oct_tree(RWStructuredBuffer<OctTreeNode> p_nodes, inout OctTreeNodeTraceInfo p_trace_info) {
    uint node_index = p_trace_info.node_index;
    for (int depth = 0; p_trace_info.remain_depth < 0 || depth < p_trace_info.remain_depth; ++depth) {
        OctTreeNode node = p_nodes[node_index];
        uint child_index = get_oct_tree_node_subregion(p_trace_info.svo_position);
        uint entry = node.children[child_index];
        OctTreeNode::ChildState child_state = (OctTreeNode::ChildState)(entry & OctTreeNode::STATE_MASK);
        if (child_state != OctTreeNode::ChildState::NODE) {
            // Traced to an empty/allocating/leaf node, set p_state and return.
            p_trace_info.state = child_state;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            p_trace_info.node_index = node_index;
            p_trace_info.child_index = child_index;
            return;
        }
        node_index = entry & OctTreeNode::INDEX_MASK;
        // Remap from parent-local coordinates into child-local coordinates.
        p_trace_info.svo_position = frac(p_trace_info.svo_position * 2.0);
    }
    // We've traced a node.
    p_trace_info.state = OctTreeNode::ChildState::NODE;
    p_trace_info.child_index = 0;
    p_trace_info.node_index = node_index;
}

uint create_oct_tree_child_node(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeInfo> p_tree_info, uint p_parent_node_index) {
    uint new_node_index;
    InterlockedAdd(p_tree_info[0].node_count, 1, new_node_index);
    [unroll] for (uint i = 0; i < 8; ++i) {
        // Initialize all of the children to empty
        p_nodes[new_node_index].children[i] = uint(OctTreeNode::ChildState::EMPTY);
    }
    p_nodes[new_node_index].parent_index = p_parent_node_index;
    uint new_data_index;
    InterlockedAdd(p_tree_info[0].data_count, 1, new_data_index);
    p_nodes[new_node_index].data_index = new_data_index;
    return new_node_index | uint(OctTreeNode::ChildState::NODE);
}

uint create_oct_tree_child_leaf(RWStructuredBuffer<OctTreeLeaf> p_leaves, RWStructuredBuffer<OctTreeInfo> p_tree_info, uint p_parent_node_index) {
    uint new_leaf_index;
    InterlockedAdd(p_tree_info[0].leaves_count, 1, new_leaf_index);
    p_leaves[new_leaf_index].parent_index = p_parent_node_index;
    uint new_data_index;
    InterlockedAdd(p_tree_info[0].data_count, 1, new_data_index);
    p_leaves[new_leaf_index].data_index = new_data_index;
    return new_leaf_index | uint(OctTreeNode::ChildState::LEAF);
}

bool try_create_oct_tree_entry(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeLeaf> p_leaves,
    RWStructuredBuffer<OctTreeInfo> p_tree_info,
    uint p_node_index, uint p_child_index, bool p_is_leaf) {
    uint original;
    // Try swaping to ALLOCATE state
    InterlockedCompareExchange<uint>(
        p_nodes[p_node_index].children[p_child_index],
        uint(OctTreeNode::ChildState::EMPTY),
        uint(OctTreeNode::ChildState::ALLOCATING),
        original
    );
    if (original != OctTreeNode::ChildState::EMPTY) {
        // CAS Failed. Allocation failed.
        return false;
    }
    // CAS succeeded. Allocate the child node:
    if (p_is_leaf) {
        p_nodes[p_node_index].children[p_child_index] = create_oct_tree_child_leaf(p_leaves, p_tree_info, p_node_index);
    }
    else {
        p_nodes[p_node_index].children[p_child_index] = create_oct_tree_child_node(p_nodes, p_tree_info, p_node_index);
    }
    return true;
}

bool try_voxel_allocate(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeLeaf> p_leaves,
    RWStructuredBuffer<OctTreeInfo> p_tree_info, inout OctTreeNodeTraceInfo p_trace_info) {
    uint node_index = p_trace_info.node_index;
    uint curr_depth = p_trace_info.curr_depth;
    // TODO: is leaf
    for (int depth = 0; p_trace_info.remain_depth < 0 || depth < p_trace_info.remain_depth; ++depth) {
        OctTreeNode node = p_nodes[node_index];
        uint child_index = get_oct_tree_node_subregion(p_trace_info.svo_position);
        uint entry = node.children[child_index];
        OctTreeNode::ChildState child_state = (OctTreeNode::ChildState)(entry & OctTreeNode::STATE_MASK);
        switch (child_state) {
        case OctTreeNode::ChildState::NODE:
            // A node, keep on looking deeper.
            break;
        case OctTreeNode::ChildState::LEAF:
            // Found a leaf, allocation success.
            return true;
        case OctTreeNode::ChildState::ALLOCATING:
            // Other thread is occupying this node. Allocation failed. Set trace info and return.
            p_trace_info.state = child_state;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            p_trace_info.node_index = node_index;
            p_trace_info.child_index = child_index;
            p_trace_info.curr_depth = curr_depth;
            return false;
        case OctTreeNode::ChildState::EMPTY:
            // Try to allocate if found child empty.
            if (!try_create_oct_tree_entry(p_nodes, p_leaves, p_tree_info, node_index, child_index, curr_depth == p_tree_info[0].max_depth - 1)) {
                // If allocation failed, add this task to the deferred list.
                p_trace_info.state = child_state;
                p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
                p_trace_info.node_index = node_index;
                p_trace_info.child_index = child_index;
                p_trace_info.curr_depth = curr_depth;
                return false;
            }
            break;
        }
        node_index = entry & OctTreeNode::INDEX_MASK;
        ++curr_depth;
        // Remap from parent-local coordinates into child-local coordinates.
        p_trace_info.svo_position = frac(p_trace_info.svo_position * 2.0);
    }
    // We've traced a node.
    p_trace_info.state = OctTreeNode::ChildState::NODE;
    p_trace_info.child_index = 0;
    p_trace_info.node_index = node_index;
    p_trace_info.curr_depth = curr_depth;
    return true;
}

#endif
