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
struct OctreeUpdateStatesDescriptorIndices {
    /**
     * @brief Descriptor-heap reference to octree_info.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_info", "read")]
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
};

[[vk::push_constant]]
ConstantBuffer<OctreeUpdateStatesDescriptorIndices> descriptor_indices;

/**
 * @brief Publishes completed allocations and accumulated leaf occupancy between construction passes.
 * @details The producer must finish and make all child initialization visible before this dispatch.
 * Each invocation exclusively updates one node. A memory dependency must precede the next consumer.
 */
[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    StructuredBuffer<OctreeInfo> tree_info = ResourceDescriptorHeap[descriptor_indices.octree_info.x];
    uint node_index = p_dispatch_thread_id.x;
    if (node_index >= tree_info[0].node_count) {
        return;
    }
    RWStructuredBuffer<OctreeNode> nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    RWStructuredBuffer<uint> pending = ResourceDescriptorHeap[descriptor_indices.pending_children.x];
    uint children_data = pending[node_index];
    if ((children_data >> OCTREE_CHILDREN_STATE_SHIFT) == uint(OctreeNode::ChildrenState::ALLOCATING)) {
        uint state = (children_data & OCTREE_CHILDREN_OCCUPANCY_MASK) != 0 ? uint(OctreeNode::ChildrenState::LEAF)
                                                                   : uint(OctreeNode::ChildrenState::NODE);
        children_data = (children_data & OCTREE_CHILDREN_PAYLOAD_MASK) | (state << OCTREE_CHILDREN_STATE_SHIFT);
        pending[node_index] = children_data;
    }
    // Existing leaves also need their newly accumulated occupancy published.
    nodes[node_index].children_data = children_data;
}
