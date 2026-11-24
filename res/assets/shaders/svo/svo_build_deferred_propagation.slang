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
#include "svo/svo_utils.slang"
#ifdef WBE_DYNAMIC_BUILD
#include "svo/svo_dynamic_build.slang"
#endif

struct SVODescIndices {
    [wbe::desc("storage_buffer", "svo_info")]
    DescriptorHandle<RWStructuredBuffer<SVOInfo>> svo_info;
    [wbe::desc("storage_buffer", "svo_node_buffer")]
    DescriptorHandle<RWStructuredBuffer<SVONode>> tree_nodes;
    [wbe::desc("storage_buffer", "svo_leaf_buffer")]
    DescriptorHandle<RWStructuredBuffer<SVOLeaf>> tree_leaves;
#ifdef WBE_USE_PING_WRITE
    [wbe::desc("storage_buffer", "svo_deferred_list_pong", "read")]
    DescriptorHandle<StructuredBuffer<uint>> deferred_list_read;
    [wbe::desc("storage_buffer", "svo_deferred_list_ping")]
    DescriptorHandle<RWStructuredBuffer<uint>> deferred_list_write;
#else
    [wbe::desc("storage_buffer", "svo_deferred_list_ping", "read")]
    DescriptorHandle<StructuredBuffer<uint>> deferred_list_read;
    [wbe::desc("storage_buffer", "svo_deferred_list_pong")]
    DescriptorHandle<RWStructuredBuffer<uint>> deferred_list_write;
#endif
    [wbe::push_constant("frame_slot")]
    uint shader_debug_frame_slot;
};

[vk::push_constant]
ConstantBuffer<SVODescIndices> di;

[shader("compute")]
[numthreads(128, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    RWStructuredBuffer<SVOInfo> svo_info = di.svo_info;
    // The list may exceed one dispatch's thread count, so each invocation strides over the whole dispatch.
    uint task_stride = WorkgroupCount().x * 128;
    RWStructuredBuffer<SVONode> tree_nodes = di.tree_nodes;
    RWStructuredBuffer<SVOLeaf> tree_leaves = di.tree_leaves;
    StructuredBuffer<uint> deferred_list_read = di.deferred_list_read;
    RWStructuredBuffer<uint> deferred_list_write = di.deferred_list_write;
    uint deferred_task_count = deferred_list_read[SVO_DEFERRED_LIST_COUNT_INDEX];
    for (uint task_index = p_dispatch_thread_id.x; task_index < deferred_task_count; task_index += task_stride) {
        uint task = deferred_list_read[SVO_DEFERRED_LIST_ENTRY_OFFSET + task_index];

        SVONodeTraceInfo trace_info = to_trace_info(task, svo_info[0].max_depth);
        // Try to allocate the node.
#ifdef WBE_DYNAMIC_BUILD
        if (!try_dynamic_voxel_allocate_atomic(tree_nodes, tree_leaves, svo_info, trace_info, di.shader_debug_frame_slot)) {
#else
        if (!try_voxel_allocate(tree_nodes, tree_leaves, svo_info, trace_info, di.shader_debug_frame_slot)) {
#endif
            // If allocation failed, push this to the deferred list.
            uint new_task_index;
            InterlockedAdd(deferred_list_write[SVO_DEFERRED_LIST_COUNT_INDEX], 1, new_task_index);
            WBE_SHADER_DEBUG_ASSERT(new_task_index < svo_info[0].max_deferred_task_count,
                ShaderDebugError::SVO_DEFERRED_TASK_INDEX_OVERFLOW, di.shader_debug_frame_slot);
            deferred_list_write[SVO_DEFERRED_LIST_ENTRY_OFFSET + new_task_index] = task;
        }
    }
}
