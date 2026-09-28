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
#ifndef WBE_FILE_OCT_TREE_DYNAMIC_READ_SLANG
#define WBE_FILE_OCT_TREE_DYNAMIC_READ_SLANG
#include "oct_tree/oct_tree_dynamic_types.slang"
/**
 * @brief Resolves a virtual node index in static or dynamic storage.
 */
OctTreeNode read_incremental_node(
    StructuredBuffer<OctTreeNode> p_nodes, StructuredBuffer<OctTreeNode> p_dynamic_nodes, OctTreeInfo p_info, uint p_index) {
    if (p_index < p_info.max_node_count) {
        return p_nodes[p_index];
    }
    return p_dynamic_nodes[p_index - p_info.max_node_count];
}
/**
 * @brief Resolves a debug instance into static leaves followed by dynamic leaves.
 */
OctTreeLeaf read_incremental_leaf(
    StructuredBuffer<OctTreeLeaf> p_leaves, StructuredBuffer<OctTreeDynamicLeafBlock> p_dynamic_leaves, OctTreeInfo p_info, uint p_index) {
    if (p_index < p_info.leaves_count) {
        return p_leaves[p_index];
    }
    uint local = p_index - p_info.leaves_count;
    return p_dynamic_leaves[local / 8].leaves[local % 8];
}
/**
 * @brief Hides superseded static leaf blocks and tests occupancy of the currently referenced block.
 */
bool is_incremental_leaf_occupied(uint p_index,
    StructuredBuffer<OctTreeNode> p_nodes,
    StructuredBuffer<OctTreeLeaf> p_leaves,
    StructuredBuffer<OctTreeNode> p_dynamic_nodes,
    StructuredBuffer<OctTreeDynamicLeafBlock> p_dynamic_leaves,
    OctTreeInfo p_info,
    uint p_frame_slot) {
    OctTreeLeaf leaf = read_incremental_leaf(p_leaves, p_dynamic_leaves, p_info, p_index);
    WBE_SHADER_DEBUG_ASSERT(leaf.parent_index >= 0 && uint(leaf.parent_index) < p_info.max_node_count + p_info.dynamic_node_count,
        ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW,
        p_frame_slot);
    OctTreeNode parent = read_incremental_node(p_nodes, p_dynamic_nodes, p_info, uint(leaf.parent_index));
    bool dynamic = p_index >= p_info.leaves_count;
    uint local = dynamic ? p_index - p_info.leaves_count : p_index;
    uint expected = uint(dynamic ? OctTreeNode::ChildrenState::LEAF_DYNAMIC : OctTreeNode::ChildrenState::LEAF);
    if ((parent.children_data >> 29) != expected || get_oct_tree_children_index(parent) / 8 != local / 8) {
        return false;
    }
    return (parent.children_data & (1u << (21 + local % 8))) != 0;
}
#endif
