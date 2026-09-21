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
    Atomic<uint> node_count;
    /**
     * @brief The number of leaves.
     */
    Atomic<uint> leaves_count;
    /**
     * @brief The number of data blocks.
     */
    Atomic<uint> data_count;
    /**
     * @brief The number of deferred tasks.
     */
    Atomic<uint> deferred_task_count;
};

[WBE_DATA_TYPE]
struct OctTreeBuildDeferredTask {
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

#endif
