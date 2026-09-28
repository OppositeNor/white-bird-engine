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

struct DrawIndexedIndirectCommand {
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

struct OctTreePreDeferredDebugCaptureDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_info")]
    uint2 oct_tree_info;
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_deferred_list_ping")]
    uint2 deferred_list;
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_pre_deferred_debug_info_buffer")]
    uint2 captured_oct_tree_info;
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_pre_deferred_debug_list_buffer")]
    uint2 captured_deferred_list;
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_pre_deferred_debug_draw_indirect_buffer")]
    uint2 indirect_draw;
};

[[vk::push_constant]]
ConstantBuffer<OctTreePreDeferredDebugCaptureDescriptorIndices> descriptor_indices;

/**
 * @brief Captures the oct-tree state produced by the raster passes before deferred propagation changes it.
 */
[shader("compute")]
[numthreads(128, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    StructuredBuffer<OctTreeInfo> oct_tree_info = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
    StructuredBuffer<OctTreeDeferredListEntry> deferred_list = ResourceDescriptorHeap[descriptor_indices.deferred_list.x];
    RWStructuredBuffer<OctTreeInfo> captured_oct_tree_info = ResourceDescriptorHeap[descriptor_indices.captured_oct_tree_info.x];
    RWStructuredBuffer<OctTreeDeferredListEntry> captured_deferred_list =
        ResourceDescriptorHeap[descriptor_indices.captured_deferred_list.x];
    RWStructuredBuffer<DrawIndexedIndirectCommand> indirect_draw = ResourceDescriptorHeap[descriptor_indices.indirect_draw.x];

    OctTreeInfo info = oct_tree_info[0];
    uint deferred_task_count = info.deferred_task_count_ping;
    if (p_dispatch_thread_id.x == 0) {
        captured_oct_tree_info[0] = info;
        DrawIndexedIndirectCommand command;
        command.index_count = 36;
        command.instance_count = info.leaves_count + info.dynamic_leaves_count + deferred_task_count;
        command.first_index = 0;
        command.vertex_offset = 0;
        command.first_instance = 0;
        indirect_draw[0] = command;
    }
    if (p_dispatch_thread_id.x < deferred_task_count) {
        captured_deferred_list[p_dispatch_thread_id.x] = deferred_list[p_dispatch_thread_id.x];
    }
}
