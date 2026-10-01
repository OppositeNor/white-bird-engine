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
#ifndef WBE_FILE_OCTREE_UTILS_SLANG
#define WBE_FILE_OCTREE_UTILS_SLANG

#include "octree/octree_common.slang"

/**
 * @brief Loads an octree node after acquiring its published child description.
 * @details The barrier makes initialization preceding NODE or LEAF publication visible to the reader.
 */
OctreeNode load_octree_node(RWStructuredBuffer<OctreeNode> p_nodes, uint p_node_index) {
    uint children_data;
    InterlockedAdd(p_nodes[p_node_index].children_data, 0, children_data);
    OctreeNode::ChildrenState state = OctreeNode::ChildrenState(children_data >> OCTREE_CHILDREN_STATE_SHIFT);
    if (state == OctreeNode::ChildrenState::NODE || state == OctreeNode::ChildrenState::LEAF) {
        DeviceMemoryBarrier();
    }
    OctreeNode result;
    result.children_data = children_data;
    result.data_index = p_nodes[p_node_index].data_index;
    result.parent_index = p_nodes[p_node_index].parent_index;
    return result;
}

/**
 * @brief Publishes initialized children and their block index as one atomic description.
 * @details The barrier makes initialization of all eight children visible before NODE or LEAF publication.
 */
void publish_octree_children_data(RWStructuredBuffer<OctreeNode> p_nodes, uint p_node_index, uint p_children_data) {
    DeviceMemoryBarrier();
    uint original_data;
    InterlockedExchange(p_nodes[p_node_index].children_data, p_children_data, original_data);
}

/**
 * @brief Marks just the intersected leaf occupied, preserving concurrent insertions into the same block.
 */
void occupy_octree_leaf(RWStructuredBuffer<OctreeNode> p_nodes, uint p_node_index, float3 p_position) {
    uint original_data;
    InterlockedOr(p_nodes[p_node_index].children_data,
        1u << (OCTREE_CHILDREN_OCCUPANCY_SHIFT + get_octree_node_subregion(p_position)),
        original_data);
}

