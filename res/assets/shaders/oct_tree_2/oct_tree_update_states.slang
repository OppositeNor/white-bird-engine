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
#include "oct_tree_2/oct_tree_utils.slang"

/**
 * @brief Reflected descriptor-heap bindings for this construction stage.
 */
struct OctTreeUpdateStatesDescriptorIndices {
    /**
     * @brief Descriptor-heap reference to oct_tree_info.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_info")]
    uint2 oct_tree_info;
    /**
     * @brief Descriptor-heap reference to oct_tree_node_buffer.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_node_buffer")]
    uint2 tree_nodes;
    /**
     * @brief Descriptor-heap reference to oct_tree_pending_children.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_pending_children")]
    uint2 pending_children;
};

[[vk::push_constant]]
ConstantBuffer<OctTreeUpdateStatesDescriptorIndices> descriptor_indices;

/**
 * @brief Publishes completed allocations and accumulated leaf occupancy between construction passes.
 * @details The producer must finish and make all child initialization visible before this dispatch.
 * Each invocation exclusively updates one node. A memory dependency must precede the next consumer.
 */
[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    StructuredBuffer<OctTreeInfo> tree_info = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
    uint node_index = p_dispatch_thread_id.x;
    if (node_index >= tree_info[0].node_count) {
        return;
    }
    RWStructuredBuffer<OctTreeNode> nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    RWStructuredBuffer<uint> pending = ResourceDescriptorHeap[descriptor_indices.pending_children.x];
    uint children_data = pending[node_index];
    if ((children_data >> 30) == uint(OctTreeNode::ChildrenState::ALLOCATING)) {
        uint state = (children_data & 0x3fc00000u) != 0 ? uint(OctTreeNode::ChildrenState::LEAF) : uint(OctTreeNode::ChildrenState::NODE);
        children_data = (children_data & 0x3fffffffu) | (state << 30);
        pending[node_index] = children_data;
    }
    // Existing leaves also need their newly accumulated occupancy published.
    nodes[node_index].children_data = children_data;
}
