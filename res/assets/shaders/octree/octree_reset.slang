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
#include "octree/octree_utils.slang"

struct OctreeResetDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "octree_info")]
    uint2 octree_info;
    [WBE_DESCRIPTOR("storage_buffer", "octree_node_buffer")]
    uint2 tree_nodes;
};

[[vk::push_constant]]
ConstantBuffer<OctreeResetDescriptorIndices> descriptor_indices;

/**
 * @brief Resets mutable octree state before construction begins.
 */
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
    RWStructuredBuffer<OctreeInfo> octree_info = ResourceDescriptorHeap[descriptor_indices.octree_info.x];
    RWStructuredBuffer<OctreeNode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];

    octree_info[0].node_count = 8;
    octree_info[0].leaves_count = 0;
    octree_info[0].data_count = 8;
    octree_info[0].deferred_task_count_ping = 0;
    octree_info[0].deferred_task_count_pong = 0;

    [unroll] for (uint i = 0; i < 8; ++i) {
        tree_nodes[i].children_data = uint(OctreeNode::ChildrenState::EMPTY) << 29;
        tree_nodes[i].data_index = i;
        tree_nodes[i].parent_index = -1;
    }
}
