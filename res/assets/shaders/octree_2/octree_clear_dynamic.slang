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
#include "utils/utils.slang"
#include "octree_2/octree_utils.slang"
#include "octree/octree_dynamic_types.slang"
/**
 * @brief Descriptor-heap resources used by incremental maintenance.
 */
struct DynamicDescriptorIndices {
    /**
     * @brief Descriptor-heap reference to octree_info.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_info")]
    uint2 info;
    /**
     * @brief Descriptor-heap reference to octree_node_buffer.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_node_buffer")]
    uint2 nodes;
    /**
     * @brief Descriptor-heap reference to octree_dynamic_nodes.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_dynamic_nodes", "read")]
    uint2 dynamic_nodes;
    /**
     * @brief Descriptor-heap reference to octree_dynamic_leaves.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_dynamic_leaves", "read")]
    uint2 dynamic_leaves;
    /**
     * @brief Descriptor-heap reference to octree_deferred_list_ping.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_deferred_list_ping", "write")]
    uint2 deferred_list_ping;
    /**
     * @brief Descriptor-heap reference to octree_deferred_list_pong.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_deferred_list_pong", "write")]
    uint2 deferred_list_pong;
};
[[vk::push_constant]]
ConstantBuffer<DynamicDescriptorIndices> descriptor_indices;

/**
 * @brief Restores static parents and discards the previous frame's dynamic allocation ranges.
 * @details Leaf restoration uses its saved block directly, never the concurrently reset dynamic count.
 * A graph dependency must complete this entire dispatch before any dynamic allocation begins.
 */
[shader("compute")]
[numthreads(128, 1, 1)]
void main(uint3 p_id : SV_DispatchThreadID) {
    RWStructuredBuffer<OctreeInfo> info = ResourceDescriptorHeap[descriptor_indices.info.x];
    RWStructuredBuffer<OctreeNode> nodes = ResourceDescriptorHeap[descriptor_indices.nodes.x];
    StructuredBuffer<OctreeDynamicLeafBlock> leaves = ResourceDescriptorHeap[descriptor_indices.dynamic_leaves.x];
    uint index = p_id.x;
    if (index < info[0].node_count) {
        uint description = nodes[index].children_data;
        uint state = description >> OCTREE_CHILDREN_STATE_SHIFT;
        if (state == uint(OctreeNode::ChildrenState::NODE_DYNAMIC)) {
            nodes[index].children_data = uint(OctreeNode::ChildrenState::EMPTY) << OCTREE_CHILDREN_STATE_SHIFT;
        } else if (state == uint(OctreeNode::ChildrenState::LEAF_DYNAMIC)) {
            nodes[index].children_data = leaves[description & OCTREE_CHILDREN_BLOCK_INDEX_MASK].original_children_data;
        }
    }
    if (index == 0) {
        info[0].dynamic_node_count = 0;
        info[0].dynamic_leaves_count = 0;
        RWStructuredBuffer<uint> deferred_list_ping = ResourceDescriptorHeap[descriptor_indices.deferred_list_ping.x];
        RWStructuredBuffer<uint> deferred_list_pong = ResourceDescriptorHeap[descriptor_indices.deferred_list_pong.x];
        deferred_list_ping[OCTREE_DEFERRED_LIST_COUNT_INDEX] = 0;
        deferred_list_pong[OCTREE_DEFERRED_LIST_COUNT_INDEX] = 0;
    }
}
