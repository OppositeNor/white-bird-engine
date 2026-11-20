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
#include "svo/svo_utils.slang"

struct SVOResetDescIndices {
    [wbe::desc("storage_buffer", "svo_info")]
    uint2 svo_info;
    [wbe::desc("storage_buffer", "svo_node_buffer")]
    uint2 tree_nodes;
    [wbe::desc("storage_buffer", "svo_deferred_list_ping", "write")]
    uint2 deferred_list_ping;
    [wbe::desc("storage_buffer", "svo_deferred_list_pong", "write")]
    uint2 deferred_list_pong;
};

[vk::push_constant]
ConstantBuffer<SVOResetDescIndices> di;

/**
 * @brief Resets mutable SVO state before construction begins.
 */
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
    RWStructuredBuffer<SVOInfo> svo_info = ResourceDescriptorHeap[di.svo_info.x];
    RWStructuredBuffer<SVONode> tree_nodes = ResourceDescriptorHeap[di.tree_nodes.x];

    svo_info[0].node_count = 8;
    svo_info[0].leaves_count = 0;
    svo_info[0].data_count = 8;
    RWStructuredBuffer<uint> deferred_list_ping = ResourceDescriptorHeap[di.deferred_list_ping.x];
    RWStructuredBuffer<uint> deferred_list_pong = ResourceDescriptorHeap[di.deferred_list_pong.x];
    deferred_list_ping[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;
    deferred_list_pong[SVO_DEFERRED_LIST_COUNT_INDEX] = 0;

    [unroll] for (uint i = 0; i < 8; ++i) {
        // Reset the top most portion of the tree.
        tree_nodes[i].children_data = uint(SVONode::ChildrenState::EMPTY) << SVO_CHILDREN_STATE_SHIFT;
        tree_nodes[i].data_index = i;
        tree_nodes[i].parent_data = make_svo_root_parent_data();
    }
}
