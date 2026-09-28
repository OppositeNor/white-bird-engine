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

/**
 * @brief Four scalar fields with a 16-byte storage-buffer array stride.
 */
[WBE_DATA_TYPE]
struct OctreeNode {
    enum class ChildrenState {
        NODE = 0,
        EMPTY = 1,
        ALLOCATING = 2,
        LEAF = 3,
        ALLOCATING_DYNAMIC = 4,
        NODE_DYNAMIC = 5,
        LEAF_DYNAMIC = 6
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
     * @details Bits 31:29 encode NODE=0, EMPTY=1, ALLOCATING=2, LEAF=3,
     * ALLOCATING_DYNAMIC=4, NODE_DYNAMIC=5 and LEAF_DYNAMIC=6.
     * LEAF uses bits 28:21 for occupancy (bit 21 is sector zero) and bits 20:0 for an eight-leaf block index.
     * NODE uses bits 20:0 for an eight-node block index; bits 28:21 are unused.
     * NODE_DYNAMIC stores a virtual block index offset by max_node_count; LEAF_DYNAMIC indexes a dynamic leaf block.
     * EMPTY and ALLOCATING ignore the lower twenty-nine bits.
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
    /**
     * @brief Original child description saved by the dynamic allocation owner.
     */
    uint original_children_data;
};

[WBE_DATA_TYPE]
struct OctreeLeaf {
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
struct OctreeInfo {
    /**
     * @brief The origin of the octree in world space.
     */
    float3 origin;
    /**
     * @brief The width of the region this octree is covering.
     */
    float width;
    /**
     * @brief The maximum depth of the octree, at least two; stored nodes start at depth one.
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
    /**
     * @brief Capacity of the separate dynamic node array.
     */
    uint max_dynamic_node_count;
    /**
     * @brief Capacity in leaf slots; the dynamic array stores blocks of eight leaves plus a saved parent word.
     */
    uint max_dynamic_leaves_count;
    /**
     * @brief Allocated dynamic node slots, reset after restoring static parent links.
     */
    uint dynamic_node_count;
    /**
     * @brief Allocated dynamic leaf slots, always a multiple of eight.
     */
    uint dynamic_leaves_count;
};

[WBE_DATA_TYPE]
struct OctreeDeferredListEntry {
    OctreeNodeTraceInfo unfinished_trace_info;
};

uint get_octree_node_subregion(float3 p_position)
{
    return (uint(p_position.x >= 0.5) << 2) |
        (uint(p_position.y >= 0.5) << 1) |
        uint(p_position.z >= 0.5);
}

struct OctreeNodeTraceInfo {
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
OctreeNode::ChildrenState get_octree_children_state(OctreeNode p_node) {
    return OctreeNode::ChildrenState(p_node.children_data >> 29);
}

/**
 * @brief Decodes the first child index for a NODE or LEAF description.
 */
uint get_octree_children_index(OctreeNode p_node) {
    return (p_node.children_data & 0x001fffffu) * 8;
}

/**
 * @brief Tests an allocated leaf slot's occupancy for debug drawing after construction.
 * @details Invalid parent links report a shader validation error.
 */
bool is_octree_leaf_occupied(uint p_leaf_index,
    StructuredBuffer<OctreeNode> p_nodes,
    StructuredBuffer<OctreeLeaf> p_leaves,
    OctreeInfo p_tree_info,
    uint p_frame_slot) {
    int parent_index = p_leaves[p_leaf_index].parent_index;
    WBE_SHADER_DEBUG_ASSERT(parent_index >= 0 && uint(parent_index) < p_tree_info.node_count,
        ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
    return (p_nodes[parent_index].children_data & (1u << (21 + p_leaf_index % 8))) != 0;
}

/**
 * @brief Loads an octree node after acquiring its published child description.
 * @details The barrier makes initialization preceding NODE or LEAF publication visible to the reader.
 */
OctreeNode load_octree_node(RWStructuredBuffer<OctreeNode> p_nodes, uint p_node_index) {
    uint children_data;
    InterlockedAdd(p_nodes[p_node_index].children_data, 0, children_data);
    OctreeNode::ChildrenState state = OctreeNode::ChildrenState(children_data >> 29);
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
    InterlockedOr(p_nodes[p_node_index].children_data, 1u << (21 + get_octree_node_subregion(p_position)), original_data);
}

void traverse_octree(RWStructuredBuffer<OctreeNode> p_nodes, inout OctreeNodeTraceInfo p_trace_info) {
    uint node_index = p_trace_info.node_index;
    uint curr_depth = p_trace_info.curr_depth;
    for (int depth = 0; p_trace_info.remain_depth < 0 || depth < p_trace_info.remain_depth; ++depth) {
        OctreeNode node = load_octree_node(p_nodes, node_index);
        OctreeNode::ChildrenState children_state = get_octree_children_state(node);
        if (children_state != OctreeNode::ChildrenState::NODE) {
            // Traced to an empty/allocating/leaf node, set p_state and return.
            p_trace_info.node_index = node_index;
            p_trace_info.curr_depth = curr_depth;
            p_trace_info.remain_depth = p_trace_info.remain_depth < 0 ? p_trace_info.remain_depth : p_trace_info.remain_depth - depth;
            return;
        }
        node_index = get_octree_children_index(node) + get_octree_node_subregion(p_trace_info.svo_position);
        ++curr_depth;
        // Remap from parent-local coordinates into child-local coordinates.
        p_trace_info.svo_position = frac(p_trace_info.svo_position * 2.0);
    }
    // We've traced a node.
    p_trace_info.curr_depth = curr_depth;
    p_trace_info.node_index = node_index;
    p_trace_info.remain_depth = 0;
}

bool try_reserve_octree_nodes(RWStructuredBuffer<OctreeInfo> p_tree_info, uint p_frame_slot, out uint p_children_index) {
    uint current_count = p_tree_info[0].node_count;
    for (;;) {
        WBE_SHADER_DEBUG_ASSERT(current_count / 8 <= 0x001fffffu &&
            current_count <= p_tree_info[0].max_node_count &&
            p_tree_info[0].max_node_count - current_count >= 8,
            ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        uint original_count;
        InterlockedCompareExchange(p_tree_info[0].node_count, current_count, current_count + 8, original_count);
        if (original_count == current_count) {
            p_children_index = current_count;
            return true;
        }
        current_count = original_count;
    }
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
        p_nodes[child_index].children_data = uint(OctreeNode::ChildrenState::EMPTY) << 29;
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
    uint current_count = p_tree_info[0].leaves_count;
    for (;;) {
        WBE_SHADER_DEBUG_ASSERT(current_count / 8 <= 0x001fffffu &&
            current_count <= p_tree_info[0].max_leaves_count &&
            p_tree_info[0].max_leaves_count - current_count >= 8,
            ShaderDebugError::OCTREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
        uint original_count;
        InterlockedCompareExchange(p_tree_info[0].leaves_count, current_count, current_count + 8, original_count);
        if (original_count == current_count) {
            p_children_index = current_count;
            return true;
        }
        current_count = original_count;
    }
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
        uint(OctreeNode::ChildrenState::EMPTY) << 29,
        uint(OctreeNode::ChildrenState::ALLOCATING) << 29,
        original
    );
    if (original != (uint(OctreeNode::ChildrenState::EMPTY) << 29)) {
        // CAS Failed. Allocation failed.
        return false;
    }
    // CAS succeeded. Allocate the child node:
    uint children_index;
    if (p_is_leaf) {
        if (!create_octree_children_leaf(p_leaves, p_tree_info, p_node_index, p_frame_slot, children_index)) {
            publish_octree_children_data(p_nodes, p_node_index, uint(OctreeNode::ChildrenState::EMPTY) << 29);
            return false;
        }
        publish_octree_children_data(
            p_nodes, p_node_index, (uint(OctreeNode::ChildrenState::LEAF) << 29) | (children_index / 8));
    }
    else {
        if (!create_octree_children_node(p_nodes, p_tree_info, p_node_index, p_frame_slot, children_index)) {
            publish_octree_children_data(p_nodes, p_node_index, uint(OctreeNode::ChildrenState::EMPTY) << 29);
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
