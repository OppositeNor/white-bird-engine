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
#include "oct_tree/oct_tree_utils.slang"

struct OctTreeResetDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_info")]
    uint2 oct_tree_info;
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_node_buffer")]
    uint2 tree_nodes;
};

[[vk::push_constant]]
ConstantBuffer<OctTreeResetDescriptorIndices> descriptor_indices;

/**
 * @brief Resets mutable oct-tree state before construction begins.
 */
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
    RWStructuredBuffer<OctTreeInfo> oct_tree_info = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
    RWStructuredBuffer<OctTreeNode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];

    oct_tree_info[0].node_count = 8;
    oct_tree_info[0].leaves_count = 0;
    oct_tree_info[0].data_count = 8;
    oct_tree_info[0].deferred_task_count_ping = 0;
    oct_tree_info[0].deferred_task_count_pong = 0;

    [unroll] for (uint i = 0; i < 8; ++i) {
        tree_nodes[i].children_data = uint(OctTreeNode::ChildrenState::EMPTY) << 30;
        tree_nodes[i].data_index = i;
        tree_nodes[i].parent_index = -1;
    }
}
