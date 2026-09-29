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
#ifndef WBE_FILE_OCTREE_DYNAMIC_READ_SLANG
#define WBE_FILE_OCTREE_DYNAMIC_READ_SLANG
#include "octree/octree_dynamic_types.slang"
/**
 * @brief Resolves a virtual node index in static or dynamic storage.
 */
OctreeNode read_incremental_node(
    StructuredBuffer<OctreeNode> p_nodes, StructuredBuffer<OctreeNode> p_dynamic_nodes, OctreeInfo p_info, uint p_index) {
    if (p_index < p_info.max_node_count) {
        return p_nodes[p_index];
    }
    return p_dynamic_nodes[p_index - p_info.max_node_count];
}
/**
 * @brief Resolves a debug instance into static leaves followed by dynamic leaves.
 */
OctreeLeaf read_incremental_leaf(
    StructuredBuffer<OctreeLeaf> p_leaves, StructuredBuffer<OctreeDynamicLeafBlock> p_dynamic_leaves, OctreeInfo p_info, uint p_index) {
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
    StructuredBuffer<OctreeNode> p_nodes,
    StructuredBuffer<OctreeLeaf> p_leaves,
    StructuredBuffer<OctreeNode> p_dynamic_nodes,
    StructuredBuffer<OctreeDynamicLeafBlock> p_dynamic_leaves,
    OctreeInfo p_info,
    uint p_frame_slot) {
    OctreeLeaf leaf = read_incremental_leaf(p_leaves, p_dynamic_leaves, p_info, p_index);
    WBE_SHADER_DEBUG_ASSERT(leaf.parent_index >= 0 && uint(leaf.parent_index) < p_info.max_node_count + p_info.dynamic_node_count,
        ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW,
        p_frame_slot);
    OctreeNode parent = read_incremental_node(p_nodes, p_dynamic_nodes, p_info, uint(leaf.parent_index));
    bool dynamic = p_index >= p_info.leaves_count;
    uint local = dynamic ? p_index - p_info.leaves_count : p_index;
    uint expected = uint(dynamic ? OctreeNode::ChildrenState::LEAF_DYNAMIC : OctreeNode::ChildrenState::LEAF);
    if ((parent.children_data >> OCTREE_CHILDREN_STATE_SHIFT) != expected ||
        get_octree_children_index(parent) / 8 != local / 8) {
        return false;
    }
    return (parent.children_data & (1u << (OCTREE_CHILDREN_OCCUPANCY_SHIFT + local % 8))) != 0;
}
#endif
