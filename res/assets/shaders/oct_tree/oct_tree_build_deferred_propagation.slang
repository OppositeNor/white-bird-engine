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
    StructuredBuffer<OctTreeDeferredListEntry> deferred_list_read = ResourceDescriptorHeap[descriptor_indices.deferred_list_read.x];
    RWStructuredBuffer<OctTreeDeferredListEntry> deferred_list_write = ResourceDescriptorHeap[descriptor_indices.deferred_list_write.x];
    OctTreeBuildDeferredTask task = deferred_list_read[p_dispatch_thread_id];

    OctTreeNodeTraceInfo trace_info = task.unfinished_trace_info;
    // Try to allocate the node.
    if (!try_voxel_allocate(tree_nodes, tree_leaves, oct_tree_info, trace_info)) {
        // If allocation failed, push this to the deferred list.
        uint new_task_index;
        InterlockedAdd(oct_tree_info[0].deferred_task_count, 1, new_task_index);
        deferred_list_write[new_task_index].unfinished_trace_info = trace_info;
    }
}