bool try_reserve_octree_nodes(RWStructuredBuffer<OctreeInfo> p_tree_info, uint p_frame_slot, out uint p_children_index) {
    InterlockedAdd(p_tree_info[0].node_count, 8, p_children_index);
    WBE_SHADER_DEBUG_ASSERT(p_children_index / 8 <= OCTREE_CHILDREN_BLOCK_INDEX_MASK &&
        p_children_index <= p_tree_info[0].max_node_count &&
        p_tree_info[0].max_node_count - p_children_index >= 8,
        ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
    return true;
}

bool create_octree_children_node(RWStructuredBuffer<OctreeNode> p_nodes,
    RWStructuredBuffer<OctreeInfo> p_tree_info,
    uint p_parent_node_index,
    uint p_frame_slot,
    out uint p_children_index) {
    if (!try_reserve_octree_nodes(p_tree_info, p_frame_slot, p_children_index)) {
        return false;
    }
    [unroll] for (uint i = 0; i < 8; ++i) {
        // Initialize all of the children to empty
        uint child_index = p_children_index + i;
        p_nodes[child_index].children_data = uint(OctreeNode::ChildrenState::EMPTY) << OCTREE_CHILDREN_STATE_SHIFT;
        p_nodes[child_index].parent_index = p_parent_node_index;
    }
    uint new_data_index;
    InterlockedAdd(p_tree_info[0].data_count, 8, new_data_index);
    [unroll] for (uint i = 0; i < 8; ++i) {
        p_nodes[p_children_index + i].data_index = new_data_index + i;
    }
    return true;
}

bool try_reserve_octree_leaves(RWStructuredBuffer<OctreeInfo> p_tree_info, uint p_frame_slot, out uint p_children_index) {
    InterlockedAdd(p_tree_info[0].leaves_count, 8, p_children_index);
    WBE_SHADER_DEBUG_ASSERT(p_children_index / 8 <= OCTREE_CHILDREN_BLOCK_INDEX_MASK &&
        p_children_index <= p_tree_info[0].max_leaves_count &&
        p_tree_info[0].max_leaves_count - p_children_index >= 8,
        ShaderDebugError::OCTREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
    return true;
}

bool create_octree_children_leaf(RWStructuredBuffer<OctreeLeaf> p_leaves,
    RWStructuredBuffer<OctreeInfo> p_tree_info,
    uint p_parent_node_index,
    uint p_frame_slot,
    out uint p_children_index) {
    if (!try_reserve_octree_leaves(p_tree_info, p_frame_slot, p_children_index)) {
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

bool try_create_octree_entry(RWStructuredBuffer<OctreeNode> p_nodes, RWStructuredBuffer<OctreeLeaf> p_leaves,
    RWStructuredBuffer<OctreeInfo> p_tree_info,
    uint p_node_index, bool p_is_leaf, uint p_frame_slot) {
    WBE_SHADER_DEBUG_ASSERT(p_node_index < p_tree_info[0].max_node_count,
        ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
    uint original;
    // Try swaping to ALLOCATE state
    InterlockedCompareExchange(
        p_nodes[p_node_index].children_data,
        uint(OctreeNode::ChildrenState::EMPTY) << OCTREE_CHILDREN_STATE_SHIFT,
        uint(OctreeNode::ChildrenState::ALLOCATING) << OCTREE_CHILDREN_STATE_SHIFT,
        original
    );
    if (original != (uint(OctreeNode::ChildrenState::EMPTY) << OCTREE_CHILDREN_STATE_SHIFT)) {
        // CAS Failed. Allocation failed.
        return false;
    }
    // CAS succeeded. Allocate the child node:
    uint children_index;
    if (p_is_leaf) {
        if (!create_octree_children_leaf(p_leaves, p_tree_info, p_node_index, p_frame_slot, children_index)) {
            publish_octree_children_data(
                p_nodes, p_node_index, uint(OctreeNode::ChildrenState::EMPTY) << OCTREE_CHILDREN_STATE_SHIFT);
            return false;
        }
        publish_octree_children_data(
            p_nodes,
            p_node_index,
            (uint(OctreeNode::ChildrenState::LEAF) << OCTREE_CHILDREN_STATE_SHIFT) | (children_index / 8));
    }
    else {
        if (!create_octree_children_node(p_nodes, p_tree_info, p_node_index, p_frame_slot, children_index)) {
            publish_octree_children_data(
                p_nodes, p_node_index, uint(OctreeNode::ChildrenState::EMPTY) << OCTREE_CHILDREN_STATE_SHIFT);
            return false;
        }
        publish_octree_children_data(p_nodes, p_node_index, children_index / 8);
    }
    return true;
}

bool try_voxel_allocate(RWStructuredBuffer<OctreeNode> p_nodes, RWStructuredBuffer<OctreeLeaf> p_leaves,
    RWStructuredBuffer<OctreeInfo> p_tree_info, inout OctreeNodeTraceInfo p_trace_info, uint p_frame_slot) {
    uint node_index = p_trace_info.node_index;
    uint curr_depth = p_trace_info.curr_depth;
    WBE_SHADER_DEBUG_ASSERT(p_tree_info[0].max_depth >= 2 &&
        p_tree_info[0].max_node_count >= 8 &&
        p_tree_info[0].max_leaves_count != 0,
        ShaderDebugError::OCTREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
    for (int depth = 0; p_trace_info.remain_depth < 0 || depth < p_trace_info.remain_depth; ++depth) {
        WBE_SHADER_DEBUG_ASSERT(curr_depth < p_tree_info[0].max_depth,
            ShaderDebugError::OCTREE_DEEPER_THAN_EXPECTED, p_frame_slot);
        WBE_SHADER_DEBUG_ASSERT(node_index < p_tree_info[0].max_node_count,
            ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        OctreeNode node = load_octree_node(p_nodes, node_index);
        if (get_octree_children_state(node) == OctreeNode::ChildrenState::EMPTY) {
            if (!try_create_octree_entry(
                    p_nodes, p_leaves, p_tree_info, node_index, curr_depth == p_tree_info[0].max_depth - 1, p_frame_slot)) {
                p_trace_info.node_index = node_index;
                p_trace_info.curr_depth = curr_depth;
                p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
                return false;
            }
            node = load_octree_node(p_nodes, node_index);
        }
        OctreeNode::ChildrenState child_state = get_octree_children_state(node);
        switch (child_state) {
        case OctreeNode::ChildrenState::NODE:
            // A node, keep on looking deeper.
            WBE_SHADER_DEBUG_ASSERT(curr_depth < p_tree_info[0].max_depth - 1,
                ShaderDebugError::OCTREE_DEEPER_THAN_EXPECTED, p_frame_slot);
            WBE_SHADER_DEBUG_ASSERT(get_octree_children_index(node) <= p_tree_info[0].max_node_count &&
                p_tree_info[0].max_node_count - get_octree_children_index(node) >= 8,
                ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
            break;
        case OctreeNode::ChildrenState::LEAF:
            // Found a leaf, allocation success.
            WBE_SHADER_DEBUG_ASSERT(curr_depth >= p_tree_info[0].max_depth - 1,
                ShaderDebugError::OCTREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
            WBE_SHADER_DEBUG_ASSERT(curr_depth <= p_tree_info[0].max_depth - 1,
                ShaderDebugError::OCTREE_DEEPER_THAN_EXPECTED, p_frame_slot);
            WBE_SHADER_DEBUG_ASSERT(get_octree_children_index(node) <= p_tree_info[0].max_leaves_count &&
                p_tree_info[0].max_leaves_count - get_octree_children_index(node) >= 8,
                ShaderDebugError::OCTREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
            occupy_octree_leaf(p_nodes, node_index, p_trace_info.svo_position);
            return true;
        case OctreeNode::ChildrenState::ALLOCATING:
            // Traced to an empty/allocating/leaf node, set p_state and return.
            p_trace_info.node_index = node_index;
            p_trace_info.curr_depth = curr_depth;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            return false;
        default:
            WBE_SHADER_DEBUG_ASSERT(false, curr_depth < p_tree_info[0].max_depth - 1
                    ? ShaderDebugError::OCTREE_SHALLOWER_THAN_EXPECTED
                    : ShaderDebugError::OCTREE_DEEPER_THAN_EXPECTED,
                p_frame_slot);
            break;
        }
        uint child_index = get_octree_children_index(node) + get_octree_node_subregion(p_trace_info.svo_position);
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
