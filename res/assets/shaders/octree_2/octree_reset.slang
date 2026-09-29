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
     * @brief Descriptor-heap reference to octree_pending_children.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_pending_children")]
    uint2 pending_children;
    /**
     * @brief Descriptor-heap reference to octree_voxel_claims.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_voxel_claims")]
    uint2 voxel_claims;
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
    RWStructuredBuffer<uint> pending_children = ResourceDescriptorHeap[descriptor_indices.pending_children.x];

    RWStructuredBuffer<uint> voxel_claims = ResourceDescriptorHeap[descriptor_indices.voxel_claims.x];
    // Depth and capacity are immutable configuration; only the counters below are reset by thread zero.
    uint extent = 1u << octree_info[0].max_depth;
    uint claim_word_count = (extent * extent * extent + 31u) / 32u;
    uint index = p_dispatch_thread_id.x;
    if (index < claim_word_count) {
        voxel_claims[index] = 0;
    }
    if (index < 8) {
        tree_nodes[index].children_data = uint(OctreeNode::ChildrenState::EMPTY) << OCTREE_CHILDREN_STATE_SHIFT;
        pending_children[index] = uint(OctreeNode::ChildrenState::EMPTY) << OCTREE_CHILDREN_STATE_SHIFT;
        tree_nodes[index].data_index = index;
        tree_nodes[index].parent_index = -1;
    }
    if (index == 0) {
        octree_info[0].node_count = 8;
        octree_info[0].leaves_count = 0;
        octree_info[0].data_count = 8;
        octree_info[0].deferred_task_count_ping = 0;
        octree_info[0].deferred_task_count_pong = 0;
    }
}
