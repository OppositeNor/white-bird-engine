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
#include "oct_tree/oct_tree_utils.slang"

struct OctTreeDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 oct_tree_info;
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 tree_leaves;
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 tree_node_data;
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 deferred_list_read;
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 deferred_list_write;
};

[shader("compute")]
[numthreads(64, 1, 1)]
void step_deferred_task(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    RWStructuredBuffer<OctTreeInfo> oct_tree_info = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
    if (p_dispatch_thread_id >= oct_tree_info[0].deferred_task_count) {
        // Extra thread in the group. No work to do.
        return;
    }
    RWStructuredBuffer<OctTreeNode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    RWStructuredBuffer<OctTreeLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    RWStructuredBuffer<uint> tree_node_data = ResourceDescriptorHeap[descriptor_indices.node_data.x];
    StructuredBuffer<OctTreeBuildDeferredTask> deferred_list_read = ResourceDescriptorHeap[descriptor_indices.deferred_list_read.x];
    RWStructuredBuffer<OctTreeBuildDeferredList> deferred_list_write = ResourceDescriptorHeap[descriptor_indices.deferred_list_write.x];
    OctTreeBuildDeferredTask task = deferred_list_read[p_dispatch_thread_id];

    OctTreeNodeTraceInfo trace_info = task.unfinished_trace_info;
    // We trace and expect it to hit a leaf.
    traverse_oct_tree(tree_nodes, trace_info);
    switch (trace_info.state) {
    case OctTreeNode::ChildState::NODE:
        // TODO: Currupted tree: There's a node deeper than the max depth.
        return;
    case OctTreeNode::ChildState::ALLOCATING:
        // TODO: Another thread is allocating the children node. Add this task to deferred.
        return;
    case OctTreeNode::ChildState::EMPTY:
        // TODO: Try allocate the node.
        break;
    case OctTreeNode::ChildState::LEAF:
        // Hits a leaf.
        if (trace_info.remain_depth != 1) {
            // TODO: Currupted tree: There's a leaf not at the bottom level.
        }
        return;
    }
}
