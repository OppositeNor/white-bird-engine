/* Copyright 2026 OppositeNor

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

/**
 * @brief Reflected descriptor-heap bindings for this construction stage.
 */
struct OctreeResetDescriptorIndices {
    /**
     * @brief Descriptor-heap reference to octree_info.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_info")]
    uint2 octree_info;
    /**
     * @brief Descriptor-heap reference to octree_node_buffer.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_node_buffer")]
    uint2 tree_nodes;
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
ConstantBuffer<OctreeResetDescriptorIndices> descriptor_indices;

/**
 * @brief Resets mutable octree state before construction begins.
 */
[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    RWStructuredBuffer<OctreeInfo> octree_info = ResourceDescriptorHeap[descriptor_indices.octree_info.x];
    RWStructuredBuffer<OctreeNode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    // Depth and capacity are immutable configuration; only the counters below are reset by thread zero.
    uint index = p_dispatch_thread_id.x;
    if (index < 8) {
        tree_nodes[index].children_data = uint(OctreeNode::ChildrenState::EMPTY) << OCTREE_CHILDREN_STATE_SHIFT;
        tree_nodes[index].data_index = index;
        tree_nodes[index].parent_index = -1;
    }
    if (index == 0) {
        octree_info[0].node_count = 8;
        octree_info[0].leaves_count = 0;
        octree_info[0].data_count = 8;
        RWStructuredBuffer<uint> deferred_list_ping = ResourceDescriptorHeap[descriptor_indices.deferred_list_ping.x];
        RWStructuredBuffer<uint> deferred_list_pong = ResourceDescriptorHeap[descriptor_indices.deferred_list_pong.x];
        deferred_list_ping[OCTREE_DEFERRED_LIST_COUNT_INDEX] = 0;
        deferred_list_pong[OCTREE_DEFERRED_LIST_COUNT_INDEX] = 0;
    }
}
