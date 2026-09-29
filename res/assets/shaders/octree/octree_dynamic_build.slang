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
#ifndef WBE_FILE_OCTREE_DYNAMIC_BUILD_SLANG
#define WBE_FILE_OCTREE_DYNAMIC_BUILD_SLANG
#include "octree/octree_dynamic_types.slang"

/**
 * @brief Extends static topology using pass-published dynamic children and copy-on-write leaves.
 * @details Dynamic node links use virtual indices starting at max_node_count. Only the CAS owner
 * initializes a block. All contenders retry after the publication dispatch; none read unfinished payloads.
 */
bool try_dynamic_voxel_allocate(RWStructuredBuffer<OctreeNode> p_static_nodes,
    RWStructuredBuffer<OctreeLeaf> p_static_leaves,
    RWStructuredBuffer<OctreeNode> p_dynamic_nodes,
    RWStructuredBuffer<OctreeDynamicLeafBlock> p_dynamic_leaves,
    RWStructuredBuffer<OctreeInfo> p_info,
    inout OctreeNodeTraceInfo p_trace,
    uint p_frame_slot) {
    uint static_capacity = p_info[0].max_node_count;
    for (int step = 0; p_trace.remain_depth < 0 || p_trace.remain_depth > 0; ++step) {
        uint node_index = p_trace.node_index;
        bool is_static = node_index < static_capacity;
        uint local_index = is_static ? node_index : node_index - static_capacity;
        WBE_SHADER_DEBUG_ASSERT(is_static ? local_index < p_info[0].node_count : local_index < p_info[0].dynamic_node_count,
            ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW,
            p_frame_slot);
        uint description;
        if (is_static) {
            InterlockedAdd(p_static_nodes[local_index].children_data, 0, description);
        } else {
            InterlockedAdd(p_dynamic_nodes[local_index].children_data, 0, description);
        }
        uint state = description >> OCTREE_CHILDREN_STATE_SHIFT;
        uint sector = get_octree_node_subregion(p_trace.svo_position);
        if (state == uint(OctreeNode::ChildrenState::NODE) || state == uint(OctreeNode::ChildrenState::NODE_DYNAMIC)) {
            p_trace.node_index = (description & OCTREE_CHILDREN_BLOCK_INDEX_MASK) * 8 + sector;
            ++p_trace.curr_depth;
            if (p_trace.remain_depth > 0) {
                --p_trace.remain_depth;
            }
            p_trace.svo_position = frac(p_trace.svo_position * 2.0);
            continue;
        }
        if (state == uint(OctreeNode::ChildrenState::LEAF_DYNAMIC)) {
            uint ignored;
            if (is_static) {
                InterlockedOr(
                    p_static_nodes[local_index].children_data, 1u << (OCTREE_CHILDREN_OCCUPANCY_SHIFT + sector), ignored);
            } else {
                InterlockedOr(
                    p_dynamic_nodes[local_index].children_data, 1u << (OCTREE_CHILDREN_OCCUPANCY_SHIFT + sector), ignored);
            }
            return true;
        }
        if (state == uint(OctreeNode::ChildrenState::ALLOCATING_DYNAMIC)) {
            return false;
        }
        WBE_SHADER_DEBUG_ASSERT(state == uint(OctreeNode::ChildrenState::EMPTY) || state == uint(OctreeNode::ChildrenState::LEAF),
            ShaderDebugError::OCTREE_SHALLOWER_THAN_EXPECTED,
            p_frame_slot);
        uint original;
        uint allocating = uint(OctreeNode::ChildrenState::ALLOCATING_DYNAMIC) << OCTREE_CHILDREN_STATE_SHIFT;
        if (is_static) {
            InterlockedCompareExchange(p_static_nodes[local_index].children_data, description, allocating, original);
        } else {
            InterlockedCompareExchange(p_dynamic_nodes[local_index].children_data, description, allocating, original);
        }
        if (original != description) {
            return false;
        }
        if (is_static) {
            p_static_nodes[local_index].original_children_data = original;
        } else {
            p_dynamic_nodes[local_index].original_children_data = original;
        }
        bool is_leaf = p_trace.curr_depth == p_info[0].max_depth - 1;
        uint children;
        uint occupancy = 0;
        if (is_leaf) {
            InterlockedAdd(p_info[0].dynamic_leaves_count, 8, children);
            WBE_SHADER_DEBUG_ASSERT(
                children < p_info[0].max_dynamic_leaves_count, ShaderDebugError::OCTREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
            uint block = children / 8;
            p_dynamic_leaves[block].original_children_data = original;
            [unroll] for (uint i = 0; i < 8; ++i) {
                OctreeLeaf leaf;
                if (state == uint(OctreeNode::ChildrenState::LEAF)) {
                    leaf = p_static_leaves[(original & OCTREE_CHILDREN_BLOCK_INDEX_MASK) * 8 + i];
                } else {
                    leaf.data_index = p_info[0].data_count + p_info[0].max_dynamic_node_count + children + i;
                    leaf.parent_index = int(node_index);
                }
                p_dynamic_leaves[block].leaves[i] = leaf;
            }
            occupancy = (original & OCTREE_CHILDREN_OCCUPANCY_MASK) | (1u << (OCTREE_CHILDREN_OCCUPANCY_SHIFT + sector));
        } else {
            InterlockedAdd(p_info[0].dynamic_node_count, 8, children);
            WBE_SHADER_DEBUG_ASSERT(
                children < p_info[0].max_dynamic_node_count, ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
            [unroll] for (uint i = 0; i < 8; ++i) {
                OctreeNode child;
                child.children_data = uint(OctreeNode::ChildrenState::EMPTY) << OCTREE_CHILDREN_STATE_SHIFT;
                child.data_index = p_info[0].data_count + children + i;
                child.parent_index = int(node_index);
                child.original_children_data = child.children_data;
                p_dynamic_nodes[children + i] = child;
            }
            children += static_capacity;
        }
        uint ignored;
        uint pending = allocating | occupancy | (children / 8);
        if (is_static) {
            InterlockedExchange(p_static_nodes[local_index].children_data, pending, ignored);
        } else {
            InterlockedExchange(p_dynamic_nodes[local_index].children_data, pending, ignored);
        }
        return false;
    }
    return true;
}
#endif
