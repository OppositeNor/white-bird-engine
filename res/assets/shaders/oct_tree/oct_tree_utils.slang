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
    // The index of the node.
    uint node_index;
    // The current depth.
    uint curr_depth;
    // The remain depth.
    int remain_depth;
    // The position in the node's svo space.
    float3 svo_position;
};

void traverse_oct_tree(RWStructuredBuffer<OctTreeNode> p_nodes, inout OctTreeNodeTraceInfo p_trace_info) {
    uint node_index = p_trace_info.node_index;
    uint curr_depth = p_trace_info.curr_depth;
    for (int depth = 0; p_trace_info.remain_depth < 0 || depth < p_trace_info.remain_depth; ++depth) {
        OctTreeNode node = p_nodes[node_index];
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

uint create_oct_tree_children_node(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeInfo> p_tree_info, uint p_parent_node_index) {
    uint children_index;
    InterlockedAdd(p_tree_info[0].node_count, 8, children_index);
    [unroll] for (uint i = 0; i < 8; ++i) {
        // Initialize all of the children to empty
        uint child_index = children_index + i;
        p_nodes[child_index].children_state = uint(OctTreeNode::ChildrenState::EMPTY);
        p_nodes[child_index].parent_index = p_parent_node_index;
    }
    uint new_data_index;
    InterlockedAdd(p_tree_info[0].data_count, 8, new_data_index);
    [unroll] for (uint i = 0; i < 8; ++i) {
        p_nodes[children_index + i].data_index = new_data_index + i;
    }
    return children_index;
}

uint create_oct_tree_children_leaf(RWStructuredBuffer<OctTreeLeaf> p_leaves, RWStructuredBuffer<OctTreeInfo> p_tree_info, uint p_parent_node_index) {
    uint children_index;
    InterlockedAdd(p_tree_info[0].leaves_count, 8, children_index);
    [unroll] for (uint i = 0; i < 8; ++i) {
        uint child_index = children_index + i;
        p_leaves[child_index].parent_index = p_parent_node_index;
    }
    uint new_data_index;
    InterlockedAdd(p_tree_info[0].data_count, 8, new_data_index);
    [unroll] for (uint i = 0; i < 8; ++i) {
        p_leaves[children_index + i].data_index = new_data_index + i;
    }
    return children_index;
}

bool try_create_oct_tree_entry(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeLeaf> p_leaves,
    RWStructuredBuffer<OctTreeInfo> p_tree_info,
    uint p_node_index, bool p_is_leaf) {
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
    if (p_is_leaf) {
        p_nodes[p_node_index].children = create_oct_tree_children_leaf(p_leaves, p_tree_info, p_node_index);
        p_nodes[p_node_index].children_state = uint(OctTreeNode::ChildrenState::NODE);
    }
    else {
        p_nodes[p_node_index].children = create_oct_tree_children_node(p_nodes, p_tree_info, p_node_index);
        p_nodes[p_node_index].children_state = uint(OctTreeNode::ChildrenState::LEAF);
    }
    return true;
}

bool try_voxel_allocate(RWStructuredBuffer<OctTreeNode> p_nodes, RWStructuredBuffer<OctTreeLeaf> p_leaves,
    RWStructuredBuffer<OctTreeInfo> p_tree_info, inout OctTreeNodeTraceInfo p_trace_info) {
    uint node_index = p_trace_info.node_index;
    uint curr_depth = p_trace_info.curr_depth;
    for (int depth = 0; p_trace_info.remain_depth < 0 || depth < p_trace_info.remain_depth; ++depth) {
        OctTreeNode node = p_nodes[node_index];
        OctTreeNode::ChildrenState child_state = OctTreeNode::ChildrenState(node.children_state);
        switch (child_state) {
        case OctTreeNode::ChildrenState::NODE:
            // A node, keep on looking deeper.
            break;
        case OctTreeNode::ChildrenState::LEAF:
            // Found a leaf, allocation success.
            return true;
        case OctTreeNode::ChildrenState::ALLOCATING:
            // Traced to an empty/allocating/leaf node, set p_state and return.
            p_trace_info.node_index = node_index;
            p_trace_info.curr_depth = curr_depth;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            return false;
        case OctTreeNode::ChildrenState::EMPTY:
            // Try to allocate if found child empty.
            if (!try_create_oct_tree_entry(p_nodes, p_leaves, p_tree_info, node_index, curr_depth == p_tree_info[0].max_depth - 1)) {
                // If allocation failed, add this task to the deferred list.
                p_trace_info.node_index = node_index;
                p_trace_info.curr_depth = curr_depth;
                p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
                return false;
            }
            break;
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
